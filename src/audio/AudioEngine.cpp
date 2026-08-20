#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include "AudioEngine.h"
#include <algorithm>
#include <cstring>

static constexpr int kFrameRate = 50;

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

    m_blip.set_sample_rate(m_config.sampleRate);
    m_apu.output(&m_blip);
    m_apu.chip_type(m_config.chipType == AyChipType::Ym2149
                         ? Ay_Apu::Chip_Type::ym2149
                         : Ay_Apu::Chip_Type::ay_3_8910);
    // 0-100 maps linearly to Ay_Apu's own 0.0-1.0 scale. Measured empirically
    // (single full-amplitude channel A, the only channel this app ever
    // drives): clipping starts around v=1.15-1.2 on ym2149 (worse than
    // ay_3_8910's ~1.5-1.6, since its DAC table has a higher floor relative
    // to its ceiling), so mapping 100 -> 2.0 (the old "50 is Ay_Apu's own
    // default" scheme) clipped audibly at high slider settings. 100 -> 1.0
    // stays clip-free with real margin (no clipping seen up to v=1.1 in
    // testing) on both chips.
    m_apu.volume(m_config.volume / 100.0);
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
    m_apu.reset();
    m_blip.clear();

    const int clockRate = m_config.clockHz;
    m_blip.clock_rate(clockRate);
    const blip_time_t cyclesPerFrame = clockRate / kFrameRate;

    m_renderBuffer.clear();
    std::vector<blip_sample_t> mono;

    for (const auto& f : frames) {
        m_apu.write(0, 0, f.tone & 0xFF);
        m_apu.write(0, 1, (f.tone >> 8) & 0x0F);
        m_apu.write(0, 6, f.noise & 0x1F);

        // Mixer: bits 1,2,4,5 disable B/C tone+noise; bit0=tone-A-disable, bit3=noise-A-disable
        uint8_t mixer = 0b00110110;
        if (!f.toneEnable)  mixer |= 0x01;
        if (!f.noiseEnable) mixer |= 0x08;
        m_apu.write(0, 7, mixer);
        m_apu.write(0, 8, f.volume & 0x0F);

        m_apu.end_frame(cyclesPerFrame);
        m_blip.end_frame(cyclesPerFrame);

        const long avail = m_blip.samples_avail();
        mono.resize(avail);
        m_blip.read_samples(mono.data(), avail);
        for (blip_sample_t s : mono) {
            m_renderBuffer.push_back(s);
            m_renderBuffer.push_back(s);  // mono -> stereo
        }
    }

    // Anti-click: linearly fade the last ~8ms to silence instead of cutting
    // straight to the trailing zeros below. The AY/YM DAC output is never
    // exactly zero-centred, so even after Blip_Buffer's own high-pass
    // filtering an instant stop can leave a small but audible pop,
    // especially right after a loud, sustained tone.
    const int fadeFrames = std::min(static_cast<int>(m_renderBuffer.size() / 2),
                                     m_config.sampleRate * 8 / 1000);
    const int fadeStart = static_cast<int>(m_renderBuffer.size()) - fadeFrames * 2;
    for (int i = 0; i < fadeFrames; ++i) {
        const double gain = static_cast<double>(fadeFrames - i) / fadeFrames;
        const int idx = fadeStart + i * 2;
        m_renderBuffer[idx]     = static_cast<int16_t>(m_renderBuffer[idx] * gain);
        m_renderBuffer[idx + 1] = static_cast<int16_t>(m_renderBuffer[idx + 1] * gain);
    }

    // ~100ms trailing silence so last note decays cleanly
    int silence = (m_config.sampleRate / 10) * 2;
    m_renderBuffer.insert(m_renderBuffer.end(), silence, int16_t{0});
}

std::vector<int16_t> AudioEngine::renderToPcm(const std::vector<FrameData>& frames) {
    if (frames.empty()) return {};
    renderFrames(frames);
    return m_renderBuffer;
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
