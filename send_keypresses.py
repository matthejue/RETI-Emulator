#!/home/areo/.virtualenv/bin/python3
#!/usr/bin/env python3
"""
Launch a TUI in a visible terminal window and replay keypresses (xdotool-style) via Python.

Defaults:
  - Reads steps from ./opts/input.txt  (override with --input)
  - Opens alacritty by default        (override with --terminal)
  - Activates/focuses the terminal window, then types keys
  - After replay: prints a done message in THIS shell and waits for you to press 'q' (global)

Notes:
  - Normal text lines are typed with NO automatic Enter.
  - Blank line -> Enter
  - Special tokens: <ENTER>, <ENTER_N:n>, <CTRL-X>, <UP>/<DOWN>/<LEFT>/<RIGHT>, <SLEEP:x>, <PASTE:file>
  - <WAIT:...>/<EXPECT:...> are just tiny delays (no screen read in this GUI approach).
"""

import argparse
import os
import re
import shlex
import subprocess
import sys
import time
import uuid
from threading import Event, Thread

# ---- runtime deps ----
try:
    import pyautogui            # sends keys
except Exception:
    print("This script needs 'pyautogui'. Install: pip install pyautogui", file=sys.stderr)
    sys.exit(2)

# Try global key listener (for quitting on 'q')
try:
    from pynput import keyboard  # global key hooks
    PYNPUT_OK = True
except Exception:
    PYNPUT_OK = False

pyautogui.FAILSAFE = True
pyautogui.PAUSE = 0.02

DEFAULT_INPUT = "./opts/input.txt"

SPECIAL_KEYS = {
    "<ENTER>": "enter",
    "<TAB>": "tab",
    "<UP>": "up",
    "<DOWN>": "down",
    "<LEFT>": "left",
    "<RIGHT>": "right",
    "<ESC>": "esc",
    "<BACKSPACE>": "backspace",
    "<DELETE>": "delete",
    "<HOME>": "home",
    "<END>": "end",
    "<PGUP>": "pageup",
    "<PGDN>": "pagedown",
}

CTRL_PATTERN = re.compile(r"<CTRL-([A-Z])>", re.IGNORECASE)
ENTER_N_PATTERN = re.compile(r"<ENTER_N:(\d+)>", re.IGNORECASE)
SLEEP_PATTERN = re.compile(r"<SLEEP:([0-9]+(?:\.[0-9]+)?)>", re.IGNORECASE)
WAIT_PATTERN  = re.compile(r"<(?:WAIT|EXPECT):(.*)>", re.IGNORECASE)
PASTE_PATTERN = re.compile(r"<PASTE:(.+)>", re.IGNORECASE)


def parse_args():
    p = argparse.ArgumentParser(description="xdotool-like GUI key sender for TUIs, in Python.")
    p.add_argument("--input", "-i", default=DEFAULT_INPUT,
                   help=f"Path to input file with keystrokes (default: {DEFAULT_INPUT})")
    p.add_argument("--terminal", default="alacritty",
                   help="Terminal to use (alacritty|gnome-terminal|xterm|kitty|konsole...)")
    p.add_argument("--term-title", default=None,
                   help="Terminal window title override. Default: auto-generated unique title.")
    p.add_argument("--wait-start", type=float, default=0.6,
                   help="Seconds to wait after opening the terminal before typing (default: 0.6)")
    p.add_argument("--wait-after-activate", type=float, default=0.2,
                   help="Extra wait after focusing/activating the terminal (default: 0.2)")
    p.add_argument("--activate-cmd", default="wmctrl -a {title}",
                   help="Command to activate window by title (use {title} placeholder). Set empty to skip.")
    p.add_argument("--verbose", "-v", action="store_true", help="Print what is being sent.")
    p.add_argument("program", help="Path to the program/binary to execute.")
    p.add_argument("prog_args", nargs=argparse.REMAINDER,
                   help="Arguments for the program. Use '--' before them if they look like script options.")
    return p.parse_args()


def build_terminal_command(terminal, title, program, program_args):
    prog = shlex.join([program] + program_args)
    if terminal == "alacritty":
        return [ "bash", "-lc",
            f'alacritty --title {shlex.quote(title)} -e bash -lc {shlex.quote(prog)}' ]
    if terminal == "gnome-terminal":
        return [ "bash", "-lc",
            f'gnome-terminal --title {shlex.quote(title)} -- bash -lc {shlex.quote(prog)}' ]
    if terminal == "xterm":
        return [ "bash", "-lc",
            f'xterm -T {shlex.quote(title)} -e {prog}' ]
    if terminal == "kitty":
        return [ "bash", "-lc",
            f'kitty --title {shlex.quote(title)} bash -lc {shlex.quote(prog)}' ]
    if terminal == "konsole":
        return [ "bash", "-lc",
            f'konsole --new-tab --hold -p tabtitle={shlex.quote(title)} -e bash -lc {shlex.quote(prog)}' ]
    return [ "bash", "-lc", f'{shlex.quote(terminal)} -e {prog}' ]


