# User Interface

## Window

Single `wxFrame` (`MainFrame`). Default size: 640×620 DIP, minimum: 640×320 DIP. All sizes use `FromDIP()` throughout for HiDPI correctness.

---

## Menu bar

### File

| Item | Shortcut | Action |
|---|---|---|
| New bank | Ctrl+N | Creates a fresh bank with one empty effect |
| Load bank... | Ctrl+O | Opens a `.afb` file |
| Save bank | Ctrl+S | Saves to current path (defaults to `noname.afb`) |
| Save bank without names | — | Saves `.afb` omitting effect name strings |
| Load current effect... | — | Replaces current effect from `.afx` file |
| Save current effect... | — | Saves current effect as `.afx` file |
| Multi-load... | — | **Stub** (not implemented) |
| Multi-save... | — | **Stub** (not implemented) |
| Exit | — | Quits the application |

### Edit

| Item | Shortcut | Action |
|---|---|---|
| Cut | Ctrl+X | Cuts selected frames to clipboard |
| Copy | Ctrl+C | Copies selected frames |
| Paste | Ctrl+V | Inserts clipboard frames at cursor |
| Delete | — | Deletes selected frames |
| Select all | Ctrl+A | Selects all frames in current effect |
| Unselect all | — | Clears selection |
| Inverse selection | Ctrl+I | Toggles selection state of all frames |

### View

| Item | Action |
|---|---|
| Piano input | Toggle piano input mode (UI stub, logic not yet implemented) |
| Linear period | Display tone periods with linear scale bars |
| Logarithmic period | Display tone periods with logarithmic scale bars |

### Bank

| Item | Shortcut | Action |
|---|---|---|
| First effect | — | Navigate to effect 0 |
| Previous effect | `-` | Navigate one effect back |
| Next effect | `+` | Navigate one effect forward |
| Last effect | — | Navigate to the last effect |
| Add effect | — | Appends a new empty effect |
| Insert effect | — | Inserts an empty effect before the current one |
| Delete effect | — | Deletes the current effect |

### Import / Export

All items are **stubs** (not implemented). Planned formats:

- Import: PSG, VTX, VGM, Wave
- Export: VTII Sample, Wave, CSV (with current/all radio)

### Help

| Item | Action |
|---|---|
| Audio settings... | Opens the audio configuration dialog |
| About | Shows the About dialog |

---

## Toolbar

Left to right:

| Control | Type | Action |
|---|---|---|
| Play | `wxBitmapButton` (green triangle) | Pre-renders and plays the current effect |
| Stop | `wxBitmapButton` (black square) | Stops playback |
| Piano | `wxBitmapToggleButton` (piano icon) | Toggles piano input mode |
| Add | `wxButton` | Adds a new empty effect |
| Del | `wxButton` | Deletes the current effect |
| `<<` | `wxButton` | First effect |
| `<` | `wxButton` | Previous effect |
| NNN/NNN | `wxTextCtrl` (read-only) | Current effect index / total count |
| `>` | `wxButton` | Next effect |
| `>>` | `wxButton` | Last effect |
| Effect name | `wxTextCtrl` | Editable on left-click; confirms on Enter; read-only otherwise |

**Status bar:** "Frames used: N \| Cursor: N"

---

## Editor canvas

A `wxPanel` with `wxBG_STYLE_PAINT`, fully owner-drawn via `wxAutoBufferedPaintDC` in `drawEditor()`.

### Column layout

| Column | Content |
|---|---|
| Pos | Frame number (hex) |
| T | Tone enable flag: `T` or `–` |
| N | Noise enable flag: `N` or `–` |
| Per | Tone period (3 hex digits) |
| Ns | Noise period (2 hex digits) |
| V | Volume (1 hex digit) |
| Period bar | Proportional bar, width ∝ tone period (4 parts of remaining width) |
| Noise bar | Proportional bar, width ∝ noise period (1 part) |
| Volume bar | Proportional bar, width ∝ volume (1 part) |

Bar widths are distributed at a **4:1:1** ratio (tone : noise : volume).

### Period bar scale modes

| Mode | Formula |
|---|---|
| Linear | `1 + toneWidth × period / 4096` |
| Logarithmic | `toneWidth × log(period / 8) / log(4095 / 8)` |

### Colours (`EditorPalette::light()`)

| Element | Colour |
|---|---|
| Background | White |
| Normal text | Black |
| Bar fill | Dark grey |
| Selected row | `#C0FFC0` (light green) |
| Cursor row | Black background, white text |

### Mouse editing

| Action | Effect |
|---|---|
| LMB click on Pos column | Move cursor to that row |
| RMB click on Pos column | Deselect that row |
| LMB/RMB drag on T or N column | Drag-paint tone/noise enable on/off |
| Click/drag on bar area | Set value proportionally to cursor X position |
| Click on hex text column | Move keyboard cursor to that row and column |

Fast drags interpolate across skipped rows.

### Keyboard editing

| Key | Action |
|---|---|
| Arrow Up / Down | Move cursor |
| Arrow Left / Right | Move between columns (tone period, noise period, volume) |
| `0`–`9`, `A`–`F` | Enter hex digit at cursor position |
| `T` | Toggle tone enable on cursor row |
| `N` | Toggle noise enable on cursor row |
| `Insert` | Insert a blank frame at cursor |
| `Delete` | Delete frame at cursor |
| `Shift` + arrows | Extend selection |
| `+` / `-` | Next / previous effect |
| `PgUp` / `PgDn` | Page scroll |
| `Home` / `End` | Jump to first / last frame |

---

## Audio Settings dialog

Opened from **Help → Audio settings...**

| Control | Type | Options |
|---|---|---|
| Output device | `wxChoice` | All miniaudio playback devices enumerated at open time |
| Sample rate | `wxChoice` | 22050 Hz, 44100 Hz, 48000 Hz, 96000 Hz |
| Volume | `wxSlider` | 0–100 |

On OK, calls `AudioEngine::reconfigure(cfg)` which shuts down and reinitialises the miniaudio playback device with the new parameters. Settings are **not persisted** between sessions.

---

## Fonts

The editor canvas uses a monospace font resolved via a fallback chain:

```
Consolas → Menlo → DejaVu Sans Mono → Liberation Mono → Courier New
```

On Windows, **UbuntuMono-Regular** and **UbuntuMono-Bold** are loaded at startup from resources embedded in the `.exe` (IDs 101 and 102) via `AddFontMemResourceEx`. These are preferred for the canvas rendering.
