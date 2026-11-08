"""Render RapidReader screens with the production UI and Arduino_GFX rasterizer.

Build native_rapid_reader_test and install firmware dependencies first. Output
is PPM at the actual Rev2 screen size, 640x172. No hardware is needed.
"""

import argparse
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from pio_gfx import patch_u8g2_coordinates


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--compiler", default=shutil.which("g++"))
    parser.add_argument("--library", type=Path, default=ROOT / ".pio/libdeps/rapid_reader_0_1_0/GFX Library for Arduino/src")
    parser.add_argument("--native-build", type=Path, default=ROOT / ".pio/build/native_rapid_reader_test")
    parser.add_argument("--harfbuzz-include", type=Path, default=ROOT / "lib/HarfBuzz/upstream/src")
    parser.add_argument("--harfbuzz-library", type=Path)
    parser.add_argument("--sheen-library", type=Path)
    args = parser.parse_args()
    libraries = []
    for name, explicit in (("HarfBuzz", args.harfbuzz_library), ("SheenBidi", args.sheen_library)):
        library = explicit or next(args.native_build.glob(f"lib*/lib{name}.a"), None)
        if library is None:
            parser.error(f"Missing {name}; build native_rapid_reader_test first")
        libraries.append(str(library))
    sources = ["ui/Ui.cpp", "ui/Controls.cpp", "ui/Icons.cpp", "ui/Theme.cpp", "ui/Localization.generated.cpp",
               "localization/LocalePack.cpp", "text/LocaleTag.cpp", "text/BidiText.cpp", "text/TextShaping.cpp",
               "ui/screens/regular/ScreenCommon.cpp", "ui/screens/regular/InterfaceSettingsScreen.cpp",
               "ui/screens/regular/ReadScreen.cpp", "ui/RapidReaderBrand.cpp", "ui/RapidReaderUi.cpp", "ui/screens/regular/LibraryScreen.cpp",
               "ui/screens/regular/ChaptersScreen.cpp", "ui/screens/regular/StatusScreen.cpp", "reader/ReadingLoop.cpp"]
    includes = [ROOT / "test/native_watch_preview", ROOT / "test/native_canvas", ROOT / "test/support",
                ROOT / "src", args.library, args.harfbuzz_include, ROOT / "lib/SheenBidi/upstream/Headers",
                ROOT / ".pio/libdeps/native_rapid_reader_test/glaze/include"]
    with tempfile.TemporaryDirectory(prefix="rapid-reader-preview-") as temporary:
        folder = Path(temporary)
        rasterizer = folder / "Arduino_GFX.cpp"
        rasterizer.write_text(patch_u8g2_coordinates((args.library / "Arduino_GFX.cpp").read_text()))
        executable = folder / "preview"
        subprocess.run([
            args.compiler, "-std=c++23", "-O1", "-funsigned-char", "-ffunction-sections", "-fdata-sections",
            *(["-stdlib=libc++"] if "clang" in args.compiler else []),
            "-DRAPID_READER=1", "-DGLZ_DEFAULT_OPTIMIZATION_SIZE", "-DGLZ_DISABLE_ALWAYS_INLINE",
            *[f"-I{path}" for path in includes], str(Path(__file__).with_name("main.cpp")),
            *[str(ROOT / "src" / source) for source in sources],
            str(args.library / "Arduino_G.cpp"), str(rasterizer), str(args.library / "canvas/Arduino_Canvas.cpp"),
            *libraries, "-Wl,--gc-sections", "-o", str(executable),
        ], check=True)
        subprocess.run([str(executable), str(args.output.resolve())], check=True)


if __name__ == "__main__":
    main()
