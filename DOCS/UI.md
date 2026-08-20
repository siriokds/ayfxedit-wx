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

| Item | Shortcut | Action |
|---|---|---|
| Piano input | Ctrl+. or Ctrl+P | Shows/hides the Piano input window (see below) |
| Linear period | — | Display tone periods with linear scale bars |
| Logarithmic period | — | Display tone periods with logarithmic scale bars |

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
| Play | `wxBitmapButton` (green triangle) | Pre-renders and plays the current effect from frame 0 (Ctrl+Enter plays from the cursor row instead) |
| Stop | `wxBitmapButton` (black square) | Stops playback |
| Piano | `wxBitmapToggleButton` (piano icon) | Shows/hides the Piano input window |
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

### Colours

`EditorPalette::light()`/`dark()` hold the few colours with no OS-native equivalent (backgrounds, cell text, bar border, cursor-cell highlight, toolbar icon colours, the play-triangle green); `EditorPalette::current()` picks between them based on `wxSystemAppearance`. Everything else uses native system colours directly, so it also tracks the user's accent colour and light/dark switching automatically:

| Element | Colour source |
|---|---|
| Background | `EditorPalette` — white (light) / `#202020` (dark) |
| Normal text | `EditorPalette` — black (light) / `#E1E1E1` (dark) |
| Bar fill | `wxSYS_COLOUR_HIGHLIGHT` (system accent); `wxSYS_COLOUR_HIGHLIGHTTEXT` instead when the row is selected, so it doesn't blend into the selection background |
| Bar border | `EditorPalette` — black (light) / `#555555` (dark) |
| Selected row | `wxSYS_COLOUR_HIGHLIGHT` background, `wxSYS_COLOUR_HIGHLIGHTTEXT` text |
| Cursor cell | `EditorPalette` — black background/white text (light), inverted in dark |

Period/Noise/Volume bars are drawn with rounded corners (macOS-style), not sharp rectangles.

### Mouse editing

| Action | Effect |
|---|---|
| LMB click/drag on Pos column | Select that row (and drag to extend) |
| RMB click/drag on Pos column | Deselect that row |
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
| `Enter` | Play the effect from frame 0 |
| `Ctrl+Enter` | Play the effect from the cursor row |
| `Space` | Stop playback |
| `Ctrl+.` / `Ctrl+P` | Show/hide the Piano input window (two alternatives so a keyboard layout quirk in one doesn't strand the shortcut) |

---

## Piano input window

`PianoWindow` (ported from the original's `TFormPiano`), a floating tool window shown/hidden via the toolbar piano icon, **View → Piano input**, or Ctrl+./Ctrl+P. Lets you write tone-period values by note instead of by hex digit.

| Control | Type | Notes |
|---|---|---|
| Octave | `wxSpinCtrl` | 1–8, default 4 |
| Step | `wxSpinCtrl` | 0–256; frames the cursor advances after each note |
| Fill | `wxSpinCtrl` | 1–256; consecutive frames written per note; disabled and mirrors Step while "Link fill = step" is checked |
| Set T | `wxCheckBox` | When checked, also sets the tone-enable flag on written frames |
| Link fill = step | `wxCheckBox` | Checked by default |
| Set volume | `wxCheckBox` | When checked, also writes the Volume spinner's value; when unchecked, volume is left untouched |
| Volume | `wxSpinCtrl` | 0–15 |
| Freq. table | `wxChoice` | Soundtracker / Protracker / ASM or PSC / Real / SQ Tracker — 5 note→period lookup tables copied verbatim from the original |
| Keys | Custom-painted panel | 7 white keys (`Z X C V B N M`) + 5 black keys (`S D G H J`), labelled by their computer key, not note name |

Playing a note — by clicking a key or pressing its mapped computer key — writes the note's AY period into the bank at the main window's cursor and advances it, exactly like the original's `EnterFromPiano`. Holding **Shift** shifts the note's octave up one; **Ctrl** shifts it down one. Numpad 1–8 sets the octave directly.

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

The editor canvas uses a monospace font resolved via a fallback chain, body text at 14pt / column headers at 12pt:

```
Consolas → Menlo → DejaVu Sans Mono → Liberation Mono → Courier New
```

Each candidate is checked with `wxFontEnumerator::IsValidFacename()` — **not** `wxFont::IsOk()`, which on the macOS backend returns true even for a face that doesn't exist (Core Text silently substitutes something else instead of failing), which used to make the chain always "succeed" on Consolas and never reach Menlo there.

On Windows, **UbuntuMono-Regular** and **UbuntuMono-Bold** are loaded at startup from resources embedded in the `.exe` (IDs 101 and 102) via `AddFontMemResourceEx`. These are preferred for the canvas rendering.

Toolbar controls (buttons, the effect number/name fields) and the Piano window's controls use the platform's native default GUI font, unmodified — no custom size or face override.
