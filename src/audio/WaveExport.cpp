#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"
#include "WaveExport.h"

bool WriteWaveFile(const std::string& filePath, const std::vector<int16_t>& stereoPcm, int sampleRate) {
    if (stereoPcm.empty() || stereoPcm.size() % 2 != 0) {
        return false;
    }

    drwav_data_format format{};
    format.container    = drwav_container_riff;
    format.format        = DR_WAVE_FORMAT_PCM;
    format.channels       = 2;
    format.sampleRate     = static_cast<drwav_uint32>(sampleRate);
    format.bitsPerSample  = 16;

    drwav wav;
    if (!drwav_init_file_write(&wav, filePath.c_str(), &format, nullptr)) {
        return false;
    }

    const drwav_uint64 frameCount = stereoPcm.size() / 2;
    const drwav_uint64 written = drwav_write_pcm_frames(&wav, frameCount, stereoPcm.data());
    drwav_uninit(&wav);
    return written == frameCount;
}
