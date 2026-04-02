#!/usr/bin/env python3
"""
fix-project-version.py
----------------------
Updates all device YAMLs so the compiled firmware version tracks the ESPHome
release tag instead of a hardcoded string like "1.0.0".

Changes made to each device YAML:
  1. Adds  firmware_version: "0.0.0"  to the substitutions block
     (creates the block if none exists).
  2. If the device has an esphome.project.version field, replaces the
     hardcoded value with  ${firmware_version}.
  3. If the device has no esphome.project block at all, inserts one so the
     update entity has a version to compare against.

At build time (CI) the compile step passes:
    esphome compile --substitution "firmware_version=2026.3.2" devices/X.yaml
which overrides the "0.0.0" default with the real release version.

Run from the repo root:
    python3 scripts/fix-project-version.py [--dry-run]
"""

import re
import sys
from pathlib import Path

DEVICES_DIR = Path("devices")
SKIP_FILES = {"secrets.yaml"}
SKIP_PREFIX = "device-"
DRY_RUN = "--dry-run" in sys.argv

FIRMWARE_VERSION_LINE = '  firmware_version: "0.0.0"\n'


def add_firmware_version_sub(lines: list[str]) -> list[str]:
    """Add firmware_version to substitutions block, or create block."""
    # Check if already present
    if any("firmware_version" in line for line in lines):
        return lines

    # Find existing substitutions: block
    for i, line in enumerate(lines):
        if re.match(r'^substitutions:\s*$', line):
            # Insert after the substitutions: line
            return lines[:i + 1] + [FIRMWARE_VERSION_LINE] + lines[i + 1:]

    # No substitutions block — insert one before the first top-level key
    for i, line in enumerate(lines):
        if re.match(r'^[a-zA-Z]', line):
            insert = ['substitutions:\n', FIRMWARE_VERSION_LINE, '\n']
            return lines[:i] + insert + lines[i:]

    return lines


def fix_project_version(lines: list[str], device_stem: str) -> list[str]:
    """Replace hardcoded project.version or add project block if missing."""
    in_esphome = False
    in_project = False
    esphome_indent_end = -1  # last line index that is part of esphome block
    project_line_idx = -1
    version_line_idx = -1

    for i, line in enumerate(lines):
        stripped = line.rstrip('\n')

        if re.match(r'^esphome:\s*$', stripped):
            in_esphome = True
            continue

        if in_esphome:
            if re.match(r'^  \S', stripped):  # 2-space indented key in esphome
                esphome_indent_end = i
                if re.match(r'^  project:\s*$', stripped):
                    in_project = True
                    project_line_idx = i
                else:
                    in_project = False
            elif re.match(r'^    \S', stripped) and in_project:
                # 4-space indented key inside project block
                if re.match(r'^    version:\s+', stripped):
                    version_line_idx = i
            elif re.match(r'^[a-zA-Z]', stripped):
                # New top-level key — esphome block is over
                in_esphome = False
                in_project = False

    if version_line_idx != -1:
        # Replace hardcoded version value
        old_line = lines[version_line_idx]
        new_line = re.sub(r'^(\s+version:\s+).*$', r'\1${firmware_version}', old_line.rstrip('\n')) + '\n'
        if old_line == new_line:
            return lines  # already using substitution
        lines = lines[:]
        lines[version_line_idx] = new_line
        return lines

    if project_line_idx != -1:
        # project: block exists but no version: line — add it
        lines = lines[:]
        lines.insert(project_line_idx + 1, '    version: ${firmware_version}\n')
        return lines

    # No project block at all — add one at end of esphome block
    if esphome_indent_end != -1:
        # Build a sanitized project name from filename
        name_part = re.sub(r'[^a-zA-Z0-9]', '_', device_stem)
        project_block = [
            '  project:\n',
            f'    name: lilmansplace.{name_part}\n',
            '    version: ${firmware_version}\n',
        ]
        lines = lines[:]
        lines[esphome_indent_end + 1:esphome_indent_end + 1] = project_block
        return lines

    print(f"  WARNING: could not locate esphome block — skipping project injection")
    return lines


def process_file(yaml_file: Path) -> bool:
    """Process one device YAML. Returns True if the file was changed."""
    original = yaml_file.read_text(encoding="utf-8")
    lines = original.splitlines(keepends=True)

    lines = add_firmware_version_sub(lines)
    lines = fix_project_version(lines, yaml_file.stem)

    updated = "".join(lines)
    if updated == original:
        return False

    if not DRY_RUN:
        yaml_file.write_text(updated, encoding="utf-8")
    return True


def main():
    changed = []
    skipped = []

    yaml_files = sorted(DEVICES_DIR.glob("*.yaml"))
    for yaml_file in yaml_files:
        if yaml_file.name in SKIP_FILES or yaml_file.name.startswith(SKIP_PREFIX):
            continue
        if yaml_file.parent.name == "includes":
            continue

        if process_file(yaml_file):
            changed.append(yaml_file.name)
            print(f"  updated: {yaml_file.name}")
        else:
            skipped.append(yaml_file.name)

    print(f"\n{'[DRY RUN] ' if DRY_RUN else ''}Done.")
    print(f"  Changed : {len(changed)}")
    print(f"  Unchanged: {len(skipped)}")
    if DRY_RUN and changed:
        print(f"\nFiles that would be changed:")
        for f in changed:
            print(f"  {f}")


if __name__ == "__main__":
    main()
