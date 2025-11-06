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

// ---------- Timing ----------
uint32_t lastPrint = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Initializing IMU, GPS, and SD card...");

  // Initialize IMU sensors
  if (!sox.begin_I2C()) {
    Serial.println("Failed to find LSM6DSOX!");
    while (1);
  }
  if (!lis3mdl.begin_I2C()) {
    Serial.println("Failed to find LIS3MDL!");
    while (1);
  }

  // Initialize SD card
  if (!SD.begin(chipSelect)) {
    Serial.println("SD initialization failed!");
  } else {
    Serial.println("SD initialized.");
    dataFile = SD.open("log.csv", FILE_WRITE);
    if (dataFile) {
      // Write CSV header
      dataFile.println("Date,Time,Satellites,Latitude,Longitude,Elevation,X Accel,Y Accel,Z Accel,X Mag,Y Mag,Z Mag,X Gyro,Y Gyro,Z Gyro,Temperature");
      dataFile.close();
    }
  }

  // Initialize GPS safely
  Wire.begin();
  delay(200);

  Serial.println("Attempting to initialize GPS...");
  bool gps_ok = false;
  for (int attempt = 0; attempt < 5; attempt++) {
    if (GPS.begin(0x10)) {
      gps_ok = true;
      break;
    }
    Serial.println("GPS init failed, retrying...");
    delay(500);
  }

  if (!gps_ok) {
    Serial.println("GPS not responding over I2C. Continuing without GPS.");
  } else {
    Serial.println("GPS initialized.");
    GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);
    GPS.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ);
    GPS.sendCommand(PGCMD_ANTENNA);
    delay(1000);
    GPS.println(PMTK_Q_RELEASE);
  }

  Serial.println("Setup complete.");
}

void loop() {
  // Read IMU
  sensors_event_t accel, gyro, temp;
  sensors_event_t mag;
  sox.getEvent(&accel, &gyro, &temp);
  lis3mdl.getEvent(&mag);

  // Read GPS continuously (non-blocking)
  GPS.read();
  if (GPS.newNMEAreceived()) {
    GPS.parse(GPS.lastNMEA());
  }

  // Print and log every 1 second
  if (millis() - lastPrint >= 1000) {
    lastPrint = millis();

    String dateStr = "NaN";
    String timeStr = "NaN";
    String latStr = "NaN";
    String lonStr = "NaN";
    String altStr = "NaN";
    String satStr = "0";

    if (GPS.fix) {
      dateStr = String(GPS.year) + "/" + String(GPS.month) + "/" + String(GPS.day);
      timeStr = String(GPS.hour) + ":" + String(GPS.minute) + ":" + String(GPS.seconds);
      latStr = String(GPS.latitude, 6) + GPS.lat;
      lonStr = String(GPS.longitude, 6) + GPS.lon;
      altStr = String(GPS.altitude);
      satStr = String((int)GPS.satellites);
    }

    // Prepare CSV line
    String csvLine = dateStr + "," + timeStr + "," + satStr + "," + latStr + "," + lonStr + "," + altStr;
    csvLine += "," + String(accel.acceleration.x, 3) + "," + String(accel.acceleration.y, 3) + "," + String(accel.acceleration.z, 3);
    csvLine += "," + String(mag.magnetic.x, 3) + "," + String(mag.magnetic.y, 3) + "," + String(mag.magnetic.z, 3);
    csvLine += "," + String(gyro.gyro.x, 3) + "," + String(gyro.gyro.y, 3) + "," + String(gyro.gyro.z, 3);
    csvLine += "," + String(temp.temperature, 2);

    // Print to Serial
    Serial.println(csvLine);

    // Write to SD
    if (SD.begin(chipSelect)) {
      dataFile = SD.open("log.csv", FILE_WRITE);
      if (dataFile) {
        dataFile.println(csvLine);
        dataFile.close();
      }
    }
  }
}
