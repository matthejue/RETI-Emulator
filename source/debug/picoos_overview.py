#!/usr/bin/env python3

import json
import struct
import sys
import tkinter as tk
from pathlib import Path
from tkinter import messagebox, scrolledtext, ttk


POLL_INTERVAL_MS = 100
SRAM_MASK = 0x7FFFFFFF
NULL = 0


class SRAM:
    def __init__(self, path):
        self.path = path
        self.words = ()

    def reload(self):
        data = self.path.read_bytes()
        usable = len(data) - len(data) % 4
        self.words = struct.unpack(f">{usable // 4}I", data[:usable])

    @staticmethod
    def index(address):
        return int(address) & SRAM_MASK

    def valid(self, address):
        return address not in (None, NULL) and self.index(address) < len(self.words)

    def word(self, address, default=0):
        index = self.index(address)
        return self.words[index] if index < len(self.words) else default

    @staticmethod
    def signed(value):
        return value - (1 << 32) if value & (1 << 31) else value

    def string(self, address, limit=160):
        if not self.valid(address):
            return ""
        chars = []
        index = self.index(address)
        while index < len(self.words) and len(chars) < limit:
            value = self.words[index]
            if value == 0:
                break
            chars.append(chr(value) if 32 <= value <= 126 else "?")
            index += 1
        suffix = "…" if len(chars) == limit else ""
        return "".join(chars) + suffix


class PicoOSLayout:
    def __init__(self, debuginfo_path, layout_path, override_path):
        self.layout_path = layout_path
        self.override_path = override_path
        self.data = json.loads(layout_path.read_text(encoding="utf-8"))
        self.globals = self.data["globals"]
        self.structs = self.data["structs"]
        self.overrides = {}

        debug = json.loads(debuginfo_path.read_text(encoding="utf-8"))
        for symbol in debug.get("variables", []):
            if symbol.get("scope") not in ("global", "<global>", ""):
                continue
            entry = self.globals.get(symbol["name"])
            if entry is not None:
                entry["address"] = int(symbol["address"])
                entry["size"] = int(symbol.get("size", entry.get("size", 1)))

        if override_path.exists():
            override = json.loads(override_path.read_text(encoding="utf-8"))
            self.overrides = override.get("symbols", {})

    def symbol_address(self, name, sections):
        override = self.overrides.get(name)
        if override is not None:
            if isinstance(override, dict):
                address = parse_address(override["address"])
                section = override.get("section", "absolute")
                if section == "data":
                    return sections["datasegment_start"] + address
                return SRAM.index(address)
            return SRAM.index(parse_address(override))

        symbol = self.globals[name]
        address = int(symbol["address"])
        if symbol.get("section", "data") == "data":
            return sections["datasegment_start"] + address
        return address

    def field(self, struct_name, field_name):
        for field in self.structs[struct_name]["fields"]:
            if field["name"] == field_name:
                return field
        raise KeyError(f"{struct_name}.{field_name}")

    def field_word(self, memory, struct_name, address, field_name):
        field = self.field(struct_name, field_name)
        return memory.word(SRAM.index(address) + field["offset"])


def parse_address(value):
    return int(value, 0) if isinstance(value, str) else int(value)


def format_address(value):
    return f"0x{int(value) & 0xFFFFFFFF:08x} [SRAM {SRAM.index(value)}]"


def format_kib(cells):
    return f"{cells * 4 / 1024:.2f}"


def add_tree(parent, columns, widths):
    frame = ttk.Frame(parent)
    tree = ttk.Treeview(frame, columns=columns, show="headings")
    vertical = ttk.Scrollbar(frame, orient="vertical", command=tree.yview)
    horizontal = ttk.Scrollbar(frame, orient="horizontal", command=tree.xview)
    tree.configure(yscrollcommand=vertical.set, xscrollcommand=horizontal.set)
    tree.grid(row=0, column=0, sticky="nsew")
    vertical.grid(row=0, column=1, sticky="ns")
    horizontal.grid(row=1, column=0, sticky="ew")
    frame.rowconfigure(0, weight=1)
    frame.columnconfigure(0, weight=1)
    for column, width in zip(columns, widths):
        tree.heading(column, text=column)
        tree.column(column, width=width, minwidth=60, stretch=True)
    return frame, tree


