#pragma once

#include <wx/dialog.h>

// Preferences window: General / Appearance / Audio (Engine, Output device),
// modeled (in reduced form) on the settings dialog in the user's other
// project, Dual. Reached via wxID_PREFERENCES (see MainFrame::createMenu).
class SettingsDialog : public wxDialog {
public:
    explicit SettingsDialog(wxWindow* parent);
};
