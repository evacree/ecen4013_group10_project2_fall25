// HC-05 Bluetooth Test
// Teensy 4.1: RX2 = pin 7, TX2 = pin 8

void setup() {
  Serial.begin(115200);         // USB serial
  Serial2.begin(9600);          // HC-06 default baud
  delay(500);

  Serial.println("HC-06 Test Started");
  Serial.println("Type in Serial Monitor and it should appear over Bluetooth.");
}

void loop() {
  // If PC USB sends something, forward to Bluetooth
  if (Serial.available()) {
    char c = Serial.read();
    Serial2.write(c);
  }

  // If Bluetooth sends something, forward to PC USB
  if (Serial2.available()) {
    char c = Serial2.read();
    Serial.write(c);
  }
}
