#include "SettingsDialog.h"

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/treebook.h>

namespace {

// Placeholder page content -- filled in as each remaining section's real
// controls are built (Appearance, then Audio's two sub-pages).
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
    book->AddSubPage(MakeStubPage(book, "Engine (stub)"), "Engine");
    book->AddSubPage(MakeStubPage(book, "Output device (stub)"), "Output device");

    auto* vs = new wxBoxSizer(wxVERTICAL);
    vs->Add(book, 1, wxEXPAND | wxALL, 8);
    vs->Add(CreateStdDialogButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxALL, 8);
    SetSizerAndFit(vs);
    SetMinSize(FromDIP(wxSize(560, 400)));

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

void SettingsDialog::onOK(wxCommandEvent& event) {
    settings_.singleInstance = singleInstanceCheck_->GetValue();
    settings_.confirmDeleteEffect = confirmDeleteCheck_->GetValue();
    event.Skip();  // let the default handler EndModal(wxID_OK) and close the dialog
}
