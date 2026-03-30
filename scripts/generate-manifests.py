#!/usr/bin/env python3
"""
generate-manifests.py
---------------------
Generates per-device ESPHome OTA update manifest JSON files from compiled
firmware binaries.  Run by CI after a tagged release build.

Usage:
    python3 scripts/generate-manifests.py \
        --version v1.2.0 \
        --firmware-dir firmware/ \
        --output-dir manifests/

Manifest format consumed by the ESPHome http_request update platform:
    {
      "name": "device-name",
      "version": "v1.2.0",
      "changelog": "https://github.com/owner/repo/releases/tag/v1.2.0",
      "ota": {
        "url": "https://github.com/owner/repo/releases/download/v1.2.0/device-name.bin",
        "md5": "<md5-of-bin-file>",
        "summary": "ESPHome firmware v1.2.0"
      }
    }
"""

import argparse
import hashlib
import json
import sys
from pathlib import Path


REPO = "lilmansplace/my-esphome-builds"


def md5sum(filepath: Path) -> str:
    md5 = hashlib.md5()
    with open(filepath, "rb") as fh:
        for chunk in iter(lambda: fh.read(65536), b""):
            md5.update(chunk)
    return md5.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Generate ESPHome OTA update manifests"
    )
    parser.add_argument(
        "--version", required=True, help="Release version tag (e.g. v1.2.0)"
    )
    parser.add_argument(
        "--firmware-dir",
        required=True,
        help="Directory containing compiled .bin files",
    )
    parser.add_argument(
        "--output-dir",
        required=True,
        help="Directory to write manifest JSON files into",
    )
    parser.add_argument(
        "--repo",
        default=REPO,
        help=f"GitHub repo slug (default: {REPO})",
    )
    args = parser.parse_args()

    firmware_dir = Path(args.firmware_dir)
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    version = args.version
    repo = args.repo
    base_url = f"https://github.com/{repo}/releases/download/{version}"
    changelog_url = f"https://github.com/{repo}/releases/tag/{version}"

    bin_files = sorted(firmware_dir.glob("*.bin"))
    if not bin_files:
        print(f"ERROR: No .bin files found in {firmware_dir}", file=sys.stderr)
        return 1

    print(f"Generating {len(bin_files)} manifests for version {version}...")
    for bin_file in bin_files:
        device_name = bin_file.stem
        firmware_url = f"{base_url}/{bin_file.name}"
        checksum = md5sum(bin_file)

        manifest = {
            "name": device_name,
            "version": version,
            "changelog": changelog_url,
            "ota": {
                "url": firmware_url,
                "md5": checksum,
                "summary": f"ESPHome firmware {version}",
            },
        }

        out_path = output_dir / f"{device_name}.json"
        with open(out_path, "w", encoding="utf-8") as fh:
            json.dump(manifest, fh, indent=2)
            fh.write("\n")

        print(f"  {out_path.name}  md5={checksum[:8]}...  {firmware_url}")

    print(f"\nDone — {len(bin_files)} manifests written to {output_dir}/")
    return 0


if __name__ == "__main__":
    sys.exit(main())
