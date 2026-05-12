#!/usr/bin/env python3

import json
import re
import struct
import sys
import tkinter as tk
from pathlib import Path
from tkinter import scrolledtext


POLL_INTERVAL_MS = 1000
STATE_STRUCT = struct.Struct("<II")
INCLUDE_RE = re.compile(r'^\s*#\s*include\s+[<"]([^>"]+)[>"]')


class SourceDebugApp:
    def __init__(self, root, debuginfo_path, state_path):
        self.root = root
        self.debuginfo_path = debuginfo_path
        self.state_path = state_path
        self.current_state = None
        self.current_file = None
        self.files = []
        self.file_cache = {}
        self.file_path_cache = {}
        self.status_var = tk.StringVar(
            value=f"debuginfo: {self.debuginfo_path}"
        )

        self.ranges = self.load_debuginfo()
        self.header = tk.Label(
            root, textvariable=self.status_var, anchor="w", justify="left"
        )
        self.header.pack(fill="x", padx=8, pady=(8, 4))

        self.text = scrolledtext.ScrolledText(
            root,
            wrap="none",
            font=("Courier", 11),
            state="disabled",
        )
        self.text.pack(fill="both", expand=True, padx=8, pady=(0, 8))
        self.text.tag_configure(
            "current_line", background="white", foreground="black"
        )

        if self.ranges:
            self.current_file = self.resolve_file_path(self.ranges[0]["file_id"])
            self.load_source_file(self.current_file)

        self.root.after(POLL_INTERVAL_MS, self.poll_state)

    def load_debuginfo(self):
        if not self.debuginfo_path.exists():
            self.status_var.set(f"debuginfo not found: {self.debuginfo_path}")
            return []

        try:
            with self.debuginfo_path.open("r", encoding="utf-8") as handle:
                debug_data = json.load(handle)
        except (OSError, json.JSONDecodeError) as exc:
            self.status_var.set(f"failed to read debuginfo: {exc}")
            return []

        self.files = debug_data["files"]
        return debug_data["ranges"]

    def resolve_file_path(self, file_id):
        if file_id not in self.file_path_cache:
            file_path = Path(self.files[file_id])
            self.file_path_cache[file_id] = self.resolve_relative_file_path(file_path)
        return self.file_path_cache[file_id]

    def resolve_relative_file_path(self, file_path):
        if file_path.is_absolute():
            return file_path

        debuginfo_dir = self.debuginfo_path.parent
        candidates = [
            debuginfo_dir / file_path,
            debuginfo_dir.parent / "lib" / file_path.stem / file_path.name,
        ]

        for candidate in candidates:
            if candidate.exists():
                return candidate.resolve()

        return candidates[0].resolve()

    def poll_state(self):
        try:
            raw_state = self.state_path.read_bytes()
        except OSError:
            self.root.after(POLL_INTERVAL_MS, self.poll_state)
            return

        if len(raw_state) >= STATE_STRUCT.size:
            state = STATE_STRUCT.unpack(raw_state[: STATE_STRUCT.size])
            if state != self.current_state:
                self.current_state = state
                self.refresh_view()

        self.root.after(POLL_INTERVAL_MS, self.poll_state)

    def refresh_view(self):
        if self.current_state is None:
            return

        pc, cs = self.current_state
        relative_pc = (pc - cs if pc >= cs else pc) + 1
        range_entry = self.lookup_range_entry(relative_pc)
        if range_entry is None:
            self.clear_highlight()
            self.status_var.set(
                f"PC={pc} CS={cs} relative_pc={relative_pc} | no source mapping"
            )
            return

        file_path = self.resolve_file_path(range_entry["file_id"])
        line_number = range_entry["line"]
        self.status_var.set(
            f"PC={pc} CS={cs} relative_pc={relative_pc} | "
            f"{file_path}:{line_number}"
        )

        if file_path != self.current_file:
            self.current_file = file_path
            self.load_source_file(file_path)

        self.highlight_line(line_number)

    def lookup_range_entry(self, relative_pc):
        for entry in self.ranges:
            if entry["start"] <= relative_pc <= entry["end"]:
                return entry
        return None

    def load_source_file(self, file_path):
        lines = self.load_expanded_source_lines(file_path, set())
        self.text.configure(state="normal")
        self.text.delete("1.0", tk.END)
        for idx, line in enumerate(lines, start=1):
            self.text.insert(tk.END, f"{idx:4d}: {line}")
            if not line.endswith("\n"):
                self.text.insert(tk.END, "\n")
        self.text.configure(state="disabled")

    def load_expanded_source_lines(self, file_path, include_stack):
        if file_path in self.file_cache:
            return self.file_cache[file_path]

        try:
            with file_path.open("r", encoding="utf-8") as handle:
                raw_lines = handle.readlines()
        except OSError as exc:
            self.file_cache[file_path] = [f"Failed to read {file_path}: {exc}\n"]
            return self.file_cache[file_path]

        expanded_lines = []
        next_include_stack = include_stack | {file_path}
        for line in raw_lines:
            include_match = INCLUDE_RE.match(line)
            if include_match is None:
                expanded_lines.append(line)
                continue

            include_path = self.resolve_include_path(
                include_match.group(1), file_path
            )
            if include_path is None or include_path in include_stack:
                expanded_lines.append(line)
                continue

            expanded_lines.extend(
                self.load_expanded_source_lines(include_path, next_include_stack)
            )

        self.file_cache[file_path] = expanded_lines
        return expanded_lines

    def resolve_include_path(self, include_name, including_file):
        include_path = Path(include_name)
        if include_path.is_absolute():
            return include_path.resolve() if include_path.exists() else None

        debuginfo_dir = self.debuginfo_path.parent
        candidates = [
            including_file.parent / include_path,
            debuginfo_dir / include_path,
            debuginfo_dir.parent / "lib" / include_path.stem / include_path.name,
        ]

        for candidate in candidates:
            if candidate.exists():
                return candidate.resolve()

        return None

    def highlight_line(self, line_number):
        self.text.configure(state="normal")
        self.text.tag_remove("current_line", "1.0", tk.END)
        start = f"{line_number}.0"
        end = f"{line_number}.end"
        self.text.tag_add("current_line", start, end)
        self.text.see(start)
        self.text.configure(state="disabled")

    def clear_highlight(self):
        self.text.configure(state="normal")
        self.text.tag_remove("current_line", "1.0", tk.END)
        self.text.configure(state="disabled")


def main():
    if len(sys.argv) != 3:
        raise SystemExit(
            "usage: source_debug.py <debuginfo.json> <source_debug_state.bin>"
        )

    debuginfo_path = Path(sys.argv[1]).resolve()
    state_path = Path(sys.argv[2]).resolve()

    root = tk.Tk()
    root.title("Source Debug")
    root.geometry("1000x700")

    SourceDebugApp(root, debuginfo_path, state_path)
    root.mainloop()


if __name__ == "__main__":
    main()
