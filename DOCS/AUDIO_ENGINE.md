# Audio Engine

## Overview

Audio playback is split into two classes:

- **`AY8910`** — cycle-accurate emulator of the AY-3-8910 chip
- **`AudioEngine`** — miniaudio wrapper that drives the emulator and streams PCM output

## AY8910 Emulator (`src/audio/AY8910.h/.cpp`)

Derived from the Gearcoleco ColecoVision emulator (GPL-3.0, copyright 2026 Saverio Russo).

### Chip parameters

| Parameter | Value |
|---|---|
| Clock rate | 1,773,400 Hz (ZX Spectrum standard) |
| Default sample rate | 48,000 Hz |
| Supported sample rates | 22050 / 44100 / 48000 / 96000 Hz |
| Internal PCM buffer | 4096 stereo frames = 8192 × int16_t |
| Output format | Stereo S16 (mono summed, duplicated to both channels) |

### Emulated hardware

| Component | Implementation |
|---|---|
| Tone generators | Channels A, B, C — 12-bit period counter, sign-flip oscillator |
| Noise generator | 17-bit LFSR with XOR feedback (bits 0 and 3) |
| Envelope generator | 16-step, 8 shapes, re-triggered on register 13 write |
| Mixer | Per-channel tone/noise enable (active-low, matches real chip register 7) |
| Volume | 4-bit per channel or envelope mode (bit 4 of registers 8–10) |
| Volume curve | Non-linear `kAY8910VolumeTable[16]` mapping to int16 amplitudes (0–4096 per channel) |

### Key methods

| Method | Description |
|---|---|
| `Init(clockRate, sampleRate)` | Allocates PCM buffer, resets chip state |
| `Reset()` | Zeroes all register and oscillator state |
| `SelectRegister(reg)` | Sets the active register latch (0–15) |
| `WriteRegister(value)` | Writes value to the latched register |
| `WriteReg(reg, value)` | Convenience: SelectRegister + WriteRegister |
| `Tick(clockCycles)` | Accumulates elapsed clock cycles |
| `Sync()` | Runs the emulation loop, writing S16 stereo samples to the internal buffer |
| `EndFrame(pSampleBuffer)` | Flushes the buffer, returns the sample count |

### Playback note

During playback, `AudioEngine` uses **channel A only**. Channels B and C are kept silent by setting mixer register 7 with bitmask `0b00110110` (B and C tone+noise disabled).

## AudioEngine (`src/audio/AudioEngine.h/.cpp`)

### `AudioConfig` struct

```cpp
struct AudioConfig {
    bool         useDefaultDevice = true;
    ma_device_id deviceId{};       // valid only when useDefaultDevice == false
    int          sampleRate = 48000;
    int          volume     = 50;      // 0–100
};
```

### Lifecycle

```
initialize(cfg)
    → ma_device_init(nullptr, &deviceConfig, ...)  with dataCallback
    → AY8910::Init(kAYClock, cfg.sampleRate)

play(frames)
    → AY8910::Reset()
    → for each frame: WriteReg × 4 + Tick(kAYClock/50) + EndFrame → append to m_renderBuffer
    → append ~100 ms trailing silence
    → ma_device_start()

dataCallback / fillOutput
    → pulls from m_renderBuffer using atomic read position, zero-fills the tail
    → sets m_playing = false when exhausted

reconfigure(cfg)
    → shutdown() + initialize(cfg)

shutdown()
    → ma_device_uninit()
```

### Volume scaling

The AY chip sums three channels each with a max amplitude of 4096, giving a theoretical peak of 12288. The raw S16 output is scaled by:

```
scale = volume * 6 / 100
```

At `volume = 50` this gives `scale = 3`, mapping the 12288 peak to 36864 — within the S16 range of 32767. At `volume = 100` clipping is possible on loud effects.

### Device enumeration

`enumerateDevices()` opens a throwaway `ma_context`, calls `ma_context_get_devices()`, and returns a `std::vector<AudioDeviceInfo>` (name + `ma_device_id`) of playback devices. Used to populate the device selector in Preferences → Audio → Output device; the persisted device *name* is resolved back to a real `ma_device_id` by a fresh enumeration at apply time (`BuildAudioConfig` in `MainFrame.cpp`), falling back to the default device if the named one is no longer present.

`supportedSampleRates(deviceId)` queries `ma_context_get_device_info()`'s `nativeDataFormats[]` to filter the Sample rate choice down to what the selected device actually supports (out of 22050/44100/48000/96000 Hz) — some backends (WASAPI in particular) only support a native rate or a small set, while others (CoreAudio) report `sampleRate == 0` meaning "any rate", in which case all four are offered.
