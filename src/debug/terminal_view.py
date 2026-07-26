#!/usr/bin/env python3

import sys
import tkinter as tk
from tkinter import font


class TerminalBuffer:
    def __init__(self):
        self.lines = [[]]
        self.col = 0

    def _write_character(self, character):
        line = self.lines[-1]
        if self.col < len(line):
            line[self.col] = character
        else:
            line.extend(" " for _ in range(self.col - len(line)))
            line.append(character)
        self.col += 1

    def _delete_previous_character(self):
        if self.col == 0:
            return
        self.col -= 1
        line = self.lines[-1]
        if self.col < len(line):
            del line[self.col]

    def feed(self, data):
        for byte in data:
            if byte == 10:
                self.lines.append([])
                self.col = 0
            elif byte == 13:
                self.col = 0
            elif byte in (8, 127):
                self._delete_previous_character()
            elif byte == 9:
                next_tab_stop = (self.col // 8 + 1) * 8
                while self.col < next_tab_stop:
                    self._write_character(" ")
            elif 32 <= byte <= 126:
                self._write_character(chr(byte))

    def text(self):
        return "\n".join("".join(line) for line in self.lines)


class TerminalView:
    POLL_INTERVAL_MS = 50

    def __init__(self, output_path, input_path):
        self.output_path = output_path
        self.input_file = open(input_path, "ab", buffering=0)
        self.offset = 0
        self.buffer = TerminalBuffer()

        self.root = tk.Tk()
        self.root.title("RETI terminal")
        self.root.geometry("900x550")

        terminal_font = font.Font(family="monospace", size=11)
        self.text = tk.Text(
            self.root,
            background="#111111",
            foreground="#eeeeee",
            insertbackground="#eeeeee",
            font=terminal_font,
            wrap="none",
            borderwidth=0,
            padx=8,
            pady=8,
        )
        vertical_scrollbar = tk.Scrollbar(
            self.root, orient="vertical", command=self.text.yview
        )
        horizontal_scrollbar = tk.Scrollbar(
            self.root, orient="horizontal", command=self.text.xview
        )
        self.text.configure(
            yscrollcommand=vertical_scrollbar.set,
            xscrollcommand=horizontal_scrollbar.set,
        )

        self.text.grid(row=0, column=0, sticky="nsew")
        vertical_scrollbar.grid(row=0, column=1, sticky="ns")
        horizontal_scrollbar.grid(row=1, column=0, sticky="ew")
        self.root.grid_rowconfigure(0, weight=1)
        self.root.grid_columnconfigure(0, weight=1)
        self.text.configure(state="disabled")
        self.root.bind("<KeyPress>", self._send_key)
        self.text.focus_set()

    def _send_key(self, event):
        special_keys = {
            "Return": b"\r",
            "BackSpace": b"\x7f",
            "Tab": b"\t",
            "Escape": b"\x1b",
        }
        data = special_keys.get(event.keysym)
        if data is None and event.char:
            try:
                data = event.char.encode("latin-1")
            except UnicodeEncodeError:
                data = None
        if data:
            self.input_file.write(data)
        return "break"

    def _read_new_output(self):
        try:
            with open(self.output_path, "rb") as output_file:
                output_file.seek(self.offset)
                data = output_file.read()
                self.offset = output_file.tell()
        except OSError:
            return b""
        return data

    def _poll(self):
        data = self._read_new_output()
        if data:
            self.buffer.feed(data)
            self.text.configure(state="normal")
            self.text.delete("1.0", "end")
            self.text.insert("1.0", self.buffer.text())
            self.text.configure(state="disabled")
            self.text.see("end")
        self.root.after(self.POLL_INTERVAL_MS, self._poll)

    def run(self):
        self._poll()
        self.root.mainloop()


def main():
    if len(sys.argv) != 3:
        raise SystemExit(f"Usage: {sys.argv[0]} OUTPUT_FILE INPUT_FILE")
    TerminalView(sys.argv[1], sys.argv[2]).run()


if __name__ == "__main__":
    main()
