#pragma once

#include <wx/dialog.h>

#include <functional>
#include <vector>

#include "../audio/AudioEngine.h"
#include "../core/Settings.h"

// Preferences window: General / Appearance / Audio (Engine, Output device),
// modeled (in reduced form) on the settings dialog in the user's other
// project, Dual. Reached via wxID_PREFERENCES (see MainFrame::createMenu).
//
// Works on a copy of the passed-in Settings (matching Dual's own dialog):
// OK writes the edited copy back via result(), Cancel just discards it.
//
// Appearance changes (theme/fonts) are the exception to "only apply on OK":
// they're applied live as the user picks them, via onLiveAppearanceChange,
// so the main window updates as a preview before the user commits. Cancel
// (button, Escape, or the title bar close box) reverts that preview back to
// the settings the dialog was opened with.
class SettingsDialog : public wxDialog {
public:
    SettingsDialog(wxWindow* parent, const Settings& initial,
                   std::function<void(const Settings&)> onLiveAppearanceChange = {});

    const Settings& result() const { return settings_; }

private:
    wxWindow* createGeneralPage(wxWindow* parent);
    wxWindow* createAppearancePage(wxWindow* parent);
    wxWindow* createEnginePage(wxWindow* parent);
    wxWindow* createOutputDevicePage(wxWindow* parent);
    void onOK(wxCommandEvent& event);
    void onCancel(wxCommandEvent& event);
    void onClose(wxCloseEvent& event);

    // Reads theme/font controls into settings_, with no side effects.
    void readAppearanceFieldsIntoSettings();
    // readAppearanceFieldsIntoSettings(), then invokes onLiveAppearanceChange_
    // (deferred -- see the .cpp) so the main window previews the change now.
    void applyAppearanceLive();

    Settings settings_;
    const Settings initial_;
    std::function<void(const Settings&)> onLiveAppearanceChange_;

    // Repopulates sampleRateChoice_ with the rates the currently selected
    // device actually supports, preferring (in order) the rate previously
    // selected, else 48000, else 44100, else whatever is first available.
    void refreshSampleRatesForSelectedDevice();

    // General
    class wxCheckBox* singleInstanceCheck_ = nullptr;
    class wxCheckBox* confirmDeleteCheck_ = nullptr;

    // Appearance
    class wxComboBox* uiFontCombo_ = nullptr;
    class wxSpinCtrl* uiFontSizeSpin_ = nullptr;
    class wxComboBox* monoFontCombo_ = nullptr;
    class wxSpinCtrl* monoFontSizeSpin_ = nullptr;

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
