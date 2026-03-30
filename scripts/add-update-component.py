#!/usr/bin/env python3
"""
add-update-component.py
-----------------------
One-time helper script that adds the OTA-update package include to every
home device YAML that doesn't already have it.

Run from the repo root:
    python3 scripts/add-update-component.py [--dry-run]

What it adds to each eligible device YAML:
    packages:
      ota_updates: !include ../includes/ota-updates.yaml   # added line

Skips:
  - device-*.yaml  (public adoption templates — no !secret refs)
  - Any YAML that already contains "ota-updates" or "http_request"
  - secrets.yaml
"""

import argparse
import sys
from pathlib import Path


DEVICES_DIR = Path("devices")
OTA_PACKAGE_LINE = "  ota_updates: !include ../includes/ota-updates.yaml\n"


def process_file(yaml_path: Path, dry_run: bool) -> str:
    """Return 'modified', 'skipped', or 'already_has_it'."""
    name = yaml_path.stem

    # Never touch these
    if name.startswith("device-") or name == "secrets":
        return "skipped"

    content = yaml_path.read_text(encoding="utf-8")

    # Already patched?
    if "ota-updates" in content or "http_request" in content:
        return "already_has_it"

    # Find the `packages:` block and append our line
    if "packages:" not in content:
        # Device has no packages block — add one
        insert_block = "\npackages:\n" + OTA_PACKAGE_LINE
        new_content = content.rstrip() + insert_block + "\n"
    else:
        # Insert as the last item in the existing packages: block
        # Strategy: find the packages: line and add our line right after it
        # (ESPHome doesn't care about package order)
        lines = content.splitlines(keepends=True)
        new_lines = []
        inserted = False
        in_packages = False
        for i, line in enumerate(lines):
            new_lines.append(line)
            if line.strip().startswith("packages:") and not inserted:
                in_packages = True
            elif in_packages and not inserted:
                # We're in the packages block — insert after the first item
                # Actually, safest: insert BEFORE the first blank line or
                # first non-indented line after packages:
                is_still_packages = line.startswith("  ") or line.strip() == ""
                if not is_still_packages:
                    # We've left the packages block — insert before this line
                    new_lines.insert(-1, OTA_PACKAGE_LINE)
                    inserted = True
                    in_packages = False

        if in_packages and not inserted:
            # packages: was the last block in the file
            new_lines.append(OTA_PACKAGE_LINE)

        new_content = "".join(new_lines)

    if not dry_run:
        yaml_path.write_text(new_content, encoding="utf-8")

    return "modified"


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Add OTA update package to all home device YAMLs"
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Show what would change without writing files",
    )
    args = parser.parse_args()

    if not DEVICES_DIR.exists():
        print("ERROR: Run this script from the repo root (devices/ not found)")
        return 1

    counts = {"modified": 0, "skipped": 0, "already_has_it": 0}

    for yaml_file in sorted(DEVICES_DIR.glob("*.yaml")):
        result = process_file(yaml_file, args.dry_run)
        counts[result] += 1
        prefix = {
            "modified": "[UPDATED]" if not args.dry_run else "[WOULD UPDATE]",
            "skipped": "[SKIP    ]",
            "already_has_it": "[HAS IT  ]",
        }[result]
        print(f"  {prefix}  {yaml_file.name}")

    print()
    if args.dry_run:
        print("DRY RUN — no files written.")
    print(
        f"Modified: {counts['modified']}  "
        f"Already patched: {counts['already_has_it']}  "
        f"Skipped templates: {counts['skipped']}"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
