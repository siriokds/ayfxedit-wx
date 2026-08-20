# AY Sound FX Editor wx

Cross-platform port of [AY Sound FX Editor v0.6](https://github.com/popovych-team/ayfxedit-improved) using wxWidgets 3.3 and SDL3.

Runs on Windows, Linux, and macOS.

## Dependencies

| Library | Version | Notes |
|---------|---------|-------|
| [wxWidgets](https://github.com/wxWidgets/wxWidgets) | 3.3.x | UI framework — build from source or use a package |
| [SDL3](https://github.com/libsdl-org/SDL) | 3.x | Audio output |
| [dr_libs](https://github.com/mackron/dr_libs) | latest | Header-only audio decode — included as submodule |

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

### 3. Install SDL3

**Windows:** download the SDL3 development package and unzip to `C:/SDL3`  
(or set `-DSDL3_ROOT=<path>` at configure time).

**Linux:** SDL3 is not yet in most distro repos. Build from source:

```sh
git clone https://github.com/libsdl-org/SDL.git -b release-3.x
cmake -B SDL/build -S SDL -DCMAKE_BUILD_TYPE=Release
cmake --build SDL/build
sudo cmake --install SDL/build
```

**macOS:** `brew install sdl3`

### 4. Configure and build

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
# SDL3.dll is copied automatically next to the executable
```

## File format

Compatible with the original `.afb` (bank) and `.afx` (single effect) binary format.
