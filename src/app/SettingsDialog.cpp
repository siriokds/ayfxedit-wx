#include "SettingsDialog.h"

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/sizer.h>
#include <wx/slider.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>
#include <wx/treebook.h>
#include <wx/treectrl.h>

#include "../audio/AudioEngine.h"
#include "ClockPicker.h"

namespace {

// Placeholder page content -- filled in as Appearance's real controls are
// built next.
wxPanel* MakeStubPage(wxWindow* parent, const wxString& label) {
    auto* panel = new wxPanel(parent);
    auto* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->AddStretchSpacer();
    sizer->Add(new wxStaticText(panel, wxID_ANY, label), 0, wxALIGN_CENTER);
    sizer->AddStretchSpacer();
    panel->SetSizer(sizer);
    return panel;
}

}  // namespace

SettingsDialog::SettingsDialog(wxWindow* parent, const Settings& initial)
    : wxDialog(parent, wxID_ANY, "Preferences", wxDefaultPosition, wxDefaultSize,
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      settings_(initial) {
    auto* book = new wxTreebook(this, wxID_ANY);
    book->AddPage(createGeneralPage(book), "General");
    book->AddPage(MakeStubPage(book, "Appearance (stub)"), "Appearance");
    book->AddPage(MakeStubPage(book, "Audio"), "Audio");
    book->AddSubPage(createEnginePage(book), "Engine");
    book->AddSubPage(createOutputDevicePage(book), "Output device");

    // Wide enough for the sidebar's longest label ("Output device") without
    // ellipsis; tall enough that all 5 rows (General/Appearance/Audio/
    // Engine/Output device) fit without the tree ctrl growing a vertical
    // scrollbar -- that scrollbar is what triggers the white-square
    // wxTreeCtrl rendering glitch, so avoiding it entirely sidesteps the bug.
    book->GetTreeCtrl()->SetMinSize(FromDIP(wxSize(160, 160)));

    auto* vs = new wxBoxSizer(wxVERTICAL);
    vs->Add(book, 1, wxEXPAND | wxALL, 8);
    vs->Add(CreateStdDialogButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxALL, 8);
    SetSizerAndFit(vs);
    SetMinSize(FromDIP(wxSize(700, 520)));
    SetSize(FromDIP(wxSize(700, 520)));
    CentreOnScreen();

    Bind(wxEVT_BUTTON, &SettingsDialog::onOK, this, wxID_OK);
}

wxWindow* SettingsDialog::createGeneralPage(wxWindow* parent) {
    auto* panel = new wxPanel(parent);
    auto* sizer = new wxBoxSizer(wxVERTICAL);

    singleInstanceCheck_ = new wxCheckBox(panel, wxID_ANY, "Single instance");
    singleInstanceCheck_->SetValue(settings_.singleInstance);
    singleInstanceCheck_->SetToolTip("Refuse to launch a second copy of the app while one is already running.");

    confirmDeleteCheck_ = new wxCheckBox(panel, wxID_ANY, "Confirm delete effect");
    confirmDeleteCheck_->SetValue(settings_.confirmDeleteEffect);

    sizer->Add(singleInstanceCheck_, 0, wxALL, 8);
    sizer->Add(confirmDeleteCheck_, 0, wxLEFT | wxRIGHT | wxBOTTOM, 8);
    panel->SetSizer(sizer);
    return panel;
}

wxWindow* SettingsDialog::createEnginePage(wxWindow* parent) {
    auto* panel = new wxPanel(parent);

    // Chip type: same 4-bit (16-level) per-channel volume register on both,
    // but AY-3-8910's envelope generator has 16 steps vs YM2149's 32 (see
    // Ay_Apu::build_env_modes_()) plus a different per-channel DAC table.
    // Independent of clock rate below -- e.g. MSX machines run at the same
    // clock whether they have a genuine AY-3-8910 or a YM2149 installed.
    auto* chipLabel = new wxStaticText(panel, wxID_ANY, "Chip:");
    chipChoice_ = new wxChoice(panel, wxID_ANY);
    chipChoice_->Append("AY-3-8910");
    chipChoice_->Append("YM2149");
    chipChoice_->SetSelection(settings_.chipType == "ym2149" ? 1 : 0);

    // Machine presets just fill in the clock field; picking "Custom" (or
    // hand-editing the clock) leaves it to the user.
    auto* machineLabel = new wxStaticText(panel, wxID_ANY, "Machine:");
    auto clockPicker = CreateClockPicker(panel, settings_.clockHz);
    machineChoice_ = clockPicker.machineChoice;
    clockSpin_ = clockPicker.clockSpin;
    auto* clockLabel = new wxStaticText(panel, wxID_ANY, "Clock (Hz):");

    // Extra low-pass, on top of whatever the output rate's own Nyquist
    // already implies -- e.g. to approximate a real machine's analog
    // output stage rolling off high frequencies. Off (0) by default: the
    // resampler's own Nyquist-based cutoff already removes anything truly
    // ultrasonic.
    lowpassCheck_ = new wxCheckBox(panel, wxID_ANY, "Extra low-pass filter:");
    lowpassCheck_->SetValue(settings_.lowpassHz > 0);
    lowpassSpin_ = new wxSpinCtrl(panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
                                  wxSP_ARROW_KEYS, 1000, 40000,
                                  settings_.lowpassHz > 0 ? settings_.lowpassHz : 18000);
    lowpassSpin_->Enable(lowpassCheck_->GetValue());
    lowpassCheck_->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) {
        lowpassSpin_->Enable(lowpassCheck_->GetValue());
    });

    auto* grid = new wxFlexGridSizer(4, 2, 6, 10);
    grid->AddGrowableCol(1, 1);
    grid->Add(chipLabel, 0, wxALIGN_CENTER_VERTICAL);
    grid->Add(chipChoice_, 1, wxEXPAND);
    grid->Add(machineLabel, 0, wxALIGN_CENTER_VERTICAL);
    grid->Add(machineChoice_, 1, wxEXPAND);
    grid->Add(clockLabel, 0, wxALIGN_CENTER_VERTICAL);
    grid->Add(clockSpin_, 1, wxEXPAND);
    grid->Add(lowpassCheck_, 0, wxALIGN_CENTER_VERTICAL);
    grid->Add(lowpassSpin_, 1, wxEXPAND);

    auto* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(grid, 0, wxEXPAND | wxALL, 12);
    panel->SetSizer(sizer);
    return panel;
}

