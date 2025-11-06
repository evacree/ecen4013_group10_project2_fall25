import FreeSimpleGUI as sg
import serial
import serial.tools.list_ports
import threading
import time
import re

# ---------- Globals ----------
ser = None
running = False

# ---------- Serial Utility Functions ----------


def list_serial_ports():
    return [port.device for port in serial.tools.list_ports.comports()]


def parse_data(line):
    """Parse CSV output from Teensy with format:
    Date,Time,Satellites,Lat,Lon,Alt,Xacc,Yacc,Zacc,Xmag,Ymag,Zmag,Xgyro,Ygyro,Zgyro,Temp
    """
    data = {}
    parts = line.strip().split(',')
    if len(parts) >= 16:
        data['date'] = parts[0]
        data['time'] = parts[1]
        data['sat'] = parts[2]
        data['lat'] = parts[3]
        data['lon'] = parts[4]
        data['alt'] = parts[5]
        data['accel'] = parts[6:9]
        data['mag'] = parts[9:12]
        data['gyro'] = parts[12:15]
        data['temp'] = parts[15]
    return data


def read_serial_data(window):
    """Thread that reads serial data and sends parsed dicts to GUI"""
    global ser, running
    first_success = False
    try:
        while running and ser:
            if ser.in_waiting > 0:
                line = ser.readline().decode(errors='ignore').strip()
                if line:
                    data = parse_data(line)
                    if data:
                        if not first_success:
                            window.write_event_value('-SHOW-', True)
                            first_success = True
                        window.write_event_value('-DATA-', data)
            time.sleep(0.02)
    except Exception as e:
        window.write_event_value('-DISCONNECT-', str(e))
    finally:
        if ser and ser.is_open:
            ser.close()


# ---------- Live Data Column (Hidden Initially) ----------
live_column = [
    [sg.Text("GPS Fix Status:"), sg.Text(
        "No Fix", key="-GPSFIX-", size=(12, 1))],
    [sg.Text("Date:"), sg.Text("--", key="-DATE-", size=(10, 1)),
     sg.Text("Time:"), sg.Text("--", key="-TIME-", size=(10, 1))],
    [sg.Text("Latitude:"), sg.Text("--", key="-LAT-", size=(12, 1))],
    [sg.Text("Longitude:"), sg.Text("--", key="-LON-", size=(12, 1))],
    [sg.Text("Elevation (m):"), sg.Text("--", key="-ALT-", size=(12, 1))],
    [sg.Text("Satellites:"), sg.Text("--", key="-SAT-", size=(6, 1))],
    [sg.Text("Acceleration (m/s²):")],
    [sg.Text("X:"), sg.Text("--", key="-ACCX-"),
     sg.Text("Y:"), sg.Text("--", key="-ACCY-"),
     sg.Text("Z:"), sg.Text("--", key="-ACCZ-")],
    [sg.Text("Magnetic Field (uT):")],
    [sg.Text("X:"), sg.Text("--", key="-MAGX-"),
     sg.Text("Y:"), sg.Text("--", key="-MAGY-"),
     sg.Text("Z:"), sg.Text("--", key="-MAGZ-")],
    [sg.Text("Angular Velocity (rad/s):")],
    [sg.Text("X:"), sg.Text("--", key="-GYRX-"),
     sg.Text("Y:"), sg.Text("--", key="-GYRY-"),
     sg.Text("Z:"), sg.Text("--", key="-GYRZ-")],
    [sg.Text("Temperature (°C):"), sg.Text("--", key="-TEMP-", size=(8, 1))]
]

# ---------- GUI Layout ----------
button_row = [
    sg.Button("Start Display", size=(20, 2), key="-START-"),
    sg.Button("End Display", size=(20, 2), key="-END-", visible=False)
]

exit_button = sg.Button("Exit", size=(20, 2))

layout = [
    [sg.Text("Teensy 4.1 Live Monitor", font=("Arial", 20, "bold"),
             justification='center', expand_x=True)],
    [sg.Column([button_row], justification='center', expand_x=True)],
    [sg.Column([[exit_button]], justification='center', expand_x=True)],
    [sg.Column(live_column, key='-LIVECOL-', visible=False)]
]

