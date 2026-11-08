# RapidReader 0.1.0

Custom RSVP Nano firmware for **Waveshare ESP32-S3 Touch LCD 3.49 Rev2**
(16 MB flash, 8 MB OPI PSRAM). The firmware identifies itself as
`rapid-reader-0.1.0`; the PlatformIO environment is `rapid_reader_0_1_0`.

## Interface

- Russian is the default. Settings → Language switches between Русский and English.
  Both translations and the Cyrillic UI/reading fonts are embedded in firmware;
  no language pack or network connection is needed.
- The boot screen uses the RR monogram from `RSVPConverter/public/favicon.svg`.
  The default theme follows that project's signal-red, dark-plum and warm-white
  palette from `src/promo/promo.css`.
- The menu contains reading and Settings. Reading retains resume, chapters and
  library. Settings directly contains brightness, theme, language, standby timer
  and screensaver, formerly under Display.
- Device, Focus, network/update menus, reading/pacing/layout configuration and
  per-book font controls are hidden. Hidden reading preferences reset to their
  upstream defaults on load and on settings edits. Reading speed stays adjustable
  and is saved. Display preferences are saved normally. Network and update
  settings remain at their defaults, including startup update checks being off.
- USB serial transfers remain compatible with the existing companion/converter.
  Books still require a FAT32 microSD card; embedded language assets do not
  replace book storage.

The original board environments remain available for upstream development.
Product behavior is enabled only with `RAPID_READER=1`.

## Screen previews

Rendered with the production UI and Arduino_GFX rasterizer at 640×172.

![RapidReader boot](images/rapid-reader/boot.png)

![Russian reading menu](images/rapid-reader/menu-ru.png)

![Russian Settings](images/rapid-reader/settings-ru.png)

![English Settings](images/rapid-reader/settings-en.png)

## Build and install

Use Python 3.11+ and PlatformIO Core 6.1.19 or newer.

```sh
git submodule update --init --recursive
pio run -e rapid_reader_0_1_0
python tools/package_rapid_reader.py --skip-build
```

Outputs are in `build/rapid-reader-0.1.0/`, with `release.json` containing
SHA-256 hashes. The binary without `-ota` contains the bootloader, partition
table and application and is flashed at **0x0**. The `-ota.bin` contains only the
application. Match the Rev2 hardware before flashing.

The manual **RapidReader firmware** workflow in GitHub Actions runs the product
tests, builds Rev2 and uploads these files as a downloadable artifact.

To flash a connected Rev2 directly:

```sh
pio run -e rapid_reader_0_1_0 -t upload --upload-port <port>
```

Regenerate the embedded translations after changing `localization/strings.toml`:

```sh
python localization/generate_localization.py
```

Product regression checks:

```sh
pio test -e native_rapid_reader_test
python localization/generate_localization.py --check
```

The screen preview renderer is `test/native_rapid_reader_preview/run.py`.

## Attribution

Based on RSVP Nano, copyright (c) 2026 RSVP Nano contributors.
The original MIT notice is retained in the repository and packaged releases.
Existing third-party font/library licence notices remain in their directories.
RapidReader branding is adapted from the supplied local RSVPConverter checkout.
The product does not expose upstream OTA update controls; startup checks are disabled.
