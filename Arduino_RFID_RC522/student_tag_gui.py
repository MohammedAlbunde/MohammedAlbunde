#!/usr/bin/env python3
"""
Student Tag GUI - RFID-RC522
----------------------------
Tkinter GUI to store student information on a MIFARE Classic 1K tag
through an Arduino running StudentTag_RC522.ino.

Fields:
  - Student name        (max 32 chars -> blocks 4,5)
  - Date of birth       (max 16 chars -> block 6)
  - Home address        (max 48 chars -> blocks 8,9,10)
  - Parent phone #1     (max 16 chars -> block 12)
  - Parent phone #2     (max 16 chars -> block 13)
  - Allergies dropdown  (max 16 chars -> block 14)

Requires:  pip install pyserial
Run:       python student_tag_gui.py
"""

import threading
import time
import tkinter as tk
from tkinter import ttk, messagebox

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    raise SystemExit("pyserial is required:  pip install pyserial")

BAUD = 115200
TAG_WAIT_S = 25  # a bit longer than the Arduino's 20 s tag timeout

ALLERGY_OPTIONS = [
    "None",
    "Peanuts",
    "Tree nuts",
    "Dairy / Milk",
    "Eggs",
    "Gluten / Wheat",
    "Seafood / Fish",
    "Soy",
    "Bee stings",
    "Penicillin",
    "Other",
]

# (label, max length in bytes) - order matches the 9 tag blocks
FIELDS = [
    ("Student name", 32),
    ("Date of birth", 16),
    ("Home address", 48),
    ("Parent phone #1", 16),
    ("Parent phone #2", 16),
    ("Allergies", 16),
]


def fields_to_blocks(values):
    """Pack the 6 field strings into 9 x 16-byte blocks (zero padded)."""
    data = b""
    for (_, max_len), value in zip(FIELDS, values):
        raw = value.encode("ascii", errors="replace")[:max_len]
        data += raw.ljust(max_len, b"\x00")
    assert len(data) == 144
    return [data[i:i + 16] for i in range(0, 144, 16)]


def blocks_to_fields(blocks):
    """Unpack 9 x 16-byte blocks back into the 6 field strings."""
    data = b"".join(blocks)
    values, pos = [], 0
    for _, max_len in FIELDS:
        chunk = data[pos:pos + max_len]
        pos += max_len
        values.append(chunk.rstrip(b"\x00").decode("ascii", errors="replace"))
    return values


