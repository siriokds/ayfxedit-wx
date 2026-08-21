#pragma once

#include <filesystem>
#include <string>

// Persisted app-wide preferences (General/Appearance/Audio settings page).
// Distinct from AudioConfig (src/audio/AudioEngine.h), which is the live,
// in-memory state the audio engine actually runs with -- Settings is what
// gets loaded at startup and saved back, AudioConfig is populated from it.
struct Settings {
    enum class Theme { FollowOS, Light, Dark };

    // General
    bool saveOnExit = true;
    bool singleInstance = false;
    bool confirmDeleteEffect = true;

    // Appearance
    Theme theme = Theme::FollowOS;
    std::string uiFontFamily;    // empty = system default
    int uiFontSize = 0;          // 0 = system default
    std::string monoFontFamily;  // empty = built-in fallback list
    int monoFontSize = 0;        // 0 = built-in default
    int rowPaddingPx = 4;        // added on top of the monospace font's own line height

    // Audio
    std::string chipType = "ay38910";  // "ay38910" | "ym2149"
    int clockHz = 1773400;
    int lowpassHz = 0;
    int sampleRate = 48000;
    int volume = 50;
    std::string outputDevice;  // empty = default device
};

// Where the settings file lives (per-OS user data dir via wxStandardPaths).
std::filesystem::path SettingsFilePath();

// Returns defaults if the file doesn't exist or fails to parse.
Settings LoadSettings();

// Writes atomically (temp file + rename). Returns false on failure.
bool SaveSettings(const Settings& settings);
