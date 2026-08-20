# Code Architecture

## Directory structure

```
ayfxedit-wx/
├── src/
│   ├── app/            — wxWidgets UI (MainFrame, entry point)
│   ├── core/           — Data model (BankModel, data structures)
│   ├── audio/          — AY8910 emulator + miniaudio output
│   └── assets/fonts/   — Bundled TTF fonts (UbuntuMono)
├── sfxcollection/      — Sample effect library (.afx files)
├── third_party/        — miniaudio, dr_libs (git submodules)
├── support/            — References: original source (ayfxedit-improved), local wxWidgets
└── DOCS/               — This documentation
```

## Layers

```
┌──────────────────────────────────────┐
│  UI layer  (src/app/)                │
│  MainFrame  — single application window│
│  PianoWindow — floating note-entry tool│
│  AyfxApp    — wxApp entry point      │
├──────────────────────────────────────┤
│  Core layer  (src/core/)             │
│  BankModel — data + file I/O         │
│  AyfxEffect / AyfxCell — structures  │
├──────────────────────────────────────┤
│  Audio layer  (src/audio/)           │
│  AudioEngine — miniaudio playback    │
│  AY8910      — chip emulator         │
└──────────────────────────────────────┘
```

## Main classes

### `MainFrame` (`src/app/MainFrame.h/.cpp`, ~2150 lines)

The single application window. Contains:
- A `BankModel bankModel_` member (by value)
- An `AudioEngine audioEngine_` member (by value)
- A lazily-created `PianoWindow* pianoWindow_` (shown/hidden, never destroyed until `MainFrame` closes)
- All custom canvas rendering (`drawEditor`)
- Mouse handling (`mouseEditAt`) and keyboard handling (`wxEVT_CHAR_HOOK`)
- All dialogs (Audio Settings, About, etc.)

There is no separate controller: UI logic and editing/navigation logic both live in `MainFrame`.

### `PianoWindow` (`src/app/PianoWindow.h/.cpp`, ~400 lines)

Floating tool window ported from the original's `TFormPiano` (`UnitPiano.cpp/.dfm`). Owner-draws a one-octave piano (white keys `Z X C V B N M`, black keys `S D G H J`, labelled by computer key rather than note name) and holds Octave/Step/Fill/Volume/Freq.-table controls. Playing a note calls back into `MainFrame::enterFromPiano()`, which writes the looked-up AY period into the bank at the cursor and advances it — the note→period lookup tables (5 tracker frequency tables × 8 octaves × 12 semitones) are copied verbatim from the original. Details in `UI.md`.

### `BankModel` (`src/core/BankModel.h/.cpp`, ~380 lines)

Pure data model with no wxWidgets dependency. Manages:
- A `std::vector<AyfxEffect>` holding the current bank's effects
- Load/save of `.afb` (bank) and `.afx` (single effect) files
- CRUD operations on effects (add, insert, delete)
- Encoding/decoding of the compressed binary format

### `AudioEngine` (`src/audio/AudioEngine.h/.cpp`, ~213 lines)

miniaudio audio backend. Manages:
- Initialisation and shutdown of the miniaudio playback device
- Full pre-rendering of an effect into a PCM buffer (`std::vector<int16_t>`)
- Buffer streaming via a pull-based `ma_device` data callback
- Output device enumeration (`enumerateDevices`)
- Runtime reconfiguration (sample rate, volume, device)

### `AY8910` (`src/audio/AY8910.h/.cpp`, ~356 lines)

Cycle-accurate AY-3-8910 chip emulator, derived from the Gearcoleco project (GPL-3.0). Details in `AUDIO_ENGINE.md`.

## Source file map

| File | Lines | Role |
|---|---|---|
| `src/app/main.cpp` | 101 | wxApp; Windows font registration |
| `src/app/MainFrame.h` | 105 | MainFrame declaration |
| `src/app/MainFrame.cpp` | ~2050 | Full UI implementation |
| `src/app/PianoWindow.h` | 49 | PianoWindow declaration |
| `src/app/PianoWindow.cpp` | 352 | Piano input window: note tables, keys, note entry |
| `src/app/app_resources.rc` | 2 | Windows RC: embedded fonts (ID 101, 102) |
| `src/core/BankModel.h` | 54 | Data structures + BankModel declaration |
| `src/core/BankModel.cpp` | 326 | File I/O, encode/decode, CRUD |
| `src/audio/AY8910.h` | 59 | AY8910 declaration |
| `src/audio/AY8910.cpp` | 356 | AY chip emulator |
| `src/audio/AudioEngine.h` | 55 | `AudioConfig`, AudioEngine declaration |
| `src/audio/AudioEngine.cpp` | 158 | miniaudio playback |
| `src/CMakeLists.txt` | 83 | Build system |

**Total source: ~3750 lines.**

## Feature status

| Feature | Status |
|---|---|
| Frame-by-frame editing | Complete |
| AY8910 playback via miniaudio | Complete |
| Load/Save `.afb` and `.afx` | Complete |
| Audio configuration (device, rate, volume) | Complete (not persisted) |
| Piano input | Complete (ported from the original's `TFormPiano`) |
| Import PSG / VTX / VGM / Wave | Stub (not implemented) |
| Export VTII Sample / Wave / CSV | Stub (not implemented) |
| Persistent configuration | Not implemented |
| Multi-load / Multi-save | Stub |
