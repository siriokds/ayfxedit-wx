#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <atomic>
#include <memory>
#include "Ay_Apu.h"
#include "Blip_Buffer.h"
#include "miniaudio.h"

static constexpr int kAudioSampleRate     = 48000;

// Well-known AY/YM PSG clock rates, in Hz. Clock rate is a property of the
// machine's own bus/crystal, independent of which chip brand (AY-3-8910 vs
// YM2149) happens to be installed -- e.g. MSX machines run at 3.579545 MHz
// regardless of chip brand. These are UI presets (see MainFrame's Audio
// Settings dialog); AudioConfig itself just stores the resulting Hz value,
// so a user can also dial in an arbitrary clock.
static constexpr int kAYClockRateSpectrum = 1773400;    // ZX Spectrum 128/+2/+3 (AY-3-8912)
static constexpr int kAYClockRateMsx      = 1789772;    // MSX: NTSC colourburst 3.579545MHz / 2
                                                          // (AY-3-8910 is only rated to 2MHz)
static constexpr int kAYClockRateCpc      = 1000000;    // Amstrad CPC (AY-3-8912)
static constexpr int kAYClockRateAtariSt  = 2000000;    // Atari ST (YM2149)

enum class AyChipType { Ay38910, Ym2149 };

struct AudioConfig {
    bool         useDefaultDevice = true;
    ma_device_id deviceId{};       // valid only when useDefaultDevice == false
    int          sampleRate = kAudioSampleRate;
    int          volume = 50;  // 0-100
    AyChipType   chipType = AyChipType::Ay38910;      // DAC/envelope characteristics
    int          clockHz  = kAYClockRateSpectrum;     // PSG clock rate
};

class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    bool initialize(const AudioConfig& cfg = {});
    void shutdown();
    void reconfigure(const AudioConfig& cfg);

    // Returns list of available audio output device names.
    static std::vector<std::string> enumerateDevices();

    struct FrameData {
        uint16_t tone;
        uint8_t  noise;
        uint8_t  volume;
        bool     toneEnable;
        bool     noiseEnable;
    };
    void play(const std::vector<FrameData>& frames);
    void stop();
    bool isPlaying() const;

    // Renders frames exactly as play() would (same chip/clock/volume, same
    // anti-click fade and trailing silence) but returns the interleaved
    // stereo 16-bit PCM instead of playing it, for exporting to a file.
    std::vector<int16_t> renderToPcm(const std::vector<FrameData>& frames);

    const AudioConfig& config() const { return m_config; }

private:
    static void dataCallback(ma_device* device, void* output, const void* input, ma_uint32 frameCount);
    void fillOutput(int16_t* output, ma_uint32 frameCount);
    void renderFrames(const std::vector<FrameData>& frames);

    Ay_Apu                     m_apu;
    Blip_Buffer                m_blip;
    std::unique_ptr<ma_device> m_device;
    AudioConfig                m_config;

    std::vector<int16_t> m_renderBuffer;
    std::atomic<int>     m_readPos{0};
    std::atomic<bool>    m_playing{false};
};
