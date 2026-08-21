#pragma once

#include <filesystem>
#include <string>

// Plain-data window geometry, persisted across sessions -- captured/restored
// by the wx-touching code in src/app/WindowGeometry.h/.cpp (kept out of this
// core/ header so Settings itself stays free of any wx dependency).
struct WindowGeometry {
    int x = 0, y = 0, w = 0, h = 0;
    bool maximized = false;
    // Distinguishes "no position ever saved" (e.g. upgrading from a version
    // predating this feature) from a legitimately saved (0, 0) -- without
    // it, a fresh install would look identical to "saved at the origin".
    bool posValid = false;
};

// Persisted app-wide preferences (General/Appearance/Audio settings page).
// Distinct from AudioConfig (src/audio/AudioEngine.h), which is the live,
// in-memory state the audio engine actually runs with -- Settings is what
// gets loaded at startup and saved back, AudioConfig is populated from it.
struct Settings {
    // General
    // Settings are always persisted immediately on OK -- no "save on exit"
    // toggle; this is just ~15 preference fields, not worth a separate
    // on/off switch for. Window geometry (below) is the one exception,
    // saved on close rather than immediately, since it isn't edited through
    // this dialog.
    bool singleInstance = false;
    bool confirmDeleteEffect = true;

    // Window position/size, saved on close, restored at startup.
    WindowGeometry windowGeometry;

    // Appearance
    // No explicit Light/Dark override: wxWidgets 3.3's SetAppearance() only
    // takes effect if called before any window is created, so it can't be
    // switched live from Preferences without a restart -- not worth the
    // restart-required UX for a "force theme" feature. Always follows the OS.
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
