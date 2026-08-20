#include "PianoWindow.h"
#include "MainFrame.h"

#include <algorithm>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/dcbuffer.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/settings.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>

namespace {

// Note period tables: 5 tracker frequency tables x 8 octaves x 12 semitones,
// copied verbatim from the original editor (UnitPiano.cpp FreqTables).
constexpr int kNoteFreqTables[5][8][12] = {
    {  // Soundtracker
        {0x0EF8, 0x0E10, 0x0D60, 0x0C80, 0x0BD8, 0x0B28, 0x0A88, 0x09F0, 0x0960, 0x08E0, 0x0858, 0x07E0},
        {0x077C, 0x0708, 0x06B0, 0x0640, 0x05EC, 0x0594, 0x0544, 0x04F8, 0x04B0, 0x0470, 0x042C, 0x03FD},
        {0x03BE, 0x0384, 0x0358, 0x0320, 0x02F6, 0x02CA, 0x02A2, 0x027C, 0x0258, 0x0238, 0x0216, 0x01F8},
        {0x01DF, 0x01C2, 0x01AC, 0x0190, 0x017B, 0x0165, 0x0151, 0x013E, 0x012C, 0x011C, 0x010A, 0x00FC},
        {0x00EF, 0x00E1, 0x00D6, 0x00C8, 0x00BD, 0x00B2, 0x00A8, 0x009F, 0x0096, 0x008E, 0x0085, 0x007E},
        {0x0077, 0x0070, 0x006B, 0x0064, 0x005E, 0x0059, 0x0054, 0x004F, 0x004B, 0x0047, 0x0042, 0x003F},
        {0x003B, 0x0038, 0x0035, 0x0032, 0x002F, 0x002C, 0x002A, 0x0027, 0x0025, 0x0023, 0x0021, 0x001F},
        {0x001D, 0x001C, 0x001A, 0x0019, 0x0017, 0x0016, 0x0015, 0x0013, 0x0012, 0x0011, 0x0010, 0x000F},
    },
    {  // Protracker
        {0x0C22, 0x0B73, 0x0ACF, 0x0A33, 0x09A1, 0x0917, 0x0894, 0x0819, 0x07A4, 0x0737, 0x06CF, 0x066D},
        {0x0611, 0x05BA, 0x0567, 0x051A, 0x04D0, 0x048B, 0x044A, 0x040C, 0x03D2, 0x039B, 0x0367, 0x0337},
        {0x0308, 0x02DD, 0x02B4, 0x028D, 0x0268, 0x0246, 0x0225, 0x0206, 0x01E9, 0x01CE, 0x01B4, 0x019B},
        {0x0184, 0x016E, 0x015A, 0x0146, 0x0134, 0x0123, 0x0112, 0x0103, 0x00F5, 0x00E7, 0x00DA, 0x00CE},
        {0x00C2, 0x00B7, 0x00AD, 0x00A3, 0x009A, 0x0091, 0x0089, 0x0082, 0x007A, 0x0073, 0x006D, 0x0067},
        {0x0061, 0x005C, 0x0056, 0x0052, 0x004D, 0x0049, 0x0045, 0x0041, 0x003D, 0x003A, 0x0036, 0x0033},
        {0x0031, 0x002E, 0x002B, 0x0029, 0x0027, 0x0024, 0x0022, 0x0020, 0x001F, 0x001D, 0x001B, 0x001A},
        {0x0018, 0x0017, 0x0016, 0x0014, 0x0013, 0x0012, 0x0011, 0x0010, 0x000F, 0x000E, 0x000D, 0x000C},
    },
    {  // ASM or PSC
        {0x0D10, 0x0C55, 0x0BA4, 0x0AFC, 0x0A5F, 0x09CA, 0x093D, 0x08B8, 0x083B, 0x07C5, 0x0755, 0x06EC},
        {0x0688, 0x062A, 0x05D2, 0x057E, 0x052F, 0x04E5, 0x049E, 0x045C, 0x041D, 0x03E2, 0x03AB, 0x0376},
        {0x0344, 0x0315, 0x02E9, 0x02BF, 0x0298, 0x0272, 0x024F, 0x022E, 0x020F, 0x01F1, 0x01D5, 0x01BB},
        {0x01A2, 0x018B, 0x0174, 0x0160, 0x014C, 0x0139, 0x0128, 0x0117, 0x0107, 0x00F9, 0x00EB, 0x00DD},
        {0x00D1, 0x00C5, 0x00BA, 0x00B0, 0x00A6, 0x009D, 0x0094, 0x008C, 0x0084, 0x007C, 0x0075, 0x006F},
        {0x0069, 0x0063, 0x005D, 0x0058, 0x0053, 0x004E, 0x004A, 0x0046, 0x0042, 0x003E, 0x003B, 0x0037},
        {0x0034, 0x0031, 0x002F, 0x002C, 0x0029, 0x0027, 0x0025, 0x0023, 0x0021, 0x001F, 0x001D, 0x001C},
        {0x001A, 0x0019, 0x0017, 0x0016, 0x0015, 0x0014, 0x0012, 0x0011, 0x0010, 0x000F, 0x000E, 0x000D},
    },
    {  // Real
        {0x0CDA, 0x0C22, 0x0B73, 0x0ACF, 0x0A33, 0x09A1, 0x0917, 0x0894, 0x0819, 0x07A4, 0x0737, 0x06CF},
        {0x066D, 0x0611, 0x05BA, 0x0567, 0x051A, 0x04D0, 0x048B, 0x044A, 0x040C, 0x03D2, 0x039B, 0x0367},
        {0x0337, 0x0308, 0x02DD, 0x02B4, 0x028D, 0x0268, 0x0246, 0x0225, 0x0206, 0x01E9, 0x01CE, 0x01B4},
        {0x019B, 0x0184, 0x016E, 0x015A, 0x0146, 0x0134, 0x0123, 0x0112, 0x0103, 0x00F5, 0x00E7, 0x00DA},
        {0x00CE, 0x00C2, 0x00B7, 0x00AD, 0x00A3, 0x009A, 0x0091, 0x0089, 0x0082, 0x007A, 0x0073, 0x006D},
        {0x0067, 0x0061, 0x005C, 0x0056, 0x0052, 0x004D, 0x0049, 0x0045, 0x0041, 0x003D, 0x003A, 0x0036},
        {0x0033, 0x0031, 0x002E, 0x002B, 0x0029, 0x0027, 0x0024, 0x0022, 0x0020, 0x001F, 0x001D, 0x001B},
        {0x001A, 0x0018, 0x0017, 0x0016, 0x0014, 0x0013, 0x0012, 0x0011, 0x0010, 0x000F, 0x000E, 0x000D},
    },
    {  // SQ Tracker
        {0x0D5D, 0x0C9C, 0x0BE7, 0x0B3C, 0x0A9B, 0x0A02, 0x0973, 0x08EB, 0x086B, 0x07F2, 0x0780, 0x0714},
        {0x06AE, 0x064E, 0x05F4, 0x059E, 0x054F, 0x0501, 0x04B9, 0x0475, 0x0435, 0x03F9, 0x03C0, 0x038A},
        {0x0357, 0x0327, 0x02FA, 0x02CF, 0x02A7, 0x0281, 0x025D, 0x023B, 0x021B, 0x01FC, 0x01E0, 0x01C5},
        {0x01AC, 0x0194, 0x017D, 0x0168, 0x0153, 0x0140, 0x012E, 0x011D, 0x010D, 0x00FE, 0x00F0, 0x00E2},
        {0x00D6, 0x00CA, 0x00BE, 0x00B4, 0x00AA, 0x00A0, 0x0097, 0x008F, 0x0087, 0x007F, 0x0078, 0x0071},
        {0x006B, 0x0065, 0x005F, 0x005A, 0x0055, 0x0050, 0x004C, 0x0047, 0x0043, 0x0040, 0x003C, 0x0039},
        {0x0035, 0x0032, 0x0030, 0x002D, 0x002A, 0x0028, 0x0026, 0x0024, 0x0022, 0x0020, 0x001E, 0x001C},
        {0x001B, 0x0019, 0x0018, 0x0016, 0x0015, 0x0014, 0x0013, 0x0012, 0x0011, 0x0010, 0x000F, 0x000E},
    },
};

constexpr const char* kNoteFreqTableNames[5] = {
    "Soundtracker", "Protracker", "ASM or PSC", "Real", "SQ Tracker"
};

// White keys C D E F G A B = notes 0,2,4,5,7,9,11; black keys C# D# F# G# A#
// = notes 1,3,6,8,10 (none between E-F, matching a real keyboard). Captions
// are the computer key that plays each note, not the note name — matching
// the original, which uses the same convention.
constexpr int kWhiteNotes[7] = {0, 2, 4, 5, 7, 9, 11};
constexpr char kWhiteKeys[7] = {'Z', 'X', 'C', 'V', 'B', 'N', 'M'};
// Index into kWhiteNotes of the white key each black key sits after.
constexpr int kBlackAfterWhite[5] = {0, 1, 3, 4, 5};
constexpr int kBlackNotes[5] = {1, 3, 6, 8, 10};
constexpr char kBlackKeys[5] = {'S', 'D', 'G', 'H', 'J'};

int NoteForChar(int c) {
    switch (c) {
        case 'Z': return 0;
        case 'S': return 1;
        case 'X': return 2;
        case 'D': return 3;
        case 'C': return 4;
        case 'V': return 5;
        case 'G': return 6;
        case 'B': return 7;
        case 'H': return 8;
        case 'N': return 9;
        case 'J': return 10;
        case 'M': return 11;
        default: return -1;
    }
}

}  // namespace

