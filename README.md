# AY Sound FX Editor wx

Cross-platform port of [AY Sound FX Editor v0.6](https://github.com/Threetwosevensixseven/ayfxedit-improved) — a sound effect editor for the AY-3-8910/YM2149 PSG chip found in the ZX Spectrum 128, MSX, Amstrad CPC, and Atari ST — using wxWidgets 3.3 and miniaudio.

Runs on Windows, Linux, and macOS.

## Features

- Effect grid editor with piano-style note input, linear/logarithmic period view, and hex T/N/Period/Noise/Volume columns, matching the original's editing model
- Playback and export through a band-limited AY/YM emulator (game-music-emu's `Ay_Apu`/`Blip_Buffer`), rendered at 192kHz internally and downsampled through a 64-tap windowed-sinc filter, not the original's naive sample-and-hold synthesis
- Selectable PSG chip (AY-3-8910 / YM2149) and clock rate, independently — presets for ZX Spectrum, MSX, Amstrad CPC, and Atari ST, or a custom clock
- **Reclock tool**: converts an effect's (or a whole bank's) stored tone/noise periods between machine clocks, so effects made for one machine can be retuned for another, with a choice of out-of-range handling (octave shift, clamp, or silence)
- **Export**: WAV, CSV, VTII Sample (Vortex Tracker II), single effect or whole bank
- **Import**: PSG (AY register dump), VTX (Vortex Tracker's LH5-compressed format), VGM (SN76489 logs, converted to AY), WAV and MP3 (pitch/volume extraction via an AMDF-style tuner)
- Multi-load/multi-save for whole banks of individual effect files
- Native dark/light mode and system fonts/colours on macOS (see `DOCS/UI.md`)

## Dependencies

| Library | Version | Notes |
|---------|---------|-------|
| [wxWidgets](https://github.com/wxWidgets/wxWidgets) | 3.3.x | UI framework — build from source or use a package |
| [miniaudio](https://github.com/mackron/miniaudio) | latest | Header-only audio output — included as submodule |
| [dr_libs](https://github.com/mackron/dr_libs) | latest | Header-only WAV/MP3 decode (`dr_wav.h`, `dr_mp3.h`) — included as submodule |
| [game-music-emu](https://bitbucket.org/mpyne/game-music-emu) `Ay_Apu`/`Blip_Buffer` | — | Band-limited AY/YM synthesis core, vendored (with documented modifications) under `third_party/blip_ay_apu/`, not a submodule — only a handful of files are needed from a much larger multi-console repo |
| ar002 `lh5` (Haruhiko Okumura) | — | Public-domain LZSS+Huffman decoder used for VTX import, vendored under `third_party/lh5/` |

## Building

### 1. Clone with submodules

```sh
git clone --recurse-submodules https://github.com/siriokds/ayfxedit-wx.git
```

Or if already cloned:

```sh
git submodule update --init --recursive
```

### 2. Build wxWidgets

Download the [wxWidgets 3.3 source](https://github.com/wxWidgets/wxWidgets/releases) and build it.
Place the result so that `cmake` can find `wxWidgetsConfig.cmake`, or set:

```sh
-DwxWidgets_DIR=<path-to-wx>/lib/cmake/wxWidgets-3.3
```

On Windows the CMakeLists already checks `.local/wx/lib/cmake/wxWidgets-3.3` relative to the repo root, so you can also put the built tree there.

### 3. Configure and build

**Linux / macOS:**

```sh
cmake -B build -S src -DCMAKE_BUILD_TYPE=Release
cmake --build build
# binary: build/ayfxedit_wx
```

**Windows (Visual Studio 2022):**

```sh
cmake -B build-vs2022 -S src -G "Visual Studio 17 2022" -A x64
cmake --build build-vs2022 --config Release
# binary: build-vs2022/Release/ayfxedit_wx.exe
```

## File format

Compatible with the original `.afb` (bank) and `.afx` (single effect) binary format.

## Documentation

See `DOCS/` for architecture, audio engine, data format, UI, and build notes.
