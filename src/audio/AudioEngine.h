#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <atomic>
#include <memory>
#include "AY8910.h"
#include "miniaudio.h"

struct AudioConfig {
    bool         useDefaultDevice = true;
    ma_device_id deviceId{};       // valid only when useDefaultDevice == false
    int          sampleRate = kAudioSampleRate;
    int          volume = 50;  // 0-100
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

    const AudioConfig& config() const { return m_config; }

private:
    static void dataCallback(ma_device* device, void* output, const void* input, ma_uint32 frameCount);
    void fillOutput(int16_t* output, ma_uint32 frameCount);
    void renderFrames(const std::vector<FrameData>& frames);

    AY8910                     m_chip;
    std::unique_ptr<ma_device> m_device;
    AudioConfig                m_config;

    std::vector<int16_t> m_renderBuffer;
    std::atomic<int>     m_readPos{0};
    std::atomic<bool>    m_playing{false};
};