PianoWindow::PianoWindow(MainFrame* mainFrame)
    : wxFrame(mainFrame, wxID_ANY, "Piano input", wxDefaultPosition, wxDefaultSize,
              wxCAPTION | wxCLOSE_BOX | wxFRAME_FLOAT_ON_PARENT | wxFRAME_TOOL_WINDOW),
      mainFrame_(mainFrame) {
    createContent();
    Bind(wxEVT_CLOSE_WINDOW, &PianoWindow::onClose, this);
    Bind(wxEVT_CHAR_HOOK, &PianoWindow::onCharHook, this);
    Bind(wxEVT_KEY_UP, &PianoWindow::onKeyUp, this);
    // The Octave spinner is first in tab order and grabs focus by default;
    // as a native text control it silently swallows note-key presses (e.g.
    // "Z") as invalid numeric input before wxEVT_CHAR_HOOK ever sees them.
    // Keying off the keys panel instead — nothing else claims focus there —
    // makes keyboard note entry work as soon as the window opens.
    Bind(wxEVT_SHOW, [this](wxShowEvent& event) {
        if (event.IsShown() && keysPanel_) {
            keysPanel_->SetFocus();
        }
        event.Skip();
    });
}

void PianoWindow::createContent() {
    auto* panel = new wxPanel(this);
    auto* root = new wxBoxSizer(wxVERTICAL);

    auto addSpin = [&](wxBoxSizer* row, const wxString& label, int lo, int hi, int initial) {
        row->Add(new wxStaticText(panel, wxID_ANY, label), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, panel->FromDIP(4));
        auto* spin = new wxSpinCtrl(panel, wxID_ANY, "", wxDefaultPosition, panel->FromDIP(wxSize(60, -1)),
                                     wxSP_ARROW_KEYS, lo, hi, initial);
        row->Add(spin, 0, wxRIGHT, panel->FromDIP(12));
        return spin;
    };

    auto* row1 = new wxBoxSizer(wxHORIZONTAL);
    octaveSpin_ = addSpin(row1, "Octave:", 1, 8, octave_);
    stepSpin_ = addSpin(row1, "Step:", 0, 256, step_);
    fillSpin_ = addSpin(row1, "Fill:", 1, 256, fill_);
    root->Add(row1, 0, wxALL, panel->FromDIP(8));

    auto* row2 = new wxBoxSizer(wxHORIZONTAL);
    setToneCheck_ = new wxCheckBox(panel, wxID_ANY, "Set T");
    setToneCheck_->SetValue(true);
    linkFillCheck_ = new wxCheckBox(panel, wxID_ANY, "Link fill = step");
    linkFillCheck_->SetValue(true);
    setVolumeCheck_ = new wxCheckBox(panel, wxID_ANY, "Set volume");
    setVolumeCheck_->SetValue(true);
    row2->Add(setToneCheck_, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL, panel->FromDIP(10));
    row2->Add(linkFillCheck_, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL, panel->FromDIP(10));
    row2->Add(setVolumeCheck_, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL, panel->FromDIP(4));
    volumeSpin_ = new wxSpinCtrl(panel, wxID_ANY, "", wxDefaultPosition, panel->FromDIP(wxSize(55, -1)),
                                  wxSP_ARROW_KEYS, 0, 15, volume_);
    row2->Add(volumeSpin_, 0);
    root->Add(row2, 0, wxLEFT | wxRIGHT | wxBOTTOM, panel->FromDIP(8));

    auto* row3 = new wxBoxSizer(wxHORIZONTAL);
    row3->Add(new wxStaticText(panel, wxID_ANY, "Freq. table:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, panel->FromDIP(4));
    tableChoice_ = new wxChoice(panel, wxID_ANY);
    for (auto name : kNoteFreqTableNames) {
        tableChoice_->Append(name);
    }
    tableChoice_->SetSelection(tableIndex_);
    row3->Add(tableChoice_, 0);
    root->Add(row3, 0, wxLEFT | wxRIGHT | wxBOTTOM, panel->FromDIP(8));

    // wxWANTS_CHARS: a plain wxPanel doesn't reliably accept keyboard focus
    // (or char events) on every platform without it, and note entry depends
    // on this panel actually holding focus so wxEVT_CHAR_HOOK sees note keys
    // before any native spin/text control's own key handling can eat them.
    keysPanel_ = new wxPanel(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxWANTS_CHARS);
    keysPanel_->SetBackgroundStyle(wxBG_STYLE_PAINT);
    keysPanel_->SetMinSize(panel->FromDIP(wxSize(280, 90)));
    root->Add(keysPanel_, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, panel->FromDIP(8));

    panel->SetSizer(root);
    auto* outer = new wxBoxSizer(wxVERTICAL);
    outer->Add(panel, 1, wxEXPAND);
    SetSizerAndFit(outer);

    linkFillCheck_->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) { updateFillEnabled(); });
    stepSpin_->Bind(wxEVT_SPINCTRL, [this](wxCommandEvent&) {
        step_ = stepSpin_->GetValue();
        updateFillEnabled();
    });
    fillSpin_->Bind(wxEVT_SPINCTRL, [this](wxCommandEvent&) { fill_ = fillSpin_->GetValue(); });
    octaveSpin_->Bind(wxEVT_SPINCTRL, [this](wxCommandEvent&) { octave_ = octaveSpin_->GetValue(); });
    volumeSpin_->Bind(wxEVT_SPINCTRL, [this](wxCommandEvent&) { volume_ = volumeSpin_->GetValue(); });
    tableChoice_->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) { tableIndex_ = tableChoice_->GetSelection(); });

    keysPanel_->Bind(wxEVT_PAINT, &PianoWindow::onKeysPaint, this);
    keysPanel_->Bind(wxEVT_LEFT_DOWN, &PianoWindow::onKeysMouseDown, this);
    keysPanel_->Bind(wxEVT_LEFT_UP, &PianoWindow::onKeysMouseUp, this);
    keysPanel_->Bind(wxEVT_CHAR_HOOK, &PianoWindow::onCharHook, this);
    keysPanel_->Bind(wxEVT_KEY_UP, &PianoWindow::onKeyUp, this);

    updateFillEnabled();
}

