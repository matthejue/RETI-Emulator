#!/usr/bin/env python3

from pathlib import Path
import argparse
import sys


def replace_int_2_with_int_1(directory: Path) -> int:
    if not directory.exists():
        print(f"Error: directory does not exist: {directory}", file=sys.stderr)
        return 1

    if not directory.is_dir():
        print(f"Error: not a directory: {directory}", file=sys.stderr)
        return 1

    changed_files = 0

    for file_path in directory.glob("*.reti"):
        try:
            original = file_path.read_text(encoding="utf-8")
        except UnicodeDecodeError:
            print(f"Skipping non-UTF-8 file: {file_path}", file=sys.stderr)
            continue

        modified = original.replace("INT 2", "INT 1")

        if modified != original:
            file_path.write_text(modified, encoding="utf-8")
            changed_files += 1
            print(f"Updated: {file_path}")

    print(f"Done. Changed {changed_files} .reti file(s).")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Replace all occurrences of 'INT 2' with 'INT 1' in .reti files in a directory."
    )
    parser.add_argument(
        "directory",
        help="Directory containing .reti files."
    )

    args = parser.parse_args()
    return replace_int_2_with_int_1(Path(args.directory))


if __name__ == "__main__":
    raise SystemExit(main())
