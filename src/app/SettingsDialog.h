#pragma once

#include <wx/dialog.h>

#include <vector>

#include "../audio/AudioEngine.h"
#include "../core/Settings.h"

// Preferences window: General / Appearance / Audio (Engine, Output device),
// modeled (in reduced form) on the settings dialog in the user's other
// project, Dual. Reached via wxID_PREFERENCES (see MainFrame::createMenu).
//
// Works on a copy of the passed-in Settings (matching Dual's own dialog):
// OK writes the edited copy back via result(), Cancel just discards it.
class SettingsDialog : public wxDialog {
public:
    SettingsDialog(wxWindow* parent, const Settings& initial);

    const Settings& result() const { return settings_; }

private:
    wxWindow* createGeneralPage(wxWindow* parent);
    wxWindow* createEnginePage(wxWindow* parent);
    wxWindow* createOutputDevicePage(wxWindow* parent);
    void onOK(wxCommandEvent& event);

    // Repopulates sampleRateChoice_ with the rates the currently selected
    // device actually supports, preferring (in order) the rate previously
    // selected, else 48000, else 44100, else whatever is first available.
    void refreshSampleRatesForSelectedDevice();

    Settings settings_;

    // General
    class wxCheckBox* singleInstanceCheck_ = nullptr;
    class wxCheckBox* confirmDeleteCheck_ = nullptr;

    // Audio > Engine
    class wxChoice* chipChoice_ = nullptr;
    class wxChoice* machineChoice_ = nullptr;
    class wxSpinCtrl* clockSpin_ = nullptr;
    class wxCheckBox* lowpassCheck_ = nullptr;
    class wxSpinCtrl* lowpassSpin_ = nullptr;

    // Audio > Output device
    class wxChoice* deviceChoice_ = nullptr;
    class wxChoice* sampleRateChoice_ = nullptr;
    class wxSlider* volumeSlider_ = nullptr;
    std::vector<AudioDeviceInfo> devices_;
    // Rates currently populated in sampleRateChoice_, in display order --
    // its selection index is only meaningful against this list, since it's
    // filtered down to what the selected device actually supports.
    std::vector<int> currentSampleRates_;
};