class PicoOSOverviewApp:
    def __init__(self, root, debuginfo_path, layout_path, override_path,
                 state_path, sram_path, navigation_path):
        self.root = root
        self.layout = PicoOSLayout(debuginfo_path, layout_path, override_path)
        self.state_path = state_path
        self.memory = SRAM(sram_path)
        self.navigation_path = navigation_path
        self.current_generation = None
        self.state = None
        self.processes = []
        self.shared_entries = []
        self.navigation_targets = {}
        self.status = tk.StringVar(value="Waiting for the emulator…")

        ttk.Label(root, textvariable=self.status, anchor="w").pack(
            fill="x", padx=8, pady=(8, 4)
        )
        main = ttk.Panedwindow(root, orient="vertical")
        main.pack(fill="both", expand=True, padx=8, pady=(0, 8))

        upper = ttk.Panedwindow(main, orient="horizontal")
        main.add(upper, weight=3)
        memory_frame, self.memory_tree = add_tree(
            upper,
            ("Start", "End", "KiB", "Region", "Details"),
            (90, 90, 70, 150, 520),
        )
        upper.add(memory_frame, weight=3)

        notebook = ttk.Notebook(upper)
        upper.add(notebook, weight=4)
        process_frame, self.process_tree = add_tree(
            notebook,
            ("PID", "State", "Executable", "Memory", "KiB", "Heap", "Parent", "PCB metadata / links"),
            (50, 90, 180, 135, 60, 145, 60, 390),
        )
        notebook.add(process_frame, text="Processes / PCBs")
        shared_frame, self.shared_tree = add_tree(
            notebook,
            ("ID", "Name", "Memory", "KiB", "References", "Unlink", "Owners", "Entry / next"),
            (50, 170, 135, 60, 80, 60, 100, 240),
        )
        notebook.add(shared_frame, text="Shared memory")
        global_frame, self.global_tree = add_tree(
            notebook,
            ("Global", "Address", "Type", "Value"),
            (190, 155, 150, 300),
        )
        notebook.add(global_frame, text="Kernel globals")
        interrupt_frame, self.interrupt_tree = add_tree(
            notebook,
            ("Kind", "Number", "Handler / source", "Address / state", "Details"),
            (120, 70, 190, 180, 300),
        )
        notebook.add(interrupt_frame, text="Interrupts")

        self.navigation_trees = (
            self.memory_tree,
            self.process_tree,
            self.shared_tree,
            self.global_tree,
            self.interrupt_tree,
        )
        self.navigation_targets = {
            tree: {} for tree in self.navigation_trees
        }
        for tree in self.navigation_trees:
            tree.bind("<Double-1>", self.open_memory_target)

        activity_frame = ttk.LabelFrame(main, text="Kernel activity history")
        self.activity = scrolledtext.ScrolledText(
            activity_frame, wrap="word", height=12, font=("Courier", 10),
            state="disabled", background="white", foreground="#202020",
            insertbackground="#202020"
        )
        self.activity.pack(fill="both", expand=True, padx=5, pady=5)
        self.activity.tag_configure("syscall", foreground="#165b9e")
        self.activity.tag_configure("host_request", foreground="#7a3e9d")
        self.activity.tag_configure("interrupt", foreground="#9a4c00")
        main.add(activity_frame, weight=2)

        self.root.after(POLL_INTERVAL_MS, self.poll_state)

    def poll_state(self):
        try:
            state = json.loads(self.state_path.read_text(encoding="utf-8"))
            generation = state["generation"]
            if generation != self.current_generation:
                self.memory.reload()
                self.state = state
                self.current_generation = generation
                self.refresh_view()
        except (OSError, ValueError, KeyError, struct.error) as exc:
            self.status.set(f"Waiting for a complete debugger refresh: {exc}")
        self.root.after(POLL_INTERVAL_MS, self.poll_state)

    def symbol_value(self, name):
        address = self.layout.symbol_address(name, self.state["sections"])
        return self.memory.word(address)

    def read_struct(self, struct_name, address):
        result = {"_address": address, "_index": SRAM.index(address)}
        for field in self.layout.structs[struct_name]["fields"]:
            result[field["name"]] = self.memory.word(
                SRAM.index(address) + field["offset"]
            )
        return result

    def linked_structs(self, head, struct_name, next_field, limit=256):
        result = []
        seen = set()
        address = head
        while self.memory.valid(address) and SRAM.index(address) not in seen:
            if len(result) >= limit:
                break
            seen.add(SRAM.index(address))
            value = self.read_struct(struct_name, address)
            result.append(value)
            address = value[next_field]
        return result

    def load_processes(self):
        head = self.symbol_value("process_list_head")
        self.processes = self.linked_structs(head, "Process", "next")
        for process in self.processes:
            process["executable"] = self.memory.string(process["binary_path"])
            process["cwd"] = self.memory.string(process["working_directory"])
            process["base_index"] = SRAM.index(process["base_address"])

    def load_shared_memory(self):
        head = self.symbol_value("shared_memory_list_head")
        self.shared_entries = self.linked_structs(
            head, "SharedMemoryEntry", "next"
        )
        for entry in self.shared_entries:
            entry["display_name"] = (
                self.memory.string(entry["name"])
                if entry["name"] != NULL else "<unlinked>"
            )
            entry["base_index"] = SRAM.index(entry["address"])
            entry["owners"] = []

        entries_by_index = {
            entry["_index"]: entry for entry in self.shared_entries
        }
        for process in self.processes:
            attachments = self.linked_structs(
                process["shared_memory_attachments"],
                "SharedMemoryAttachment",
                "next",
            )
            for attachment in attachments:
                entry = entries_by_index.get(SRAM.index(attachment["entry"]))
                if entry is not None:
                    entry["owners"].append(str(self.memory.signed(process["pid"])))

    def heap_blocks(self, heap_symbol, start, end):
        first = self.symbol_value(heap_symbol)
        if not self.memory.valid(first):
            return []
        header_size = self.layout.structs["BlockHeader"]["size"]
        blocks = []
        seen = set()
        address = SRAM.index(first)
        while start <= address <= end and address not in seen and len(blocks) < 2048:
            seen.add(address)
            header = self.read_struct("BlockHeader", address)
            size = self.memory.signed(header["size"])
            if size < 0 or address + header_size + size - 1 > end:
                break
            blocks.append(
                {
                    "header": address,
                    "payload": address + header_size,
                    "size": size,
                    "total": header_size + size,
                    "free": header["free"] != 0,
                    "next": header["next"],
                }
            )
            if header["next"] == NULL:
                break
            address = SRAM.index(header["next"])
        return blocks

    def refresh_view(self):
        self.load_processes()
        self.load_shared_memory()
        self.render_processes()
        self.render_shared_memory()
        self.render_globals()
        self.render_interrupts()
        self.render_memory()
        self.render_activity()

        registers = self.state["registers"]
        mode = "paused" if self.state.get("paused") else "running"
        navigation = (
            "double-click memory entries to open them in the selected SRAM box"
            if self.state.get("paused") and self.state.get("active_sram_box")
            else "SRAM navigation unavailable"
        )
        override_note = (
            f" | overrides: {self.layout.override_path}"
            if self.layout.overrides else
            f" | optional overrides: {self.layout.override_path}"
        )
        self.status.set(
            f"PicoOS {mode} | PC={format_address(registers['PC'])} | "
            f"{len(self.processes)} processes | "
            f"{len(self.shared_entries)} shared-memory objects | "
            f"{navigation}{override_note}"
        )

    def clear_tree(self, tree):
        self.navigation_targets[tree].clear()
        tree.delete(*tree.get_children())

    def add_navigation_target(self, tree, item, start, end=None):
        start = SRAM.index(start)
        end = start if end is None else SRAM.index(end)
        if 0 <= start <= end < self.state["sram_size"]:
            self.navigation_targets[tree][item] = (start, end)

    def add_sized_navigation_target(self, tree, item, address, size):
        start = SRAM.index(address)
        size = max(1, int(size))
        end = min(start + size - 1, self.state["sram_size"] - 1)
        self.add_navigation_target(tree, item, start, end)

    def open_memory_target(self, event):
        tree = event.widget
        item = tree.identify_row(event.y)
        target = self.navigation_targets.get(tree, {}).get(item)
        if target is None:
            return

        self.request_memory_target(target)

    def request_memory_target(self, target):
        state = self.state
        try:
            state = json.loads(self.state_path.read_text(encoding="utf-8"))
        except (OSError, ValueError):
            pass

        paused = state is not None and state.get("paused", False)
        sram_selected = (
            state is not None and state.get("active_sram_box", False)
        )
        if not paused or not sram_selected:
            missing = []
            if not paused:
                missing.append("halt execution in stepping/debug mode")
            if not sram_selected:
                missing.append(
                    "select one of the three SRAM boxes with Tab or Shift+Tab"
                )
            messagebox.showerror(
                "Cannot open SRAM address",
                "To open this memory entry, " + " and ".join(missing) + ".",
                parent=self.root,
            )
            return

        start, end = target
        temporary_path = self.navigation_path.with_name(
            self.navigation_path.name + ".tmp"
        )
        try:
            temporary_path.write_text(
                f"{start} {end}\n", encoding="utf-8"
            )
            temporary_path.replace(self.navigation_path)
        except OSError as exc:
            messagebox.showerror(
                "Cannot open SRAM address",
                f"Failed to send the SRAM address to the emulator: {exc}",
                parent=self.root,
            )

    def render_processes(self):
        self.clear_tree(self.process_tree)
        active = SRAM.index(self.symbol_value("active_process"))
        states = self.layout.data.get("process_states", {})
        for process in self.processes:
            pid = self.memory.signed(process["pid"])
            state_number = self.memory.signed(process["state"])
            state = states.get(str(state_number), str(state_number))
            if process["_index"] == active:
                state = f"{state} (active)"
            heap_start = SRAM.index(process["heap_start"])
            heap_size = self.memory.signed(process["heap_size"])
            next_value = "NULL" if process["next"] == NULL else format_address(process["next"])
            descriptors = (
                "NULL" if process["file_descriptors"] == NULL
                else format_address(process["file_descriptors"])
            )
            attachments = (
                "NULL" if process["shared_memory_attachments"] == NULL
                else format_address(process["shared_memory_attachments"])
            )
            item = self.process_tree.insert(
                "", "end",
                values=(
                    pid,
                    state,
                    process["executable"] or "<unknown>",
                    format_address(process["base_address"]),
                    format_kib(self.memory.signed(process["size"])),
                    f"{heap_start} + {heap_size} cells",
                    self.memory.signed(process["parent_pid"]),
                    f"PCB {process['_index']} → {next_value}; "
                    f"cwd={process['cwd'] or '/'}; fds={descriptors}; "
                    f"shm={attachments}; exit={self.memory.signed(process['exit_status'])}",
                ),
            )
            self.add_sized_navigation_target(
                self.process_tree, item, process["base_address"],
                self.memory.signed(process["size"]),
            )

    def shared_region_sizes(self):
        sections = self.state["sections"]
        constants = self.layout.data.get("memory_constants", {})
        process_start = SRAM.index(
            constants.get("PROCESS_MEMORY_START", sections["stack_start"] + 1)
        )
        process_end = SRAM.index(
            constants.get("SRAM_MAX_ADDRESS_IN_MEMORY_MAP", self.state["sram_size"] - 1)
        )
        blocks = self.heap_blocks("process_memory_heap", process_start, process_end)
        return {block["payload"]: block["size"] for block in blocks}

    def render_shared_memory(self):
        self.clear_tree(self.shared_tree)
        sizes = self.shared_region_sizes()
        for entry in self.shared_entries:
            next_value = "NULL" if entry["next"] == NULL else format_address(entry["next"])
            size = sizes.get(entry["base_index"], 0)
            item = self.shared_tree.insert(
                "", "end",
                values=(
                    self.memory.signed(entry["id"]),
                    entry["display_name"],
                    format_address(entry["address"]),
                    format_kib(size),
                    self.memory.signed(entry["reference_count"]),
                    "yes" if entry["unlink_requested"] else "no",
                    ", ".join(entry["owners"]) or "—",
                    f"entry {entry['_index']} → {next_value}",
                ),
            )
            self.add_sized_navigation_target(
                self.shared_tree, item, entry["address"], size
            )

    def render_globals(self):
        self.clear_tree(self.global_tree)
        preferred = [
            "process_list_head", "process_list_tail", "active_process",
            "next_process_id", "foreground_process_id",
            "terminal_input_process_id", "shared_memory_list_head",
            "next_shared_memory_id", "kernel_heap", "process_memory_heap",
            "reschedule_requested", "interrupt_device_isrs",
            "interrupt_device_priorities",
        ]
        names = preferred + sorted(
            name for name in self.layout.globals
            if not name.startswith("__") and name not in preferred
        )
        for name in names:
            if name not in self.layout.globals:
                continue
            symbol = self.layout.globals[name]
            address = self.layout.symbol_address(name, self.state["sections"])
            value = self.memory.word(address)
            type_name = symbol.get("type", "")
            struct_name = (
                type_name.removeprefix("struct ")
                if type_name.startswith("struct ") and not type_name.endswith("*")
                else None
            )
            if struct_name in self.layout.structs:
                fields = []
                for field in self.layout.structs[struct_name]["fields"]:
                    field_value = self.memory.word(address + field["offset"])
                    if field["type"].endswith("*"):
                        formatted = (
                            "NULL" if field_value == NULL
                            else format_address(field_value)
                        )
                    else:
                        formatted = str(self.memory.signed(field_value))
                    fields.append(f"{field['name']}={formatted}")
                display = f"{struct_name}{{{', '.join(fields)}}}"
            elif type_name.endswith("*"):
                display = "NULL" if value == NULL else format_address(value)
                if type_name == "char *" and value != NULL:
                    display += f' "{self.memory.string(value)}"'
            elif symbol.get("size", 1) > 1:
                cells = [self.memory.word(address + offset) for offset in range(min(8, symbol["size"]))]
                display = ", ".join(str(self.memory.signed(cell)) for cell in cells)
                if symbol["size"] > len(cells):
                    display += ", …"
            else:
                display = f"{self.memory.signed(value)} (0x{value:08x})"
            item = self.global_tree.insert(
                "", "end", values=(name, format_address(address), type_name, display)
            )
            self.add_sized_navigation_target(
                self.global_tree, item, address, symbol.get("size", 1)
            )

    def render_interrupts(self):
        self.clear_tree(self.interrupt_tree)
        vector_names = self.layout.data["interrupt_vector"]
        labels = self.layout.data["labels"]
        for number, name in enumerate(vector_names):
            actual = self.memory.word(number)
            expected = labels.get(name)
            details = "matches build artifact" if SRAM.index(actual) == expected else f"expected SRAM {expected}"
            item = self.interrupt_tree.insert(
                "", "end",
                values=("vector", number, name, format_address(actual), details),
            )
            self.add_navigation_target(self.interrupt_tree, item, actual)

        for device in self.state["interrupts"].get("devices", []):
            number = device["isr"]
            handler = vector_names[number] if number < len(vector_names) else "disabled"
            item = self.interrupt_tree.insert(
                "", "end",
                values=("device", number, handler, device["name"], f"priority {device['priority']}"),
            )
            if number < len(vector_names):
                self.add_navigation_target(
                    self.interrupt_tree, item, self.memory.word(number)
                )

        for depth, handler in enumerate(self.state["interrupts"].get("active", [])):
            number = handler["isr"]
            if number < len(vector_names):
                name = vector_names[number]
            else:
                name = handler["source"]
            detail = ""
            if handler["source"] == "syscall":
                syscall = self.layout.data["syscalls"].get(str(handler["syscall_number"]), {})
                detail = syscall.get("name", f"syscall {handler['syscall_number']}")
            item = self.interrupt_tree.insert(
                "", "end",
                values=("active", depth, name, handler["source"], detail),
            )
            if number < len(vector_names):
                self.add_navigation_target(
                    self.interrupt_tree, item, self.memory.word(number)
                )

    def insert_memory_region(self, start, end, kind, details):
        if end < start:
            return
        item = self.memory_tree.insert(
            "", "end",
            values=(start, end, format_kib(end - start + 1), kind, details),
        )
        self.add_navigation_target(self.memory_tree, item, start, end)

    def describe_heap_block(self, block, kernel):
        if block["free"]:
            return "UNUSED", (
                f"free payload {block['size']} cells / {format_kib(block['size'])} KiB; "
                f"header at {block['header']}"
            )
        if kernel:
            process = next((item for item in self.processes if item["_index"] == block["payload"]), None)
            if process is not None:
                return "PCB", f"PID {self.memory.signed(process['pid'])}, {process['executable']}"
            entry = next((item for item in self.shared_entries if item["_index"] == block["payload"]), None)
            if entry is not None:
                return "SHM METADATA", f"id {self.memory.signed(entry['id'])}, {entry['display_name']}"
            return "KERNEL ALLOCATION", f"payload {block['size']} cells; header at {block['header']}"

        process = next((item for item in self.processes if item["base_index"] == block["payload"]), None)
        if process is not None:
            return "PROCESS", (
                f"PID {self.memory.signed(process['pid'])}, "
                f"{process['executable']}; payload {block['size']} cells"
            )
        entry = next((item for item in self.shared_entries if item["base_index"] == block["payload"]), None)
        if entry is not None:
            owners = ", ".join(entry["owners"]) or "none"
            return "SHARED MEMORY", (
                f"id {self.memory.signed(entry['id'])}, {entry['display_name']}, "
                f"owners {owners}; payload {block['size']} cells"
            )
        return "ALLOCATED", f"unmatched payload {block['size']} cells; header at {block['header']}"

    def insert_heap(self, heap_symbol, start, end, kernel):
        blocks = self.heap_blocks(heap_symbol, start, end)
        if not blocks:
            self.insert_memory_region(start, end, "UNUSED", f"{heap_symbol} is not initialized")
            return
        cursor = start
        for block in blocks:
            if block["header"] > cursor:
                self.insert_memory_region(cursor, block["header"] - 1, "UNUSED", "not linked by allocator")
            kind, details = self.describe_heap_block(block, kernel)
            block_end = block["header"] + block["total"] - 1
            self.insert_memory_region(block["header"], block_end, kind, details)
            cursor = block_end + 1
        if cursor <= end:
            self.insert_memory_region(cursor, end, "UNUSED", "remaining allocator area")

    def render_memory(self):
        self.clear_tree(self.memory_tree)
        sections = self.state["sections"]
        constants = self.layout.data.get("memory_constants", {})
        vector_end = sections["interrupt_service_routines_start"] - 1
        self.insert_memory_region(0, vector_end, "INTERRUPT VECTOR TABLE", "handler addresses")
        self.insert_memory_region(
            sections["codesegment_start"], sections["datasegment_start"] - 1,
            "KERNEL CODE / ISRs", "PicoOS executable code"
        )
        self.insert_memory_region(
            sections["datasegment_start"], sections["heap_start"] - 1,
            "KERNEL GLOBAL DATA", "symbols resolved from .debuginfo"
        )

        heap_start = sections["heap_start"]
        heap_end = heap_start + sections["heap_size"] - 1
        self.insert_heap("kernel_heap", heap_start, heap_end, True)
        self.insert_memory_region(
            heap_end + 1, sections["stack_start"], "KERNEL STACK",
            "grows down from stack_start"
        )

        process_start = SRAM.index(
            constants.get("PROCESS_MEMORY_START", sections["stack_start"] + 1)
        )
        process_end = SRAM.index(
            constants.get("SRAM_MAX_ADDRESS_IN_MEMORY_MAP", self.state["sram_size"] - 1)
        )
        self.insert_heap("process_memory_heap", process_start, process_end, False)

    def decode_struct_argument(self, syscall, event):
        struct_name = syscall.get("type")
        definition = self.layout.structs.get(struct_name)
        if definition is None:
            return format_address(event["argument"])
        words = event.get("argument_words", [])
        parts = []
        for field in definition["fields"]:
            if field["offset"] >= len(words):
                continue
            value = int(words[field["offset"]])
            if field["type"] == "char *":
                text = self.memory.string(value)
                parts.append(f'{field["name"]}={format_address(value)} "{text}"')
            elif field["type"].endswith("*"):
                parts.append(f"{field['name']}={format_address(value)}")
            else:
                parts.append(f"{field['name']}={self.memory.signed(value)}")
        return f"{struct_name}{{{', '.join(parts)}}} @ {format_address(event['argument'])}"

    def format_event(self, event):
        kind = event["kind"]
        prefix = f"#{int(event['sequence']):04d} PC={format_address(event['pc'])}"
        if kind == "syscall":
            syscall = self.layout.data["syscalls"].get(str(event["number"]), {})
            name = syscall.get("name", f"UNKNOWN_{event['number']}")
            argument_kind = syscall.get("kind", "value")
            if argument_kind == "none":
                return f"{prefix}  SYSCALL {name}()", "syscall"
            if argument_kind == "struct":
                argument = self.decode_struct_argument(syscall, event)
            elif argument_kind == "string":
                argument = f'{format_address(event["argument"])} "{self.memory.string(event["argument"])}"'
            else:
                argument = str(self.memory.signed(event["argument"]))
            return f"{prefix}  SYSCALL {name}({argument})", "syscall"
        if kind == "host_request":
            return f'{prefix}  HOST REQUEST  {event["text"]}', "host_request"
        if kind == "cpu_exception":
            causes = {1: "divide by zero", 2: "stack overflow", 3: "illegal instruction"}
            cause = causes.get(event["detail"], str(event["detail"]))
            return f"{prefix}  CPU EXCEPTION  {cause} → ISR {event['isr']}", "interrupt"
        if kind == "hardware_interrupt":
            detail = ""
            if event["source"] == "UART":
                byte = event["detail"] & 0xFF
                detail = f" byte={byte} ({chr(byte)!r})"
            elif event["source"] == "DMA":
                detail = f" status={event['detail']}"
            repeated = event.get("repeat_count", 1)
            count = f" ×{repeated}" if repeated > 1 else ""
            return f"{prefix}  {event['source']} INTERRUPT{count} → ISR {event['isr']}{detail}", "interrupt"
        if kind == "software_interrupt":
            return f"{prefix}  SOFTWARE INTERRUPT {event['isr']}", "interrupt"
        return f"{prefix}  {event['source']}  {event['text']}", "interrupt"

    def render_activity(self):
        self.activity.configure(state="normal")
        self.activity.delete("1.0", tk.END)
        for event in self.state.get("events", []):
            line, tag = self.format_event(event)
            self.activity.insert(tk.END, line + "\n", tag)
        self.activity.see(tk.END)
        self.activity.configure(state="disabled")


def main():
    if len(sys.argv) != 7:
        raise SystemExit(
            "usage: picoos_overview.py <kernel.debuginfo> "
            "<kernel.overview> <kernel.overview.override.json> "
            "<state.json> <sram.bin> <navigation.txt>"
        )

    paths = [Path(argument).resolve() for argument in sys.argv[1:]]
    root = tk.Tk()
    root.title("PicoOS Overview")
    root.geometry("1500x900")
    PicoOSOverviewApp(root, *paths)
    root.mainloop()


if __name__ == "__main__":
    main()
