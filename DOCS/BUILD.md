# Build Instructions

## Prerequisites

| Tool | Minimum version |
|---|---|
| CMake | 3.24 |
| C++ compiler | C++20 support (MSVC 2022, GCC 12+, Clang 15+) |
| wxWidgets | 3.2+ |
| SDL3 | Latest |

---

## Windows (Visual Studio 2022)

### 1. SDL3

Download the SDL3 development package (MSVC) and extract to `C:\SDL3`. The CMake script expects:

```
C:\SDL3\include\SDL3\SDL.h
C:\SDL3\lib\x64\SDL3.lib
C:\SDL3\lib\x64\SDL3.dll
```

Or set `-DSDL3_ROOT=<your_path>` at configure time.

### 2. wxWidgets

Either install wxWidgets system-wide (vcpkg or manual build) so that CMake's `find_package(wxWidgets CONFIG)` can find it, or place a local build under:

```
<repo_root>/.local/wx/lib/cmake/wxWidgets-3.3/
```

Alternatively build `support/wxWidgets-3.3.3/` from source and pass `-DAYFX_USE_LOCAL_WX=ON`.

### 3. Configure and build

```bat
cmake -S src -B build-vs2022 -G "Visual Studio 17 2022" -A x64
cmake --build build-vs2022 --config Release
```

### 4. Output

```
build-vs2022/Release/ayfxedit_wx.exe
build-vs2022/Release/SDL3.dll          (copied automatically by post-build rule)
```

---

## Linux

### 1. Install dependencies

**Ubuntu / Debian:**
```bash
sudo apt install build-essential cmake libwxgtk3.2-dev
```

SDL3 must be built from source (not yet in most distro repos):
```bash
git clone https://github.com/libsdl-org/SDL.git -b main
cd SDL && cmake -B build -DCMAKE_BUILD_TYPE=Release && sudo cmake --install build
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
brew install wxwidgets sdl3
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
| `SDL3_ROOT` | `C:/SDL3` (Windows) | Path to SDL3 development package |

---

## Submodules

Initialize `third_party/dr_libs` before building:

```bash
git submodule update --init --recursive
```

`dr_libs` provides `dr_wav.h`, `dr_mp3.h`, and `dr_flac.h` for future WAV/audio import-export. Currently only added to the include path; not yet called from source.

---

## Windows resource compilation

`src/app/app_resources.rc` embeds the UbuntuMono font files as RCDATA (IDs 101 and 102). This is handled automatically by the CMake `target_sources` directive when building with MSVC. The fonts are loaded at runtime via `AddFontMemResourceEx` in `AyfxApp::OnInit`.
