#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <atomic>
#include "AY8910.h"
#include <SDL3/SDL.h>

struct AudioConfig {
    SDL_AudioDeviceID deviceId = SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK;
    int sampleRate = kAudioSampleRate;
    int volume = 50;  // 0-100
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
    static void SDLCALL audioCallback(void* userdata, SDL_AudioStream* stream, int additional_bytes, int total_bytes);
    void fillStream(SDL_AudioStream* stream, int additional_bytes);
    void renderFrames(const std::vector<FrameData>& frames);

    AY8910            m_chip;
    SDL_AudioStream*  m_stream  = nullptr;
    AudioConfig       m_config;

    std::vector<int16_t> m_renderBuffer;
    std::atomic<int>     m_readPos{0};
    std::atomic<bool>    m_playing{false};
};
