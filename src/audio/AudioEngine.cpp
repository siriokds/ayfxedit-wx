#include "AudioEngine.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cstring>

static constexpr int kAYClock    = 1773400;
static constexpr int kFrameRate  = 50;

AudioEngine::AudioEngine() {}

AudioEngine::~AudioEngine() {
    shutdown();
}

bool AudioEngine::initialize(const AudioConfig& cfg) {
    m_config = cfg;
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        return false;
    }

    SDL_AudioSpec spec{};
    spec.format   = SDL_AUDIO_S16;
    spec.channels = 2;
    spec.freq     = m_config.sampleRate;

    m_stream = SDL_OpenAudioDeviceStream(
        m_config.deviceId, &spec, audioCallback, this);
    if (!m_stream) return false;

    m_chip.Init(kAYClock, m_config.sampleRate);
    return true;
}

void AudioEngine::shutdown() {
    stop();
    if (m_stream) {
        SDL_DestroyAudioStream(m_stream);
        m_stream = nullptr;
    }
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

void AudioEngine::reconfigure(const AudioConfig& cfg) {
    bool wasPlaying = isPlaying();
    shutdown();
    initialize(cfg);
    (void)wasPlaying;
}

std::vector<std::string> AudioEngine::enumerateDevices() {
    std::vector<std::string> result;
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) return result;

    int count = 0;
    SDL_AudioDeviceID* ids = SDL_GetAudioPlaybackDevices(&count);
    if (ids) {
        for (int i = 0; i < count; ++i) {
            const char* name = SDL_GetAudioDeviceName(ids[i]);
            if (name) result.push_back(name);
        }
        SDL_free(ids);
    }
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    return result;
}

void AudioEngine::renderFrames(const std::vector<FrameData>& frames) {
    m_chip.Reset(kAYClock, m_config.sampleRate);
    m_renderBuffer.clear();

    for (const auto& f : frames) {
        m_chip.WriteReg(0, f.tone & 0xFF);
        m_chip.WriteReg(1, (f.tone >> 8) & 0x0F);
        m_chip.WriteReg(6, f.noise & 0x1F);

        // Mixer: bits 1,2,4,5 disable B/C tone+noise; bit0=tone-A-disable, bit3=noise-A-disable
        uint8_t mixer = 0b00110110;
        if (!f.toneEnable)  mixer |= 0x01;
        if (!f.noiseEnable) mixer |= 0x08;
        m_chip.WriteReg(7, mixer);
        m_chip.WriteReg(8, f.volume & 0x0F);

        m_chip.Tick(kAYClock / kFrameRate);

        int16_t frameBuf[kAudioBufferSize];
        int count = m_chip.EndFrame(frameBuf);
        m_renderBuffer.insert(m_renderBuffer.end(), frameBuf, frameBuf + count);
    }

    // Volume scale: AY max per channel = 4096, S16 max = 32767.
    // volume 100 → scale ~6 (~75% headroom); volume 50 → scale ~3.
    const int scale = (m_config.volume * 6 + 50) / 100;
    if (scale != 1) {
        for (auto& s : m_renderBuffer) {
            s = static_cast<int16_t>(std::clamp(static_cast<int>(s) * scale, -32768, 32767));
        }
    }

    // ~100ms trailing silence so last note decays cleanly
    int silence = (m_config.sampleRate / 10) * 2;
    m_renderBuffer.insert(m_renderBuffer.end(), silence, int16_t{0});
}

void AudioEngine::play(const std::vector<FrameData>& frames) {
    if (frames.empty() || !m_stream) return;
    stop();
    renderFrames(frames);
    m_readPos.store(0);
    m_playing.store(true);
    SDL_ResumeAudioStreamDevice(m_stream);
}

void AudioEngine::stop() {
    m_playing.store(false);
    if (m_stream) {
        SDL_PauseAudioStreamDevice(m_stream);
        SDL_ClearAudioStream(m_stream);
    }
}

bool AudioEngine::isPlaying() const {
    return m_playing.load();
}

void SDLCALL AudioEngine::audioCallback(void* userdata, SDL_AudioStream* stream,
                                         int additional_bytes, int /*total_bytes*/) {
    static_cast<AudioEngine*>(userdata)->fillStream(stream, additional_bytes);
}

void AudioEngine::fillStream(SDL_AudioStream* stream, int additional_bytes) {
    if (!m_playing.load()) return;

    int needed  = additional_bytes / static_cast<int>(sizeof(int16_t));
    int readPos = m_readPos.load();
    int avail   = static_cast<int>(m_renderBuffer.size()) - readPos;

    if (avail <= 0) {
        m_playing.store(false);
        return;
    }

    int toCopy = std::min(needed, avail);
    SDL_PutAudioStreamData(stream,
        m_renderBuffer.data() + readPos,
        toCopy * static_cast<int>(sizeof(int16_t)));
    m_readPos.store(readPos + toCopy);

    if (readPos + toCopy >= static_cast<int>(m_renderBuffer.size())) {
        m_playing.store(false);
    }
}
