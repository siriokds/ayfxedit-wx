#pragma once

#include <wx/dialog.h>

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
    void onOK(wxCommandEvent& event);

    Settings settings_;

    class wxCheckBox* singleInstanceCheck_ = nullptr;
    class wxCheckBox* confirmDeleteCheck_ = nullptr;
};
