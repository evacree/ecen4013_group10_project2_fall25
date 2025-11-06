#include <SD.h>

File dataFile;

void setup() {
  Serial.begin(115200);
  while (!Serial) {}
  
  if (!SD.begin(BUILTIN_SDCARD)) {
    Serial.println("SD failed or not present");
    return;
  }
  
  dataFile = SD.open("log.csv");
  if (dataFile) {
    Serial.println("Reading log.csv...");
    while (dataFile.available()) {
      Serial.write(dataFile.read());  // prints file contents to Serial
    }
    dataFile.close();
    Serial.println("\nDone reading CSV.");
  } else {
    Serial.println("Failed to open log.csv");
  }
}

void loop() {
  // nothing here
}
