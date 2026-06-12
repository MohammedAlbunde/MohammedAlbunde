/*
 * RFID-RC522 Read/Write Tag Example
 * ---------------------------------
 * Reads and writes data on MIFARE Classic 1K tags using the MFRC522 module.
 *
 * Library required: "MFRC522" by GithubCommunity (install via Library Manager)
 *
 * Wiring (Arduino Uno/Nano):
 *   RC522    Arduino
 *   ------   -------
 *   SDA(SS)  D10
 *   SCK      D13
 *   MOSI     D11
 *   MISO     D12
 *   RST      D9
 *   GND      GND
 *   3.3V     3.3V   (IMPORTANT: do NOT use 5V!)
 *
 * Usage:
 *   Open Serial Monitor at 9600 baud.
 *   Send 'r' to switch to READ mode, 'w' to switch to WRITE mode.
 *   In WRITE mode, type the text to store (max 16 chars), then present a tag.
 *   In READ mode, present a tag to read its UID and stored data.
 */

#include <SPI.h>
#include <MFRC522.h>

#define SS_PIN  10
#define RST_PIN 9

// Block 4 = first block of sector 1 (sector 0 holds manufacturer data)
#define DATA_BLOCK 4

MFRC522 mfrc522(SS_PIN, RST_PIN);
MFRC522::MIFARE_Key key;

enum Mode { MODE_READ, MODE_WRITE };
Mode mode = MODE_READ;

byte writeBuffer[16];

void setup() {
  Serial.begin(9600);
  while (!Serial);          // wait for Serial (needed on Leonardo/Micro)

  SPI.begin();
  mfrc522.PCD_Init();

  // Default key for new/blank MIFARE Classic tags: FF FF FF FF FF FF
  for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF;

  Serial.println(F("RFID-RC522 Read/Write ready."));
  Serial.println(F("Send 'r' for READ mode, 'w' for WRITE mode."));
  Serial.println(F("Mode: READ - present a tag..."));
}

void loop() {
  handleSerialCommands();

  // Look for a new card
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
    return;
  }

  printUID();

  if (mode == MODE_READ) {
    readTag();
  } else {
    writeTag();
  }

  // Halt the tag and stop encryption so the next tag can be read
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
}

void handleSerialCommands() {
  if (!Serial.available()) return;

  char c = Serial.read();

  if (c == 'r' || c == 'R') {
    mode = MODE_READ;
    flushSerial();
    Serial.println(F("\nMode: READ - present a tag..."));
  } else if (c == 'w' || c == 'W') {
    mode = MODE_WRITE;
    flushSerial();
    Serial.println(F("\nMode: WRITE - type text (max 16 chars) and press Enter:"));

    // Wait for the text to write
    String text = readLine();
    memset(writeBuffer, 0, sizeof(writeBuffer));        // pad with zeros
    text.getBytes(writeBuffer, sizeof(writeBuffer) + 1);

    Serial.print(F("Will write: \""));
    Serial.print(text);
    Serial.println(F("\" - present a tag..."));
  }
}

void printUID() {
  Serial.print(F("\nTag UID:"));
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
    Serial.print(mfrc522.uid.uidByte[i], HEX);
  }
  Serial.println();

  MFRC522::PICC_Type type = mfrc522.PICC_GetType(mfrc522.uid.sak);
  Serial.print(F("Tag type: "));
  Serial.println(mfrc522.PICC_GetTypeName(type));
}

void readTag() {
  if (!authenticate()) return;

  byte buffer[18];
  byte size = sizeof(buffer);

  MFRC522::StatusCode status =
      mfrc522.MIFARE_Read(DATA_BLOCK, buffer, &size);

  if (status != MFRC522::STATUS_OK) {
    Serial.print(F("Read failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
    return;
  }

  Serial.print(F("Block "));
  Serial.print(DATA_BLOCK);
  Serial.print(F(" (hex):"));
  for (byte i = 0; i < 16; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
  Serial.println();

  Serial.print(F("As text: \""));
  for (byte i = 0; i < 16; i++) {
    if (buffer[i] >= 32 && buffer[i] <= 126) Serial.write(buffer[i]);
  }
  Serial.println(F("\""));
}

void writeTag() {
  if (!authenticate()) return;

  MFRC522::StatusCode status =
      mfrc522.MIFARE_Write(DATA_BLOCK, writeBuffer, 16);

  if (status != MFRC522::STATUS_OK) {
    Serial.print(F("Write failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
    return;
  }

  Serial.println(F("Write OK!"));
  mode = MODE_READ;
  Serial.println(F("Mode: READ - present a tag to verify..."));
}

bool authenticate() {
  MFRC522::StatusCode status = mfrc522.PCD_Authenticate(
      MFRC522::PICC_CMD_MF_AUTH_KEY_A, DATA_BLOCK, &key, &(mfrc522.uid));

  if (status != MFRC522::STATUS_OK) {
    Serial.print(F("Authentication failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
    return false;
  }
  return true;
}

String readLine() {
  String line = "";
  while (true) {
    if (Serial.available()) {
      char c = Serial.read();
      if (c == '\n' || c == '\r') {
        if (line.length() > 0) break;   // ignore leading CR/LF
      } else {
        line += c;
        if (line.length() >= 16) break; // block holds max 16 bytes
      }
    }
  }
  return line;
}

void flushSerial() {
  delay(10);
  while (Serial.available()) Serial.read();
}
