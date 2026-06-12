/*
 * Student Tag - RFID-RC522 Serial Bridge
 * --------------------------------------
 * Companion sketch for student_tag_gui.py (Python Tkinter GUI).
 *
 * The PC GUI sends student data (name, DoB, address, parent phones,
 * allergies) over USB serial and this sketch stores it on a MIFARE
 * Classic 1K tag across 9 data blocks (144 bytes total).
 *
 * Library required: "MFRC522" by GithubCommunity (Library Manager)
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
 * Serial protocol (115200 baud, newline-terminated lines):
 *   PC -> "W"                 start a write operation
 *   PC -> 32 hex chars x 9    one line per block, Arduino answers "NEXT"
 *                             after each line
 *   Arduino -> "WAIT_TAG"     present a tag now (20 s timeout)
 *   Arduino -> "OK" / "ERR:<reason>"
 *
 *   PC -> "R"                 start a read operation
 *   Arduino -> "WAIT_TAG"
 *   Arduino -> "DATA", then 9 lines of 32 hex chars, then "OK"
 *              (or "ERR:<reason>")
 */

#include <SPI.h>
#include <MFRC522.h>

#define SS_PIN  10
#define RST_PIN 9

// 9 data blocks in sectors 1-3, skipping the sector trailer blocks
// (7, 11, 15) which hold the access keys.
// Layout used by the GUI:
//   blocks 4,5    -> student name      (32 bytes)
//   block  6      -> date of birth     (16 bytes)
//   blocks 8,9,10 -> home address      (48 bytes)
//   block  12     -> parent phone #1   (16 bytes)
//   block  13     -> parent phone #2   (16 bytes)
//   block  14     -> allergies         (16 bytes)
const byte DATA_BLOCKS[] = {4, 5, 6, 8, 9, 10, 12, 13, 14};
const byte NUM_BLOCKS = sizeof(DATA_BLOCKS);

const unsigned long TAG_TIMEOUT_MS = 20000;

MFRC522 mfrc522(SS_PIN, RST_PIN);
MFRC522::MIFARE_Key key;

byte payload[sizeof(DATA_BLOCKS)][16];

void setup() {
  Serial.begin(115200);
  while (!Serial);

  SPI.begin();
  mfrc522.PCD_Init();

  // Factory default key for blank MIFARE Classic tags
  for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF;

  Serial.println(F("READY"));
}

void loop() {
  if (!Serial.available()) return;

  char cmd = Serial.read();
  drainLine();

  if (cmd == 'W' || cmd == 'w') {
    handleWrite();
  } else if (cmd == 'R' || cmd == 'r') {
    handleRead();
  }
}

void handleWrite() {
  // Receive 9 blocks of data as hex lines, acknowledging each one so
  // the PC never overruns the 64-byte serial buffer.
  for (byte b = 0; b < NUM_BLOCKS; b++) {
    if (!readHexLine(payload[b])) {
      Serial.println(F("ERR:BAD_HEX"));
      return;
    }
    Serial.println(F("NEXT"));
  }

  Serial.println(F("WAIT_TAG"));
  if (!waitForTag()) {
    Serial.println(F("ERR:TIMEOUT"));
    return;
  }

  for (byte b = 0; b < NUM_BLOCKS; b++) {
    byte block = DATA_BLOCKS[b];

    MFRC522::StatusCode status = mfrc522.PCD_Authenticate(
        MFRC522::PICC_CMD_MF_AUTH_KEY_A, block, &key, &(mfrc522.uid));
    if (status != MFRC522::STATUS_OK) {
      fail(F("ERR:AUTH"));
      return;
    }

    status = mfrc522.MIFARE_Write(block, payload[b], 16);
    if (status != MFRC522::STATUS_OK) {
      fail(F("ERR:WRITE"));
      return;
    }
  }

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
  Serial.println(F("OK"));
}

void handleRead() {
  Serial.println(F("WAIT_TAG"));
  if (!waitForTag()) {
    Serial.println(F("ERR:TIMEOUT"));
    return;
  }

  byte buffer[18];

  Serial.println(F("DATA"));
  for (byte b = 0; b < NUM_BLOCKS; b++) {
    byte block = DATA_BLOCKS[b];

    MFRC522::StatusCode status = mfrc522.PCD_Authenticate(
        MFRC522::PICC_CMD_MF_AUTH_KEY_A, block, &key, &(mfrc522.uid));
    if (status != MFRC522::STATUS_OK) {
      fail(F("ERR:AUTH"));
      return;
    }

    byte size = sizeof(buffer);
    status = mfrc522.MIFARE_Read(block, buffer, &size);
    if (status != MFRC522::STATUS_OK) {
      fail(F("ERR:READ"));
      return;
    }

    for (byte i = 0; i < 16; i++) {
      if (buffer[i] < 0x10) Serial.print('0');
      Serial.print(buffer[i], HEX);
    }
    Serial.println();
  }

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
  Serial.println(F("OK"));
}

bool waitForTag() {
  unsigned long start = millis();
  while (millis() - start < TAG_TIMEOUT_MS) {
    if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
      return true;
    }
  }
  return false;
}

// Reads one newline-terminated line of exactly 32 hex chars into out[16].
bool readHexLine(byte *out) {
  char line[33];
  byte len = 0;
  unsigned long start = millis();

  while (millis() - start < 5000) {
    if (!Serial.available()) continue;
    char c = Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      if (len == 32) break;
      return false;
    }
    if (len >= 32) return false;
    line[len++] = c;
  }
  if (len != 32) return false;

  for (byte i = 0; i < 16; i++) {
    int hi = hexVal(line[i * 2]);
    int lo = hexVal(line[i * 2 + 1]);
    if (hi < 0 || lo < 0) return false;
    out[i] = (hi << 4) | lo;
  }
  return true;
}

int hexVal(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}

void fail(const __FlashStringHelper *msg) {
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
  Serial.println(msg);
}

void drainLine() {
  delay(5);
  while (Serial.available()) {
    char c = Serial.peek();
    if (c == '\n' || c == '\r') Serial.read();
    else break;
  }
}
