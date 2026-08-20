# Data Formats

## In-memory structures

### `AyfxCell` — one frame of a sound effect

```cpp
struct AyfxCell {
    uint16_t tone        = 0;     // 12-bit tone period (0–4095)
    uint8_t  noise       = 0;     // 5-bit noise period (0–31)
    uint8_t  volume      = 0;     // 4-bit volume (0–15)
    bool     toneEnable  = false;
    bool     noiseEnable = false;
    bool     selected    = false; // UI state only, not serialised
};
```

### `AyfxEffect` — one sound effect

```cpp
struct AyfxEffect {
    std::string         name;    // up to ~255 chars, NUL-terminated in file
    std::vector<AyfxCell> frames; // always kMaxFrames (4096) allocated
};
```

The "real length" of an effect is determined at save time as the index of the last frame where `volume > 0`.

---

## Single effect file format — `.afx`

Variable-length binary. Each frame is encoded as one **flag byte** plus zero, one, or two optional data bytes.

### Flag byte layout

| Bits | Field |
|---|---|
| 3:0 | Volume (0–15) |
| 4 | Tone disabled: `1` = tone off |
| 5 | Tone period follows: `1` = next 2 bytes (LE) carry the 12-bit tone period |
| 6 | Noise period follows: `1` = next byte carries the 5-bit noise period |
| 7 | Noise disabled: `1` = noise off |

If bit 5 is clear, the tone period is unchanged from the previous frame (delta compression).  
If bit 6 is clear, the noise period is unchanged from the previous frame.

### End-of-effect sentinel

```
0xD0  0x20
```

This 2-byte sequence terminates the frame stream.

### Decode summary

```
prevTone  = 0
prevNoise = 0
loop:
    read flagByte
    if flagByte == 0xD0 and next == 0x20 → end
    volume      = flagByte & 0x0F
    toneEnable  = !(flagByte & 0x10)
    noiseEnable = !(flagByte & 0x80)
    if flagByte & 0x20: read uint16_le → tone; prevTone = tone
    else:               tone = prevTone
    if flagByte & 0x40: read uint8 → noise; prevNoise = noise
    else:               noise = prevNoise
    emit AyfxCell{tone, noise, volume, toneEnable, noiseEnable}
```

---

## Bank file format — `.afb`

A bank packs multiple effects with a Z80-friendly self-relative offset table.

### File layout

```
Offset 0:       N  (1 byte)  — effect count; 0 means 256
Offset 1...:    offset table — N × 2-byte little-endian entries
                Each value is relative to the second byte of that entry.
                i.e. absolute_offset = table_entry_address + 1 + value
Then:           effect data blocks (variable length, .afx format each)
Optionally:     NUL-terminated name string per effect, appended after each block
                (omitted when saved with "Save bank without names")
```

### Offset table note

The self-relative addressing is inherited from the original Z80 player where the table is accessed directly from assembly code. The loader in `BankModel` reconstructs absolute file offsets from the relative values.

---

## AudioEngine::FrameData — transient playback struct

Used only during `AudioEngine::play()` to pass decoded frame data to the AY chip:

```cpp
struct FrameData {
    uint16_t tone;
    uint8_t  noise;
    uint8_t  volume;
    bool     toneEnable;
    bool     noiseEnable;
};
```

This struct is never written to disk.