class StudentTagApp:
    def __init__(self, root):
        self.root = root
        self.ser = None
        root.title("Student RFID Tag - RC522")
        root.resizable(False, False)

        pad = {"padx": 8, "pady": 4}

        # --- Serial port row -------------------------------------------
        port_frame = ttk.LabelFrame(root, text="Arduino connection")
        port_frame.grid(row=0, column=0, columnspan=2, sticky="ew", **pad)

        self.port_var = tk.StringVar()
        self.port_box = ttk.Combobox(port_frame, textvariable=self.port_var,
                                     state="readonly", width=28)
        self.port_box.grid(row=0, column=0, **pad)

        ttk.Button(port_frame, text="Refresh",
                   command=self.refresh_ports).grid(row=0, column=1, **pad)
        self.connect_btn = ttk.Button(port_frame, text="Connect",
                                      command=self.toggle_connection)
        self.connect_btn.grid(row=0, column=2, **pad)

        # --- Student fields --------------------------------------------
        form = ttk.LabelFrame(root, text="Student information")
        form.grid(row=1, column=0, columnspan=2, sticky="ew", **pad)

        self.entries = []
        for i, (label, max_len) in enumerate(FIELDS):
            ttk.Label(form, text=f"{label}:").grid(
                row=i, column=0, sticky="e", **pad)

            if label == "Allergies":
                var = tk.StringVar(value=ALLERGY_OPTIONS[0])
                widget = ttk.Combobox(form, textvariable=var,
                                      values=ALLERGY_OPTIONS,
                                      state="readonly", width=38)
            else:
                var = tk.StringVar()
                vcmd = (root.register(
                    lambda text, n=max_len: len(text) <= n), "%P")
                widget = ttk.Entry(form, textvariable=var, width=40,
                                   validate="key", validatecommand=vcmd)
            widget.grid(row=i, column=1, sticky="w", **pad)
            self.entries.append(var)

            hint = "DD/MM/YYYY" if label == "Date of birth" else f"max {max_len}"
            ttk.Label(form, text=hint, foreground="grey").grid(
                row=i, column=2, sticky="w", **pad)

        # --- Action buttons --------------------------------------------
        self.write_btn = ttk.Button(root, text="Write to Tag",
                                    command=self.write_tag, state="disabled")
        self.write_btn.grid(row=2, column=0, sticky="ew", **pad)

        self.read_btn = ttk.Button(root, text="Read from Tag",
                                   command=self.read_tag, state="disabled")
        self.read_btn.grid(row=2, column=1, sticky="ew", **pad)

        # --- Status bar --------------------------------------------------
        self.status = tk.StringVar(value="Select the Arduino port and connect.")
        ttk.Label(root, textvariable=self.status, relief="sunken",
                  anchor="w").grid(row=3, column=0, columnspan=2,
                                   sticky="ew", padx=8, pady=(4, 8))

        self.refresh_ports()

    # --- Serial handling ---------------------------------------------------

    def refresh_ports(self):
        ports = [p.device for p in serial.tools.list_ports.comports()]
        self.port_box["values"] = ports
        if ports and not self.port_var.get():
            self.port_var.set(ports[0])

    def toggle_connection(self):
        if self.ser:
            self.ser.close()
            self.ser = None
            self.connect_btn.config(text="Connect")
            self.set_busy(True)
            self.status.set("Disconnected.")
            return

        port = self.port_var.get()
        if not port:
            messagebox.showwarning("No port", "Select a serial port first.")
            return

        try:
            self.ser = serial.Serial(port, BAUD, timeout=1)
        except serial.SerialException as e:
            messagebox.showerror("Connection failed", str(e))
            return

        self.status.set("Connecting (Arduino is resetting)...")
        self.connect_btn.config(state="disabled")
        threading.Thread(target=self._wait_ready, daemon=True).start()

    def _wait_ready(self):
        # Opening the port resets the Arduino; wait for its READY banner.
        for _ in range(8):
            line = self.ser.readline().decode(errors="ignore").strip()
            if line == "READY":
                self.root.after(0, self._on_connected)
                return
        self.root.after(0, self._on_connected)  # proceed anyway

    def _on_connected(self):
        self.connect_btn.config(text="Disconnect", state="normal")
        self.set_busy(False)
        self.status.set(f"Connected to {self.ser.port}.")

    def set_busy(self, busy):
        state = "disabled" if busy else "normal"
        self.write_btn.config(state=state)
        self.read_btn.config(state=state)

    def _read_line(self, timeout_s):
        """Read one non-empty line from serial within timeout_s seconds."""
        deadline = time.time() + timeout_s
        old = self.ser.timeout
        self.ser.timeout = 1
        try:
            while time.time() < deadline:
                line = self.ser.readline().decode(errors="ignore").strip()
                if line:
                    return line
            return ""
        finally:
            self.ser.timeout = old

    # --- Write ---------------------------------------------------------

    def write_tag(self):
        values = [v.get().strip() for v in self.entries]
        if not values[0]:
            messagebox.showwarning("Missing data", "Student name is required.")
            return

        blocks = fields_to_blocks(values)
        self.set_busy(True)
        self.status.set("Sending data to Arduino...")
        threading.Thread(target=self._write_worker, args=(blocks,),
                         daemon=True).start()

    def _write_worker(self, blocks):
        try:
            self.ser.reset_input_buffer()
            self.ser.write(b"W\n")

            for block in blocks:
                self.ser.write(block.hex().upper().encode() + b"\n")
                resp = self._read_line(5)
                if resp != "NEXT":
                    self._finish(f"Transfer failed ({resp or 'no reply'}).")
                    return

            if self._read_line(5) != "WAIT_TAG":
                self._finish("Arduino did not enter tag-wait mode.")
                return

            self.root.after(0, self.status.set,
                            "Present the tag to the reader now...")
            resp = self._read_line(TAG_WAIT_S)
            if resp == "OK":
                self._finish("Student data written to tag successfully!",
                             info=True)
            else:
                self._finish(f"Write failed ({resp or 'timeout'}).")
        except serial.SerialException as e:
            self._finish(f"Serial error: {e}")

    # --- Read ----------------------------------------------------------

    def read_tag(self):
        self.set_busy(True)
        self.status.set("Requesting read...")
        threading.Thread(target=self._read_worker, daemon=True).start()

    def _read_worker(self):
        try:
            self.ser.reset_input_buffer()
            self.ser.write(b"R\n")

            if self._read_line(5) != "WAIT_TAG":
                self._finish("Arduino did not enter tag-wait mode.")
                return

            self.root.after(0, self.status.set,
                            "Present the tag to the reader now...")
            resp = self._read_line(TAG_WAIT_S)
            if resp != "DATA":
                self._finish(f"Read failed ({resp or 'timeout'}).")
                return

            blocks = []
            for _ in range(9):
                line = self._read_line(5)
                if len(line) != 32:
                    self._finish(f"Bad data from Arduino ({line!r}).")
                    return
                blocks.append(bytes.fromhex(line))

            if self._read_line(5) != "OK":
                self._finish("Read did not complete cleanly.")
                return

            values = blocks_to_fields(blocks)
            self.root.after(0, self._populate, values)
            self._finish("Tag read successfully.", info=True)
        except (serial.SerialException, ValueError) as e:
            self._finish(f"Error: {e}")

    def _populate(self, values):
        for var, value, (label, _) in zip(self.entries, values, FIELDS):
            if label == "Allergies":
                var.set(value if value in ALLERGY_OPTIONS else
                        (value or ALLERGY_OPTIONS[0]))
            else:
                var.set(value)

    # --- Helpers ---------------------------------------------------------

    def _finish(self, message, info=False):
        def update():
            self.set_busy(False)
            self.status.set(message)
            if info:
                messagebox.showinfo("Done", message)
            else:
                messagebox.showerror("Problem", message)
        self.root.after(0, update)


def main():
    root = tk.Tk()
    StudentTagApp(root)
    root.mainloop()


if __name__ == "__main__":
    main()
