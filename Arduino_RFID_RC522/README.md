# Arduino RFID-RC522 — Read/Write Tag

Arduino sketches to **read and write data** on MIFARE Classic 1K tags/cards using the **RC522** RFID module.

Two projects are included:

1. **`RFID_RC522_ReadWrite/`** — simple Serial Monitor read/write demo (16-char messages).
2. **`StudentTag_RC522/` + `student_tag_gui.py`** — a desktop **GUI (Python/Tkinter)** to store a full student record on a tag: name, date of birth, home address, two parent phone numbers, and an allergies **dropdown menu**.

## Hardware Required

| Item | Qty |
|------|-----|
| Arduino Uno / Nano | 1 |
| RFID-RC522 module | 1 |
| MIFARE Classic 1K tag or card | 1+ |
| Jumper wires | 7 |

## Wiring (Arduino Uno / Nano)

| RC522 Pin | Arduino Pin |
|-----------|-------------|
| SDA (SS)  | D10 |
| SCK       | D13 |
| MOSI      | D11 |
| MISO      | D12 |
| RST       | D9  |
| GND       | GND |
| 3.3V      | **3.3V** ⚠️ |

> ⚠️ **Power the RC522 from 3.3V only.** Connecting it to 5V can damage the module. (The SPI logic pins tolerate the Uno's 5V signals.)

For **Arduino Mega**: SCK → D52, MOSI → D51, MISO → D50, SS → D10 (or any pin, update `SS_PIN`).

## Library Installation

In the Arduino IDE: **Sketch → Include Library → Manage Libraries…** then search for **"MFRC522"** (by GithubCommunity) and install it.

## How to Use

1. Upload `RFID_RC522_ReadWrite/RFID_RC522_ReadWrite.ino` to your board.
2. Open the **Serial Monitor** at **9600 baud** (set line ending to *Newline*).
3. Commands:
   - Send **`r`** → READ mode: present a tag to see its UID, type, and the text stored in block 4.
   - Send **`w`** → WRITE mode: type the text to store (max 16 characters), press Enter, then present a tag.
4. After a successful write, the sketch switches back to READ mode so you can verify the data immediately.

### Example Serial Output

```
RFID-RC522 Read/Write ready.
Send 'r' for READ mode, 'w' for WRITE mode.
Mode: READ - present a tag...

Tag UID: A3 5F 21 D9
Tag type: MIFARE 1KB
Block 4 (hex): 48 65 6C 6C 6F 00 00 00 00 00 00 00 00 00 00 00
As text: "Hello"
```

## Student Tag GUI

A desktop app to register student info on RFID tags. The GUI runs on the PC and talks to the Arduino over USB serial — the Arduino acts as a bridge to the RC522 reader.

### Setup

1. Upload `StudentTag_RC522/StudentTag_RC522.ino` to the Arduino (same wiring as above).
2. On the PC, install Python 3 and pyserial:
   ```
   pip install pyserial
   ```
3. Run the GUI:
   ```
   python student_tag_gui.py
   ```

### Usage

1. Pick the Arduino's serial port from the dropdown and click **Connect**.
2. Fill in the form:
   - **Student name** (max 32 chars)
   - **Date of birth** (DD/MM/YYYY)
   - **Home address** (max 48 chars)
   - **Parent phone #1** and **#2** (max 16 chars each)
   - **Allergies** — select from the dropdown (None, Peanuts, Tree nuts, Dairy, Eggs, Gluten, Seafood, Soy, Bee stings, Penicillin, Other)
3. Click **Write to Tag**, then hold the tag on the reader when prompted (20 s window).
4. Click **Read from Tag** and present a tag to load its stored record back into the form.

### Tag memory layout

| Field | Blocks | Bytes |
|-------|--------|-------|
| Student name | 4, 5 | 32 |
| Date of birth | 6 | 16 |
| Home address | 8, 9, 10 | 48 |
| Parent phone #1 | 12 | 16 |
| Parent phone #2 | 13 | 16 |
| Allergies | 14 | 16 |

Sector trailer blocks (7, 11, 15) and the manufacturer sector (0) are never touched.

## Notes

- Authentication uses the factory default key `FF FF FF FF FF FF` (Key A), which works on new/blank tags.
- Each MIFARE Classic block holds **16 bytes**; the simple demo stores one block (16 chars), the student GUI spans 9 blocks (144 bytes).
- Student data is stored **unencrypted** on the tag — anyone with a reader can view it. For real deployments consider changing the sector keys and/or encrypting the payload.
