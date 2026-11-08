"""Build and package the RapidReader LCD 3.49 Rev2 firmware."""

import argparse
import fnmatch
import hashlib
import json
import shutil
import subprocess
from pathlib import Path

from export_web_firmware import ROOT, merge_firmware

ENV = "rapid_reader_0_1_0"
VERSION = "rapid-reader-0.1.0"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--skip-build", action="store_true")
    parser.add_argument("--pio", default=shutil.which("pio") or str(Path.home() / ".platformio/penv/bin/pio"))
    parser.add_argument("--output", type=Path, default=ROOT / "build" / VERSION)
    args = parser.parse_args()
    if not args.skip_build:
        subprocess.run([args.pio, "run", "-e", ENV], cwd=ROOT, check=True)

    build = ROOT / ".pio/build" / ENV
    header = build / "generated/FirmwareVersion.generated.h"
    if not header.exists() or f'kFirmwareVersion[] = "{VERSION}";' not in header.read_text():
        raise SystemExit("Build does not identify itself as rapid-reader-0.1.0")
    args.output.mkdir(parents=True, exist_ok=True)
    merged = args.output / f"{VERSION}-esp32-s3-touch-lcd-3.49-rev2.bin"
    ota = args.output / f"{VERSION}-esp32-s3-touch-lcd-3.49-rev2-ota.bin"
    merge_firmware(ENV, merged)
    shutil.copy2(build / "firmware.bin", ota)
    shutil.copy2(ROOT / "LICENSE", args.output / "LICENSE")
    shutil.copy2(ROOT / "docs/rapid-reader.md", args.output / "README.md")
    shutil.copytree(ROOT / "docs/images/rapid-reader", args.output / "images/rapid-reader", dirs_exist_ok=True)
    patterns = ("LICENSE*", "COPYING*", "NOTICE*", "OFL.txt", "FONTLOG.txt")
    for directory in (ROOT / "third_party", ROOT / "lib", ROOT / ".pio/libdeps" / ENV):
        for path in directory.rglob("*"):
            if path.is_file() and any(fnmatch.fnmatch(path.name.upper(), pattern.upper()) for pattern in patterns):
                relative = path.relative_to(ROOT)
                destination = args.output / "notices" / relative
                destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(path, destination)
    artifacts = {
        path.name: {"bytes": path.stat().st_size, "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
        for path in (merged, ota)
    }
    (args.output / "release.json").write_text(json.dumps({
        "version": VERSION,
        "board": "waveshare_esp32s3_touch_lcd_3_49_rev2",
        "flash_offset": 0,
        "artifacts": artifacts,
    }, indent=2) + "\n")
    print(f"Packaged {VERSION} in {args.output}")


if __name__ == "__main__":
    main()
