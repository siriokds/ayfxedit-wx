# Build Instructions

## Prerequisites

| Tool | Minimum version |
|---|---|
| CMake | 3.24 |
| C++ compiler | C++20 support (MSVC 2022, GCC 12+, Clang 15+) |
| wxWidgets | 3.2+ |

Audio is provided by [miniaudio](https://github.com/mackron/miniaudio), vendored as a git submodule under `third_party/miniaudio` (header-only, statically compiled in — no separate install or DLL to ship).

---

## Windows (Visual Studio 2022)

### 1. wxWidgets

Either install wxWidgets system-wide (vcpkg or manual build) so that CMake's `find_package(wxWidgets CONFIG)` can find it, or place a local build under:

```
<repo_root>/.local/wx/lib/cmake/wxWidgets-3.3/
```

Alternatively build `support/wxWidgets-3.3.3/` from source and pass `-DAYFX_USE_LOCAL_WX=ON`.

### 2. Configure and build

```bat
cmake -S src -B build-vs2022 -G "Visual Studio 17 2022" -A x64
cmake --build build-vs2022 --config Release
```

### 3. Output

```
build-vs2022/Release/ayfxedit_wx.exe
```

---

## Linux

### 1. Install dependencies

**Ubuntu / Debian:**
```bash
sudo apt install build-essential cmake libwxgtk3.2-dev
```

### 2. Configure and build

```bash
cmake -S src -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### 3. Output

```
build/ayfxedit_wx
```

---

## macOS

### 1. Install dependencies via Homebrew

```bash
brew install wxwidgets
```

### 2. Configure and build

```bash
cmake -S src -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### 3. Output

```
build/ayfxedit_wx
```

---

## Continuous Integration

`.github/workflows/build.yml` builds on every push/PR, verifying the build only (no release artifacts):

- **macOS**: `brew install wxwidgets`, same as local dev above.
- **Windows**: wxWidgets has no ready-made Windows package, so it's built from source via vcpkg (`vcpkg install wxwidgets:x64-windows`), cached across runs (vcpkg's binary cache, persisted via `actions/cache`, keyed on a fixed suffix bumped manually when the triplet/wx version changes — not on the workflow file's own hash, which forced a ~15-20 min rebuild on every unrelated edit) since building it from scratch takes 10-20 minutes. Two Windows-specific gotchas hit while setting this up:
  - cmake's `-G "Visual Studio 17 2022"` generator failed with "could not find any instance of Visual Studio" on the `windows-latest` runner (which turned out to have VS 18 installed, not VS 17) — worked around by loading the MSVC dev environment directly via `ilammy/msvc-dev-cmd` and using the `NMake Makefiles` generator instead, which doesn't need to locate a VS instance itself.
  - MSVC failed on every `std::min`/`std::max` call in `AudioEngine.cpp` with a cryptic `error C2589: '(': illegal token on right side of '::'` — `<windows.h>` (pulled in transitively by `miniaudio.h`) defines `min`/`max` as macros unless `NOMINMAX` is defined first; fixed by adding `NOMINMAX`/`WIN32_LEAN_AND_MEAN` as compile definitions for `WIN32` builds in `src/CMakeLists.txt`.
- **Linux**: not set up yet.

### Releases

`.github/workflows/release.yml` triggers on pushing a `v*` tag (e.g. `v0.6.1`) and publishes a GitHub Release with downloadable, dependency-free zips for **macOS and Windows only** — both statically linked so the shipped binary doesn't need Homebrew/vcpkg installed on the end user's machine:

- **macOS**: wxWidgets 3.3.3 built from source with `--disable-shared` and its own bundled auxiliary libs (`--with-*=builtin`), cached (fixed key, same reasoning as above). The executable is wrapped in a proper `AY Sound FX Editor.app` bundle with a minimal `Info.plist` (`CFBundleIdentifier` `com.siriokds.ayfxedit-wx`) — a bare Unix executable has no icon/Dock presence and isn't how macOS software is normally distributed. No version number in the bundle/display name itself, per macOS convention (it goes in `Info.plist`, read from the pushed tag).
- **Windows**: vcpkg's `x64-windows-static` triplet links wxWidgets and the MSVC runtime into the `.exe` itself. Needed `-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded` on our own `cmake` configure call to match — the vcpkg toolchain file builds its own packages with the static CRT for this triplet but doesn't force that choice onto the consuming project automatically, so without it the linker rejects the mix (`error LNK2038: mismatch detected for 'RuntimeLibrary'`).
- **Linux**: intentionally excluded — a binary built on the CI's Ubuntu image is dynamically linked against that specific glibc/GTK3 version and isn't guaranteed to run on other distros; the honest option there is build-from-source (above), not a prebuilt binary that might silently fail elsewhere.

ayfxedit-wx's own version numbering (the `v*` tag) is independent of "v0.6" (the *original* Delphi tool's version, still shown in the app's title bar/About dialog since that's what's being ported) — first release is `v0.6.1`.

---

## CMake options

| Option | Default | Description |
|---|---|---|
| `AYFX_USE_LOCAL_WX` | `OFF` | Use `support/wxWidgets-3.3.3/` instead of a system install |

---

## Submodules

Initialize submodules before building:

```bash
git submodule update --init --recursive
```

- `third_party/miniaudio` provides `miniaudio.h`, used by `AudioEngine` for cross-platform audio playback. Header-only and statically compiled in — no separate install or DLL to ship.
- `third_party/dr_libs` provides `dr_wav.h`, `dr_mp3.h`, and `dr_flac.h` for future WAV/audio import-export. Currently only added to the include path; not yet called from source.

---

## Windows resource compilation

`src/app/app_resources.rc` embeds the UbuntuMono font files as RCDATA (IDs 101 and 102). This is handled automatically by the CMake `target_sources` directive when building with MSVC. The fonts are loaded at runtime via `AddFontMemResourceEx` in `AyfxApp::OnInit`.