void PianoWindow::updateFillEnabled() {
    const bool linked = linkFillCheck_->GetValue();
    fillSpin_->Enable(!linked);
    if (linked) {
        fillSpin_->SetValue(step_);
        fill_ = step_;
    }
}

int PianoWindow::keyAt(int x, int y) const {
    const wxSize sz = keysPanel_->GetClientSize();
    if (sz.GetWidth() <= 0 || sz.GetHeight() <= 0) {
        return -1;
    }
    const double whiteW = static_cast<double>(sz.GetWidth()) / 7.0;
    const int blackH = sz.GetHeight() * 3 / 5;
    const double blackW = whiteW * 0.6;

    // Black keys are on top, so test them first.
    for (int i = 0; i < 5; ++i) {
        const double cx = (kBlackAfterWhite[i] + 1) * whiteW;
        const double left = cx - blackW / 2.0;
        if (y < blackH && x >= left && x <= left + blackW) {
            return kBlackNotes[i];
        }
    }

    if (y < sz.GetHeight()) {
        const int col = std::clamp(static_cast<int>(x / whiteW), 0, 6);
        return kWhiteNotes[col];
    }
    return -1;
}

void PianoWindow::onKeysPaint(wxPaintEvent&) {
    wxAutoBufferedPaintDC dc(keysPanel_);
    const wxSize sz = keysPanel_->GetClientSize();
    const wxColour accent = wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT);

    dc.SetBackground(wxBrush(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW)));
    dc.Clear();
    if (sz.GetWidth() <= 0 || sz.GetHeight() <= 0) {
        return;
    }

    const double whiteW = static_cast<double>(sz.GetWidth()) / 7.0;
    const int blackH = sz.GetHeight() * 3 / 5;
    const double blackW = whiteW * 0.6;

    // Piano keys are white/black regardless of app theme, like a real
    // instrument — not sourced from EditorPalette/system colours.
    dc.SetFont(wxFontInfo(10).Bold());
    for (int i = 0; i < 7; ++i) {
        const wxRect r(static_cast<int>(i * whiteW), 0, static_cast<int>(whiteW), sz.GetHeight());
        dc.SetBrush(wxBrush(kWhiteNotes[i] == highlightedKey_ ? accent : *wxWHITE));
        dc.SetPen(*wxBLACK_PEN);
        dc.DrawRectangle(r);
        dc.SetTextForeground(*wxBLACK);
        dc.DrawLabel(wxString(kWhiteKeys[i]),
                     wxRect(r.GetX(), r.GetHeight() - r.GetHeight() / 4, r.GetWidth(), r.GetHeight() / 4),
                     wxALIGN_CENTER_HORIZONTAL);
    }
    for (int i = 0; i < 5; ++i) {
        const double cx = (kBlackAfterWhite[i] + 1) * whiteW;
        const wxRect r(static_cast<int>(cx - blackW / 2.0), 0, static_cast<int>(blackW), blackH);
        dc.SetBrush(wxBrush(kBlackNotes[i] == highlightedKey_ ? accent : *wxBLACK));
        dc.SetPen(*wxBLACK_PEN);
        dc.DrawRectangle(r);
        dc.SetTextForeground(*wxWHITE);
        dc.DrawLabel(wxString(kBlackKeys[i]),
                     wxRect(r.GetX(), r.GetHeight() - r.GetHeight() / 3, r.GetWidth(), r.GetHeight() / 3),
                     wxALIGN_CENTER_HORIZONTAL);
    }
}

