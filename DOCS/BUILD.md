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