window = sg.Window("Teensy Monitor", layout, finalize=True,
                   resizable=False, size=(650, 600))

running = False
serial_thread = None

# ---------- Main Event Loop ----------
while True:
    event, values = window.read(timeout=100)

    if event in (sg.WIN_CLOSED, "Exit"):
        running = False
        if ser and ser.is_open:
            ser.close()
        break

    elif event == "-START-":
        ports = list_serial_ports()
        print("Detected serial ports:", ports)
        success = False
        for port in ports:
            try:
                print("Trying port:", port)
                ser = serial.Serial(port, 115200, timeout=1)
                running = True
                serial_thread = threading.Thread(
                    target=read_serial_data, args=(window,), daemon=True)
                serial_thread.start()
                success = True
                break
            except Exception as e:
                print("Failed on port", port, e)
                continue
        if not success:
            sg.popup_error("No USB stream found. Returning to main menu.")
            continue
        # Don't hide the button until data arrives (handled in -SHOW-)

    elif event == '-SHOW-':
        window['-LIVECOL-'].update(visible=True)
        window["-END-"].update(visible=True)
        window["-START-"].update(visible=False)

    elif event == "-END-":
        running = False
        if ser and ser.is_open:
            ser.close()
        window['-LIVECOL-'].update(visible=False)
        window["-END-"].update(visible=False)
        window["-START-"].update(visible=True)
        # Reset values
        for key in ["-GPSFIX-", "-DATE-", "-TIME-", "-LAT-", "-LON-", "-ALT-", "-SAT-",
                    "-ACCX-", "-ACCY-", "-ACCZ-", "-MAGX-", "-MAGY-", "-MAGZ-",
                    "-GYRX-", "-GYRY-", "-GYRZ-", "-TEMP-"]:
            window[key].update("No Fix" if key == "-GPSFIX-" else "--")

    elif event == '-DATA-':
        data = values['-DATA-']
        # Update GPS Fix Status
        if 'sat' in data and data['sat'] != '0' and data['sat'].lower() != 'nan':
            window["-GPSFIX-"].update("Fix Acquired")
        else:
            window["-GPSFIX-"].update("No Fix")
        # Update all fields
        for key in ['date', 'time', 'lat', 'lon', 'alt', 'sat']:
            if key in data:
                window_key = "-" + key.upper() + "-" if key != 'sat' else "-SAT-"
                window[window_key].update(data[key])
        if 'accel' in data:
            window["-ACCX-"].update(data['accel'][0])
            window["-ACCY-"].update(data['accel'][1])
            window["-ACCZ-"].update(data['accel'][2])
        if 'mag' in data:
            window["-MAGX-"].update(data['mag'][0])
            window["-MAGY-"].update(data['mag'][1])
            window["-MAGZ-"].update(data['mag'][2])
        if 'gyro' in data:
            window["-GYRX-"].update(data['gyro'][0])
            window["-GYRY-"].update(data['gyro'][1])
            window["-GYRZ-"].update(data['gyro'][2])
        if 'temp' in data:
            window["-TEMP-"].update(data['temp'])

    elif event == '-DISCONNECT-':
        sg.popup_error(
            "Device disconnected or serial error occurred.\nReturning to main menu.")
        running = False
        if ser and ser.is_open:
            ser.close()
        window['-LIVECOL-'].update(visible=False)
        window["-END-"].update(visible=False)
        window["-START-"].update(visible=True)
        # Reset values
        for key in ["-GPSFIX-", "-DATE-", "-TIME-", "-LAT-", "-LON-", "-ALT-", "-SAT-",
                    "-ACCX-", "-ACCY-", "-ACCZ-", "-MAGX-", "-MAGY-", "-MAGZ-",
                    "-GYRX-", "-GYRY-", "-GYRZ-", "-TEMP-"]:
            window[key].update("No Fix" if key == "-GPSFIX-" else "--")

window.close()
