#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Writes interleaved stereo 16-bit PCM (as produced by
// AudioEngine::renderToPcm) to a standard RIFF WAV file at filePath.
// Returns false on failure (bad path, no write permission, empty buffer).
bool WriteWaveFile(const std::string& filePath, const std::vector<int16_t>& stereoPcm, int sampleRate);
