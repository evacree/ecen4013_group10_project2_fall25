#include <Wire.h>
#include <Adafruit_GPS.h>
#include <Adafruit_LSM6DSOX.h>
#include <Adafruit_LIS3MDL.h>

Adafruit_GPS GPS(&Wire);
Adafruit_LSM6DSOX sox;
Adafruit_LIS3MDL lis3mdl;

uint32_t lastPrint = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Initializing IMU and GPS over I2C...");

  // Initialize IMU sensors
  if (!sox.begin_I2C(0x6A)) {
    Serial.println("Failed to find LSM6DSOX!");
    while (1);
  }
  if (!lis3mdl.begin_I2C(0x1C)) {
    Serial.println("Failed to find LIS3MDL!");
    while (1);
  }
  Serial.println("IMU sensors initialized.");

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
  char c = GPS.read();
  if (GPS.newNMEAreceived()) {
    if (!GPS.parse(GPS.lastNMEA())) {
      return;
    }
  }

  // Print synchronized data every 1 second
  if (millis() - lastPrint >= 1000) {
    lastPrint = millis();

    Serial.println("====================================");
    Serial.print("Timestamp (ms): ");
    Serial.println(millis());

    if (GPS.fix) {
      Serial.print("Latitude: ");
      Serial.print(GPS.latitude, 6); Serial.print(GPS.lat);
      Serial.print(" | Longitude: ");
      Serial.print(GPS.longitude, 6); Serial.println(GPS.lon);

      Serial.print("Altitude (MSL): ");
      Serial.print(GPS.altitude); Serial.println(" m");

      Serial.print("Satellites: ");
      Serial.println((int)GPS.satellites);
    } else {
      Serial.println("GPS: No fix yet");
    }

    Serial.print("Accel (m/s^2): X=");
    Serial.print(accel.acceleration.x, 3);
    Serial.print(" Y=");
    Serial.print(accel.acceleration.y, 3);
    Serial.print(" Z=");
    Serial.println(accel.acceleration.z, 3);

    Serial.print("Gyro (rad/s): X=");
    Serial.print(gyro.gyro.x, 3);
    Serial.print(" Y=");
    Serial.print(gyro.gyro.y, 3);
    Serial.print(" Z=");
    Serial.println(gyro.gyro.z, 3);

    Serial.print("Mag (uT): X=");
    Serial.print(mag.magnetic.x, 3);
    Serial.print(" Y=");
    Serial.print(mag.magnetic.y, 3);
    Serial.print(" Z=");
    Serial.println(mag.magnetic.z, 3);

    Serial.print("Temperature (C): ");
    Serial.println(temp.temperature, 2);
  }
}
