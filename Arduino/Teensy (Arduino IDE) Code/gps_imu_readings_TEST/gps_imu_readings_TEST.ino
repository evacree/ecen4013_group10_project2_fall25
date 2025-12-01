#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <Adafruit_GPS.h>
#include <Adafruit_LSM6DSOX.h>
#include <Adafruit_LIS3MDL.h>

// ---------- Sensors ----------
Adafruit_GPS GPS(&Wire);
Adafruit_LSM6DSOX sox;
Adafruit_LIS3MDL lis3mdl;

// ---------- SD ----------
const int chipSelect = BUILTIN_SDCARD;
File dataFile;

char logFilename[20];   // e.g., "LOG0001.CSV"

// ---------- Bluetooth (HC-05) ----------
#define BT_BAUD 9600
#define LED_PIN 13

// ---------- Timing ----------
unsigned long lastOutput = 0;  // 1 Hz output

// ---------- Create numbered log file ----------
void createNewLogFile() {
  int fileIndex = 1;

  while (fileIndex < 9999) {
    sprintf(logFilename, "LOG%04d.CSV", fileIndex);

    if (!SD.exists(logFilename)) {
      dataFile = SD.open(logFilename, FILE_WRITE);
      if (dataFile) {
        dataFile.println("Date,Time,Satellites,Latitude,Longitude,Altitude,"
                         "X Accel,Y Accel,Z Accel,"
                         "X Mag,Y Mag,Z Mag,"
                         "X Gyro,Y Gyro,Z Gyro,Temperature");
        dataFile.close();
      }
      return;
    }
    fileIndex++;
  }
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(BT_BAUD);
  pinMode(LED_PIN, OUTPUT);

  // Initialize IMUs
  if (!sox.begin_I2C()) while (1);
  if (!lis3mdl.begin_I2C()) while (1);

  // Initialize SD + numbered log file
  if (SD.begin(chipSelect)) {
    createNewLogFile();
  }

  // Initialize GPS
  Wire.begin();
  GPS.begin(0x10);

  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA); // RMC + GGA
  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ);    // 1 Hz GPS update rate
  GPS.sendCommand(PGCMD_ANTENNA);
}

void loop() {
  // --------- Read GPS continuously ---------
  GPS.read();
  if (GPS.newNMEAreceived()) GPS.parse(GPS.lastNMEA());

  // --------- Read IMU ---------
  sensors_event_t accel, gyro, temp;
  sensors_event_t mag;
  sox.getEvent(&accel, &gyro, &temp);
  lis3mdl.getEvent(&mag);

  // --------- Output data at 1 Hz ---------
  unsigned long now = millis();
  if (now - lastOutput >= 1000) {
    lastOutput = now;

    // LED: steady = lock, blink = no lock
    if (GPS.fix) digitalWrite(LED_PIN, HIGH);
    else digitalWrite(LED_PIN, !digitalRead(LED_PIN));

    // CSV fields
    String dateStr = GPS.fix ? String(GPS.year) + "/" + String(GPS.month) + "/" + String(GPS.day) : "NaN";
    String timeStr = GPS.fix ? String(GPS.hour) + ":" + String(GPS.minute) + ":" + String(GPS.seconds) : "NaN";
    String satStr  = GPS.fix ? String((int)GPS.satellites) : "0";
    String latStr  = GPS.fix ? String(GPS.latitude, 6) + GPS.lat : "NaN";
    String lonStr  = GPS.fix ? String(GPS.longitude, 6) + GPS.lon : "NaN";
    String altStr  = GPS.fix ? String(GPS.altitude, 2) : "NaN";

    String csvLine = dateStr + "," + timeStr + "," + satStr + "," + latStr + "," + lonStr + "," + altStr;
    csvLine += "," + String(accel.acceleration.x,3) + "," + String(accel.acceleration.y,3) + "," + String(accel.acceleration.z,3);
    csvLine += "," + String(mag.magnetic.x,3) + "," + String(mag.magnetic.y,3) + "," + String(mag.magnetic.z,3);
    csvLine += "," + String(gyro.gyro.x,3) + "," + String(gyro.gyro.y,3) + "," + String(gyro.gyro.z,3);
    csvLine += "," + String(temp.temperature,2);

    // USB + Bluetooth
    Serial.println(csvLine);
    Serial2.println(csvLine);

    // SD append
    if (SD.begin(chipSelect)) {
      dataFile = SD.open(logFilename, FILE_WRITE);
      if (dataFile) {
        dataFile.println(csvLine);
        dataFile.close();
      }
    }
  }
}
