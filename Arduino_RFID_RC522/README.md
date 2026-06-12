# Arduino RFID-RC522 — Read/Write Tag

Arduino sketch to **read and write data** on MIFARE Classic 1K tags/cards using the **RC522** RFID module.

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

## Notes

- Data is stored in **block 4** (first block of sector 1). Sector 0 holds manufacturer data and must not be overwritten.
- Authentication uses the factory default key `FF FF FF FF FF FF` (Key A), which works on new/blank tags.
- Each MIFARE Classic block holds **16 bytes**, so messages are limited to 16 characters.
