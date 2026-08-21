#include "SettingsDialog.h"

#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/treebook.h>

namespace {

// Placeholder page content -- filled in as each section's real controls
// are built (General, then Appearance, then Audio's two sub-pages).
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

SettingsDialog::SettingsDialog(wxWindow* parent)
    : wxDialog(parent, wxID_ANY, "Preferences", wxDefaultPosition, wxDefaultSize,
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER) {
    auto* book = new wxTreebook(this, wxID_ANY);
    book->AddPage(MakeStubPage(book, "General (stub)"), "General");
    book->AddPage(MakeStubPage(book, "Appearance (stub)"), "Appearance");
    book->AddPage(MakeStubPage(book, "Audio"), "Audio");
    book->AddSubPage(MakeStubPage(book, "Engine (stub)"), "Engine");
    book->AddSubPage(MakeStubPage(book, "Output device (stub)"), "Output device");

    auto* vs = new wxBoxSizer(wxVERTICAL);
    vs->Add(book, 1, wxEXPAND | wxALL, 8);
    vs->Add(CreateStdDialogButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxALL, 8);
    SetSizerAndFit(vs);
    SetMinSize(FromDIP(wxSize(560, 400)));
}