wxWindow* SettingsDialog::createOutputDevicePage(wxWindow* parent) {
    auto* panel = new wxPanel(parent);

    devices_ = AudioEngine::enumerateDevices();

    auto* devLabel = new wxStaticText(panel, wxID_ANY, "Output device:");
    deviceChoice_ = new wxChoice(panel, wxID_ANY);
    deviceChoice_->Append("Default");
    for (const auto& d : devices_) deviceChoice_->Append(d.name);
    int devSel = 0;
    for (std::size_t i = 0; i < devices_.size(); ++i) {
        if (devices_[i].name == settings_.outputDevice) { devSel = static_cast<int>(i) + 1; break; }
    }
    deviceChoice_->SetSelection(devSel);

    auto* rateLabel = new wxStaticText(panel, wxID_ANY, "Sample rate:");
    sampleRateChoice_ = new wxChoice(panel, wxID_ANY);
    deviceChoice_->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) { refreshSampleRatesForSelectedDevice(); });
    refreshSampleRatesForSelectedDevice();

    auto* volLabel = new wxStaticText(panel, wxID_ANY, "Volume:");
    volumeSlider_ = new wxSlider(panel, wxID_ANY, settings_.volume, 0, 100,
                                 wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL | wxSL_LABELS);

    auto* grid = new wxFlexGridSizer(3, 2, 6, 10);
    grid->AddGrowableCol(1, 1);
    grid->Add(devLabel, 0, wxALIGN_CENTER_VERTICAL);
    grid->Add(deviceChoice_, 1, wxEXPAND);
    grid->Add(rateLabel, 0, wxALIGN_CENTER_VERTICAL);
    grid->Add(sampleRateChoice_, 1, wxEXPAND);
    grid->Add(volLabel, 0, wxALIGN_CENTER_VERTICAL);
    grid->Add(volumeSlider_, 1, wxEXPAND);

    auto* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(grid, 0, wxEXPAND | wxALL, 12);
    panel->SetSizer(sizer);
    return panel;
}

void SettingsDialog::refreshSampleRatesForSelectedDevice() {
    // Remember the rate currently selected (if any) so it can be kept when
    // still supported by the newly selected device.
    int previouslySelected = -1;
    if (sampleRateChoice_->GetCount() > 0) {
        const int sel = sampleRateChoice_->GetSelection();
        if (sel != wxNOT_FOUND) previouslySelected = currentSampleRates_[static_cast<std::size_t>(sel)];
    } else {
        previouslySelected = settings_.sampleRate;
    }

    const int devSel = deviceChoice_->GetSelection();
    const ma_device_id* deviceId = devSel > 0 ? &devices_[static_cast<std::size_t>(devSel) - 1].id : nullptr;
    currentSampleRates_ = AudioEngine::supportedSampleRates(deviceId);

    sampleRateChoice_->Clear();
    for (int rate : currentSampleRates_) sampleRateChoice_->Append(wxString::Format("%d Hz", rate));

    // Prefer: the previously selected rate if still supported, else 48000,
    // else 44100, else whatever is first in the (non-empty) list.
    auto indexOf = [this](int rate) -> int {
        for (std::size_t i = 0; i < currentSampleRates_.size(); ++i) {
            if (currentSampleRates_[i] == rate) return static_cast<int>(i);
        }
        return wxNOT_FOUND;
    };
    int newSel = indexOf(previouslySelected);
    if (newSel == wxNOT_FOUND) newSel = indexOf(48000);
    if (newSel == wxNOT_FOUND) newSel = indexOf(44100);
    if (newSel == wxNOT_FOUND) newSel = 0;
    sampleRateChoice_->SetSelection(newSel);
}

void SettingsDialog::onOK(wxCommandEvent& event) {
    settings_.singleInstance = singleInstanceCheck_->GetValue();
    settings_.confirmDeleteEffect = confirmDeleteCheck_->GetValue();

    settings_.chipType = chipChoice_->GetSelection() == 1 ? "ym2149" : "ay38910";
    settings_.clockHz = clockSpin_->GetValue();
    settings_.lowpassHz = lowpassCheck_->GetValue() ? lowpassSpin_->GetValue() : 0;

    const int devSel = deviceChoice_->GetSelection();
    settings_.outputDevice = devSel > 0 ? deviceChoice_->GetString(devSel).ToStdString() : std::string();
    settings_.sampleRate = currentSampleRates_[static_cast<std::size_t>(sampleRateChoice_->GetSelection())];
    settings_.volume = volumeSlider_->GetValue();

    event.Skip();  // let the default handler EndModal(wxID_OK) and close the dialog
}
