# AY Sound FX Editor wx — Project Overview

## What it is

**AY Sound FX Editor** is a well-known program for composing sound effects targeting the **AY-3-8910 / AY8910** programmable sound generator — the chip used in **ZX Spectrum**, **MSX**, **Amstrad CPC**, and many other 8-bit computers of the 1980s. The original program (v0.6, by shiru@mail.ru) was Windows-only.

**ayfxedit-wx** is a cross-platform port (Windows / Linux / macOS) built with **wxWidgets 3.2+** and **miniaudio**, written in C++20.

## Purpose

The program lets you compose sound effects frame by frame for the AY-3-8910 chip:

- Each **frame** corresponds to 1/50 of a second (PAL TV frame rate)
- An effect can be up to **4096 frames** long (~82 seconds)
- A **bank** can hold up to **256 effects**
- The output is compatible with the original `.afb`/`.afx` format and with the bundled Z80 assembly player for ZX Spectrum

## Documents

| File | Contents |
|---|---|
| `ARCHITECTURE.md` | Code structure, main classes, layers |
| `AUDIO_ENGINE.md` | AY8910 emulator and miniaudio integration |
| `DATA_FORMATS.md` | Binary format for `.afx` (single effect) and `.afb` (bank) |
| `UI.md` | User interface, menus, toolbar, editor canvas |
| `BUILD.md` | Build instructions for Windows, Linux, macOS |

## Main dependencies

| Library | Version | Role |
|---|---|---|
| wxWidgets | 3.2+ | Cross-platform UI framework |
| miniaudio | git submodule | PCM audio output |
| dr_libs | git submodule | WAV read/write (for future import/export) |
| UbuntuMono | bundled TTF | Monospace font used in the editor canvas |

## Supported platforms

| OS | Status |
|---|---|
| Windows 10/11 | Primary, tested with VS2022 |
| Linux | Supported |
| macOS | Supported (brew wxwidgets) |
