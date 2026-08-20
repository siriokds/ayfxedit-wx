#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include "AudioEngine.h"
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

    ma_device_config deviceConfig    = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format     = ma_format_s16;
    deviceConfig.playback.channels   = 2;
    deviceConfig.sampleRate          = static_cast<ma_uint32>(m_config.sampleRate);
    deviceConfig.dataCallback        = &AudioEngine::dataCallback;
    deviceConfig.pUserData           = this;
    if (!m_config.useDefaultDevice) {
        deviceConfig.playback.pDeviceID = &m_config.deviceId;
    }

    m_device = std::make_unique<ma_device>();
    if (ma_device_init(nullptr, &deviceConfig, m_device.get()) != MA_SUCCESS) {
        m_device.reset();
        return false;
    }

    m_chip.Init(kAYClock, m_config.sampleRate);
    return true;
}

void AudioEngine::shutdown() {
    stop();
    if (m_device) {
        ma_device_uninit(m_device.get());
        m_device.reset();
    }
}

void AudioEngine::reconfigure(const AudioConfig& cfg) {
    shutdown();
    initialize(cfg);
}

std::vector<std::string> AudioEngine::enumerateDevices() {
    std::vector<std::string> result;

    ma_context context;
    if (ma_context_init(nullptr, 0, nullptr, &context) != MA_SUCCESS) return result;

    ma_device_info* playbackInfos;
    ma_uint32       playbackCount;
    if (ma_context_get_devices(&context, &playbackInfos, &playbackCount, nullptr, nullptr) == MA_SUCCESS) {
        for (ma_uint32 i = 0; i < playbackCount; ++i) {
            result.push_back(playbackInfos[i].name);
        }
    }

    ma_context_uninit(&context);
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
    if (frames.empty() || !m_device) return;
    stop();
    renderFrames(frames);
    m_readPos.store(0);
    m_playing.store(true);
    ma_device_start(m_device.get());
}

void AudioEngine::stop() {
    m_playing.store(false);
    if (m_device) {
        ma_device_stop(m_device.get());
    }
}

bool AudioEngine::isPlaying() const {
    return m_playing.load();
}

void AudioEngine::dataCallback(ma_device* device, void* output, const void* /*input*/, ma_uint32 frameCount) {
    static_cast<AudioEngine*>(device->pUserData)->fillOutput(static_cast<int16_t*>(output), frameCount);
}

void AudioEngine::fillOutput(int16_t* output, ma_uint32 frameCount) {
    const int needed = static_cast<int>(frameCount) * 2;  // stereo

    if (!m_playing.load()) {
        std::memset(output, 0, needed * sizeof(int16_t));
        return;
    }

    int readPos = m_readPos.load();
    int avail   = static_cast<int>(m_renderBuffer.size()) - readPos;

    if (avail <= 0) {
        m_playing.store(false);
        std::memset(output, 0, needed * sizeof(int16_t));
        return;
    }

    int toCopy = std::min(needed, avail);
    std::memcpy(output, m_renderBuffer.data() + readPos, toCopy * sizeof(int16_t));
    if (toCopy < needed) {
        std::memset(output + toCopy, 0, (needed - toCopy) * sizeof(int16_t));
    }
    m_readPos.store(readPos + toCopy);

    if (readPos + toCopy >= static_cast<int>(m_renderBuffer.size())) {
        m_playing.store(false);
    }
}