void PianoWindow::onKeysMouseDown(wxMouseEvent& event) {
    enterNote(keyAt(event.GetX(), event.GetY()));
}

void PianoWindow::onKeysMouseUp(wxMouseEvent&) {
    highlightedKey_ = -1;
    if (keysPanel_) {
        keysPanel_->Refresh();
    }
}

void PianoWindow::enterNote(int note) {
    if (note < 0 || note > 11) {
        return;
    }

    int coct = octave_;
    if (wxGetKeyState(WXK_SHIFT)) {
        ++coct;
    }
    if (wxGetKeyState(WXK_CONTROL)) {
        --coct;
    }
    coct = std::clamp(coct - 1, 0, 7);

    const int period = kNoteFreqTables[tableIndex_][coct][note];
    const int volume = setVolumeCheck_->GetValue() ? volume_ : -1;
    const int fill = linkFillCheck_->GetValue() ? step_ : fill_;

    mainFrame_->enterFromPiano(period, setToneCheck_->GetValue(), volume, step_, fill);

    highlightedKey_ = note;
    if (keysPanel_) {
        keysPanel_->Refresh();
    }
}

void PianoWindow::onCharHook(wxKeyEvent& event) {
    if (event.ControlDown() && (event.GetKeyCode() == '.' || event.GetKeyCode() == 'P')) {
        Hide();
        mainFrame_->syncPianoToggleState(false);
        return;
    }

    const int key = event.GetKeyCode();
    if (key >= WXK_NUMPAD1 && key <= (WXK_NUMPAD1 + 7)) {
        octave_ = key - WXK_NUMPAD1 + 1;
        octaveSpin_->SetValue(octave_);
        return;
    }

    const int note = NoteForChar(key);
    if (note >= 0) {
        enterNote(note);
        return;
    }

    event.Skip();
}

void PianoWindow::onKeyUp(wxKeyEvent& event) {
    // Matches the original's FormKeyUp: any key release clears the
    // highlighted key, regardless of which key it was.
    highlightedKey_ = -1;
    if (keysPanel_) {
        keysPanel_->Refresh();
    }
    event.Skip();
}

void PianoWindow::onClose(wxCloseEvent& event) {
    event.Veto();
    Hide();
    mainFrame_->syncPianoToggleState(false);
}