def try_activate_window(activate_cmd_template, title, verbose=False):
    if not activate_cmd_template:
        return
    cmd = activate_cmd_template.format(title=shlex.quote(title))
    try:
        if verbose:
            print(f"[activate] {cmd}")
        subprocess.run(cmd, shell=True, check=False,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    except Exception:
        pass


def send_ctrl(letter):
    pyautogui.hotkey('ctrl', letter.lower())


def send_text(text, verbose=False):
    if verbose:
        print(f"[type] {text}")
    pyautogui.typewrite(text)


def send_key(keyname, verbose=False):
    if verbose:
        print(f"[key] {keyname}")
    pyautogui.press(keyname)


def run_replay(input_path, verbose=False):
    with open(input_path, "r", encoding="utf-8") as f:
        for raw in f:
            line = raw.rstrip("\r\n")

            # Blank line → ENTER
            if line.strip() == "":
                send_key("enter", verbose)
                continue

            s = line.strip()

            # Comment
            if s.startswith("#"):
                continue

            # ENTER_N
            m = ENTER_N_PATTERN.fullmatch(s)
            if m:
                for _ in range(max(1, int(m.group(1)))):
                    send_key("enter", verbose)
                continue

            # SLEEP
            m = SLEEP_PATTERN.fullmatch(s)
            if m:
                delay = float(m.group(1))
                if verbose:
                    print(f"[sleep] {delay}s")
                time.sleep(delay)
                continue

            # WAIT/EXPECT (dummy delay only)
            m = WAIT_PATTERN.fullmatch(s)
            if m:
                if verbose:
                    print(f"[wait] 0.2s (no screen read)")
                time.sleep(0.2)
                continue

            # PASTE:file
            m = PASTE_PATTERN.fullmatch(s)
            if m:
                text = open(m.group(1), "r", encoding="utf-8").read()
                if verbose:
                    print(f"[paste] {m.group(1)}")
                pyautogui.typewrite(text)
                continue

            # Special keys
            keyname = SPECIAL_KEYS.get(s.upper())
            if keyname:
                send_key(keyname, verbose)
                continue

            # CTRL-<X>
            m = CTRL_PATTERN.fullmatch(s)
            if m:
                send_ctrl(m.group(1))
                continue

            # Default: type text only (no Enter)
            send_text(line, verbose)


def _wait_for_q_or_fallback():
    """
    Wait for 'q' globally using pynput. If not available, fallback to pressing Enter in this shell.
    """
    print("[done] Scripted input finished. Press 'q' in your keyboard to quit the driver "
          "(or Ctrl+C here).", file=sys.stderr)

    if not PYNPUT_OK:
        try:
            input("[fallback] Global key listening unavailable. Press Enter here to quit… ")
        except KeyboardInterrupt:
            pass
        return

    stop_event = Event()

    def on_press(key):
        try:
            if hasattr(key, 'char') and key.char == 'q':
                stop_event.set()
                return False  # stop listener
        except Exception:
            pass

    listener = keyboard.Listener(on_press=on_press)
    try:
        listener.start()
        # Block until 'q' pressed
        stop_event.wait()
    finally:
        try:
            listener.stop()
        except Exception:
            pass


def main():
    args = parse_args()

    if not os.path.isfile(args.input):
        print(f"Input file not found: {args.input}", file=sys.stderr)
        sys.exit(1)

    title = args.term_title or f"TUIDRIVER-{uuid.uuid4().hex[:8]}"

    remainder = args.prog_args
    if remainder and remainder[0] == "--":
        remainder = remainder[1:]
    term_cmd = build_terminal_command(args.terminal, title, args.program, remainder)

    if args.verbose:
        print("[launch]", " ".join(term_cmd))

    subprocess.Popen(term_cmd, stdin=subprocess.DEVNULL,
                     stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    time.sleep(args.wait_start)
    try_activate_window(args.activate_cmd, title, verbose=args.verbose)
    time.sleep(args.wait_after_activate)

    try:
        run_replay(args.input, verbose=args.verbose)
        _wait_for_q_or_fallback()
    except KeyboardInterrupt:
        print("\n[abort] keyboard interrupt — stopping driver.", file=sys.stderr)


if __name__ == "__main__":
    main()
