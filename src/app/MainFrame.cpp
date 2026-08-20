#include "MainFrame.h"
#include "PianoWindow.h"
#include "../audio/WaveExport.h"
#include "../core/VtxDecoder.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstring>
#include <fstream>
#include <string>
#include <string_view>

#include <wx/accel.h>
#include <wx/choice.h>
#include <wx/dialog.h>
#include <wx/slider.h>
#include <wx/stattext.h>
#include <wx/bmpbndl.h>
#include <wx/bmpbuttn.h>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/dcbuffer.h>
#include <wx/dcmemory.h>
#include <wx/dirdlg.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/font.h>
#include <wx/fontenum.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <wx/panel.h>
#include <wx/radiobut.h>
#include <wx/settings.h>
#include <wx/sizer.h>
#include <wx/stdpaths.h>
#include <wx/textctrl.h>
#include <wx/tglbtn.h>

namespace {

enum : int {
    kIdNewBank = wxID_HIGHEST + 101,
    kIdLoadBank,
    kIdSaveBank,
    kIdSaveBankNoNames,
    kIdLoadEffect,
    kIdSaveEffect,

    kIdEditCut,
    kIdEditCopy,
    kIdEditPaste,
    kIdEditDelete,
    kIdEditSelectAll,
    kIdEditUnselectAll,
    kIdEditInverseSelection,

    kIdViewPiano,
    kIdViewLinear,
    kIdViewLog,

    kIdFirstEffect,
    kIdPrevEffect,
    kIdNextEffect,
    kIdLastEffect,
    kIdAddEffect,
    kIdInsertEffect,
    kIdDeleteEffect,

    kIdApplyName,
    kIdPlayEffect,
    kIdStopEffect,
    kIdAudioSettings,
    kIdExportWave,
    kIdExportCsv,
    kIdExportVt2,
    kIdImportPsg,
    kIdImportVtx,
    kIdImportVgm,
    kIdMultiLoadBank,
    kIdMultiSaveBank,
    kIdExportScopeCurrent,
    kIdExportScopeAll,
    kIdNotImplemented
};

constexpr double kVt2NoteFreqs[12] = {
    2093.0, 2217.4, 2349.2, 2489.0, 2637.0, 2793.8,
    2960.0, 3136.0, 3322.4, 3520.0, 3729.2, 3951.0,
};

// Vortex Tracker II export note/octave -> AY tone-period offset table,
// ported verbatim from the original's EffectExportVT2 (export_vt2.h). One
// extra slot (index 96) is kept zero-initialized: the original's BaseNote
// can reach 8*12=96, one past its own 8*12-entry table (an off-by-one in
// the original that reads past the array there; harmless in Delphi's
// static array layout, but would be real UB here), so this array is sized
// 8*12+1 to give that edge case a defined (if musically meaningless) value
// instead.
std::array<int, 8 * 12 + 1> BuildVt2OffsetTable() {
    std::array<int, 8 * 12 + 1> offset{};
    for (int i = 0; i < 8; ++i) {
        for (int j = 0; j < 12; ++j) {
            const int n = static_cast<int>(kAYClockRateSpectrum * 8.0 / kVt2NoteFreqs[j]);
            offset[i * 12 + j] = n >> i;
        }
    }
    return offset;
}

struct EditorLayout {
    static constexpr int kXOff = 4;
    static constexpr int kYOff = 4;
    static constexpr int kHead = 20;
    static constexpr int kLineHgt = 20;

    static constexpr int kToneOff = 151;
    static constexpr int kToneWdt = 326;
    static constexpr int kNoiseOff = 484;
    static constexpr int kNoiseWdt = 64;
    static constexpr int kVolOff = 555;
    static constexpr int kVolWdt = 64;

    // T/N boxes widened and shifted left so the whitespace on either side of
    // them (Pos-to-T, N-to-Per) reads as roughly even, measured against the
    // header font's actual glyph metrics rather than assumed ones — with the
    // narrower fallback font this previously silently substituted on macOS
    // (see MakeEditorFont), "Pos" rendered narrow enough that this wasn't
    // noticeable; real Menlo/Consolas needs the wider box to still balance.
    static constexpr int kTFlagOff = 20;
    static constexpr int kTFlagWdt = 9;
    static constexpr int kNFlagOff = 46;
    static constexpr int kNFlagWdt = 9;
    static constexpr int kPosWdt = 27;

    static constexpr int kTColOff = 72;
    static constexpr int kTColWdt = 27;
    static constexpr int kNColOff = 108;
    static constexpr int kNColWdt = 18;
    static constexpr int kVColOff = 135;
    static constexpr int kVColWdt = 9;
};

int periodToWidth(int period, int toneWidth, bool linear) {
    if (linear) {
        return 1 + (toneWidth * period) / 4096;
    }
    if (period <= 0) {
        return 0;
    }

    const double p = static_cast<double>(period) / 8.0;
    const double den = std::log(4095.0 / 8.0);
    return static_cast<int>(static_cast<double>(toneWidth) * std::log(p) / den);
}

int widthToPeriod(int width, int toneWidth, bool linear) {
    if (linear) {
        return (width * 4096) / toneWidth;
    }

    const double k = static_cast<double>(toneWidth) / std::log(4095.0 / 8.0);
    return static_cast<int>(8.0 * std::exp(static_cast<double>(1 + width) / k));
}

int Dip(const wxWindow* w, int v) {
    return w ? w->FromDIP(v) : v;
}

struct BarLayout {
    int toneOff, toneW;
    int noiseOff, noiseW;
    int volOff, volW;
};

BarLayout ComputeBarLayout(const wxWindow* w) {
    const int toneOff  = Dip(w, EditorLayout::kToneOff);
    const int gap      = Dip(w, 6);
    const int minSmall = Dip(w, 48);  // minimum width for noise/volume bars
    const int rightMargin = Dip(w, 8);
    const int panelW   = w ? w->GetClientSize().GetWidth() : 640;
    // Available space for all three bars + 2 gaps
    const int totalBarSpace = std::max(0, panelW - rightMargin - toneOff - gap * 2);
    // Distribute 4:1:1, but noise/vol never below minSmall
    const int smallW   = std::max(minSmall, totalBarSpace / 6);
    const int toneW    = std::max(Dip(w, 60), totalBarSpace - smallW * 2);
    const int noiseW   = smallW;
    const int volW     = smallW;
    const int noiseOff = toneOff + toneW + gap;
    const int volOff   = noiseOff + noiseW + gap;
    return {toneOff, toneW, noiseOff, noiseW, volOff, volW};
}

wxFont MakeEditorFont(int pointSize, bool bold = false) {
    // Platform-native monospace chain: light/regular weight preferred.
    // Windows: Consolas; macOS: Menlo; Linux: DejaVu Sans Mono / Liberation Mono.
    //
    // NOTE: wxFont::IsOk() is not a reliable "does this face exist" check —
    // on the macOS/Cocoa backend it returns true even for a face name that
    // doesn't exist (Core Text silently substitutes a default font instead
    // of failing), so it always accepted the first ("Consolas") candidate
    // there and never fell through to Menlo. wxFontEnumerator::IsValidFacename
    // actually queries font availability and must be used instead.
    static const char* kFaces[] = {
        "Consolas",        // Windows
        "Menlo",           // macOS
        "DejaVu Sans Mono",// Linux
        "Liberation Mono", // Linux alt
        "Courier New",     // universal fallback
    };
    for (const char* face : kFaces) {
        if (wxFontEnumerator::IsValidFacename(face)) {
            return wxFontInfo(pointSize).Family(wxFONTFAMILY_TELETYPE).FaceName(face).Bold(bold);
        }
    }
    return wxFontInfo(pointSize).Family(wxFONTFAMILY_TELETYPE).Bold(bold);
}

int EditorPointSize() {
    return 14;
}

int EditorHeaderPointSize() {
    return 12;
}

int EditorLineHeight(const wxWindow* w) {
    if (!w) {
        return EditorLayout::kLineHgt;
    }

    const wxFont font = MakeEditorFont(EditorPointSize());
    wxCoord textW = 0;
    wxCoord textH = 0;
    w->GetTextExtent("0", &textW, &textH, nullptr, nullptr, &font);
    // +1 for the 1px bottom separator; no extra vertical padding.
    return textH + 1;
}

int EditorTextHitWidth(const wxWindow* w, const wxString& sample, int minWidth) {
    if (!w) {
        return minWidth;
    }

    const wxFont font = MakeEditorFont(EditorPointSize());
    wxCoord textW = 0;
    wxCoord textH = 0;
    w->GetTextExtent(sample, &textW, &textH, nullptr, nullptr, &font);
    return std::max(minWidth, static_cast<int>(textW) + Dip(w, 4));
}

wxSize MeasureEditorText(const wxWindow* w, const wxString& text) {
    if (!w) {
        return wxSize(static_cast<int>(text.length()) * 8, EditorLayout::kLineHgt);
    }

    const wxFont font = MakeEditorFont(EditorPointSize());
    wxCoord textW = 0;
    wxCoord textH = 0;
    w->GetTextExtent(text, &textW, &textH, nullptr, nullptr, &font);
    return wxSize(textW, textH);
}

wxRect MakeEditorTextRect(const wxWindow* w, int x, int y, const wxString& text, int lineHeight) {
    const int padX = Dip(w, 2);
    const wxSize size = MeasureEditorText(w, text);
    return wxRect(x - padX, y, size.GetWidth() + padX * 2, lineHeight);
}

struct EditorPalette {
    wxColour background;  // editor panel background
    wxColour text;        // normal cell text
    wxColour barBorder;   // bar rectangle border
    wxColour cursorBg;    // active cell highlight background
    wxColour cursorText;  // active cell highlight text
    wxColour iconFg;      // toolbar icon foreground (stop square, piano keys)
    wxColour iconBg;      // toolbar icon background (white keys on piano)
    wxColour playFill;    // play triangle fill
    wxColour playBorder;  // play triangle border

    static const EditorPalette& light() {
        static const EditorPalette p = {
            wxColour(255, 255, 255),  // background
            wxColour(  0,   0,   0),  // text
            wxColour(  0,   0,   0),  // barBorder
            wxColour(  0,   0,   0),  // cursorBg
            wxColour(255, 255, 255),  // cursorText
            wxColour(  0,   0,   0),  // iconFg
            wxColour(255, 255, 255),  // iconBg
            wxColour(  0, 185,   0),  // playFill
            wxColour( 30, 120,  30),  // playBorder
        };
        return p;
    }

    static const EditorPalette& dark() {
        static const EditorPalette p = {
            wxColour( 32,  32,  32),  // background
            wxColour(225, 225, 225),  // text
            wxColour( 85,  85,  85),  // barBorder
            wxColour(255, 255, 255),  // cursorBg
            wxColour(  0,   0,   0),  // cursorText
            wxColour(225, 225, 225),  // iconFg
            wxColour( 32,  32,  32),  // iconBg
            wxColour(  0, 210,   0),  // playFill
            wxColour( 60, 170,  60),  // playBorder
        };
        return p;
    }

    // Picks light() or dark() to match the current system/app appearance
    // (wxWidgets 3.3's native dark mode support — see wxSystemAppearance).
    static const EditorPalette& current() {
        return wxSystemSettings::GetAppearance().IsDark() ? dark() : light();
    }
};

wxBitmap CreateToolbarBitmap(const wxWindow* w) {
    const int s = Dip(w, 22);
    wxBitmap bmp(s, s, 32);
    wxMemoryDC mdc;
    mdc.SelectObject(bmp);
    mdc.SetBackground(wxBrush(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE)));
    mdc.Clear();
    mdc.SelectObject(wxNullBitmap);
    return bmp;
}

wxBitmap MakePlayBitmap(const wxWindow* w) {
    const EditorPalette& pal = EditorPalette::current();
    wxBitmap bmp = CreateToolbarBitmap(w);
    wxMemoryDC mdc;
    mdc.SelectObject(bmp);
    mdc.SetPen(*wxTRANSPARENT_PEN);
    mdc.SetBrush(wxBrush(pal.playFill));

    const int x0 = Dip(w, 6);
    const int y0 = Dip(w, 4);
    const int x1 = Dip(w, 6);
    const int y1 = Dip(w, 18);
    const int x2 = Dip(w, 17);
    const int y2 = Dip(w, 11);
    wxPoint pts[3] = {wxPoint(x0, y0), wxPoint(x1, y1), wxPoint(x2, y2)};
    mdc.DrawPolygon(3, pts);

    mdc.SetPen(wxPen(pal.playBorder, std::max(1, Dip(w, 1))));
    mdc.SetBrush(*wxTRANSPARENT_BRUSH);
    mdc.DrawPolygon(3, pts);
    mdc.SelectObject(wxNullBitmap);
    return bmp;
}

wxBitmap MakeStopBitmap(const wxWindow* w) {
    const EditorPalette& pal = EditorPalette::current();
    wxBitmap bmp = CreateToolbarBitmap(w);
    wxMemoryDC mdc;
    mdc.SelectObject(bmp);
    const int pad = Dip(w, 5);
    const int side = std::max(1, Dip(w, 12));
    mdc.SetPen(wxPen(pal.iconFg, std::max(1, Dip(w, 1))));
    mdc.SetBrush(wxBrush(pal.iconFg));
    mdc.DrawRectangle(pad, pad, side, side);
    mdc.SelectObject(wxNullBitmap);
    return bmp;
}

wxBitmap MakePianoBitmap(const wxWindow* w) {
    const EditorPalette& pal = EditorPalette::current();
    wxBitmap bmp = CreateToolbarBitmap(w);
    wxMemoryDC mdc;
    mdc.SelectObject(bmp);

    const int x = Dip(w, 3);
    const int y = Dip(w, 3);
    const int wdt = std::max(1, Dip(w, 16));
    const int hgt = std::max(1, Dip(w, 16));
    const int keyW = std::max(1, wdt / 5);

    mdc.SetPen(wxPen(pal.iconFg, std::max(1, Dip(w, 1))));
    mdc.SetBrush(wxBrush(pal.iconBg));
    mdc.DrawRectangle(x, y, wdt, hgt);

    for (int i = 1; i < 5; ++i) {
        const int xx = x + i * keyW;
        mdc.DrawLine(xx, y + Dip(w, 8), xx, y + hgt - 1);
    }

    mdc.SetBrush(wxBrush(pal.iconFg));
    const int bW = std::max(1, keyW / 2);
    const int bH = std::max(1, Dip(w, 7));
    mdc.DrawRectangle(x + keyW - bW / 2, y, bW, bH);
    mdc.DrawRectangle(x + 2 * keyW - bW / 2, y, bW, bH);
    mdc.DrawRectangle(x + 4 * keyW - bW / 2, y, bW, bH);

    mdc.SelectObject(wxNullBitmap);
    return bmp;
}

}  // namespace

MainFrame::MainFrame()
    : wxFrame(nullptr, wxID_ANY, "AY Sound FX Editor v0.6", wxDefaultPosition, wxSize(640, 620)) {
    const wxSize baseSize = wxWindow::FromDIP(wxSize(640, 620), this);
    const wxSize minSize = wxWindow::FromDIP(wxSize(640, 320), this);
    SetSize(baseSize);
    SetMinSize(minSize);
    audioEngine_.initialize();
    createMenu();
    CreateStatusBar(1);
    createContent();
    refreshView();
}

void MainFrame::createMenu() {
    auto* menuBar = new wxMenuBar();

    auto* fileMenu = new wxMenu();
    fileMenu->Append(kIdNewBank, "&New bank\tCtrl+N");
    fileMenu->Append(kIdLoadBank, "&Load bank...\tCtrl+O");
    fileMenu->Append(kIdSaveBank, "&Save bank...\tCtrl+S");
    fileMenu->Append(kIdSaveBankNoNames, "Save bank w/o names...");
    fileMenu->AppendSeparator();
    fileMenu->Append(kIdLoadEffect, "Load current effect...");
    fileMenu->Append(kIdSaveEffect, "Save current effect...");
    fileMenu->AppendSeparator();
    fileMenu->Append(kIdMultiLoadBank, "Multi-load to bank...");
    fileMenu->Append(kIdMultiSaveBank, "Multi-save from bank...");
    fileMenu->AppendSeparator();
    fileMenu->Append(wxID_EXIT, "E&xit");

    auto* editMenu = new wxMenu();
    editMenu->Append(kIdEditCut, "Cut\tCtrl+X");
    editMenu->Append(kIdEditCopy, "Copy\tCtrl+C");
    editMenu->Append(kIdEditPaste, "Paste\tCtrl+V");
    editMenu->AppendSeparator();
    editMenu->Append(kIdEditDelete, "Delete\tDel");
    editMenu->AppendSeparator();
    editMenu->Append(kIdEditSelectAll, "Select all\tCtrl+A");
    editMenu->Append(kIdEditUnselectAll, "Unselect all");
    editMenu->Append(kIdEditInverseSelection, "Inverse selection\tCtrl+I");

    auto* viewMenu = new wxMenu();
    viewMenu->AppendCheckItem(kIdViewPiano, "Piano input");
    viewLinearItem_ = viewMenu->AppendRadioItem(kIdViewLinear, "Linear period");
    viewLogItem_ = viewMenu->AppendRadioItem(kIdViewLog, "Logarithmic period");
    viewLinearItem_->Check(true);

    auto* bankMenu = new wxMenu();
    bankMenu->Append(kIdFirstEffect, "First effect");
    bankMenu->Append(kIdPrevEffect, "Previous effect\t-");
    bankMenu->Append(kIdNextEffect, "Next effect\t+");
    bankMenu->Append(kIdLastEffect, "Last effect");
    bankMenu->AppendSeparator();
    bankMenu->Append(kIdAddEffect, "Add effect");
    bankMenu->Append(kIdInsertEffect, "Insert effect");
    bankMenu->Append(kIdDeleteEffect, "Delete effect");

    auto* importMenu = new wxMenu();
    importMenu->Append(kIdImportPsg, "PSG for AY...");
    importMenu->Append(kIdImportVtx, "VTX file...");
    importMenu->Append(kIdImportVgm, "VGM file...");
    importMenu->Append(kIdNotImplemented, "Wave file...");

    auto* exportMenu = new wxMenu();
    exportMenu->Append(kIdExportVt2, "VTII Sample...");
    exportMenu->Append(kIdExportWave, "Wave file...");
    exportMenu->Append(kIdExportCsv, "CSV...");
    exportMenu->AppendSeparator();
    exportCurrentItem_ = exportMenu->AppendRadioItem(kIdExportScopeCurrent, "Current effect");
    exportAllItem_ = exportMenu->AppendRadioItem(kIdExportScopeAll, "All effects");
    exportCurrentItem_->Check(true);

    auto* helpMenu = new wxMenu();
    helpMenu->Append(kIdAudioSettings, "Audio settings...");
    helpMenu->AppendSeparator();
    helpMenu->Append(wxID_ABOUT, "About");

    menuBar->Append(fileMenu, "&File");
    menuBar->Append(editMenu, "&Edit");
    menuBar->Append(viewMenu, "&View");
    menuBar->Append(bankMenu, "&Bank");
    menuBar->Append(importMenu, "&Import");
    menuBar->Append(exportMenu, "&Export");
    menuBar->Append(helpMenu, "&Help");
    SetMenuBar(menuBar);

    Bind(wxEVT_MENU, &MainFrame::onNewBank, this, kIdNewBank);
    Bind(wxEVT_MENU, &MainFrame::onLoadBank, this, kIdLoadBank);
    Bind(wxEVT_MENU, &MainFrame::onSaveBank, this, kIdSaveBank);
    Bind(wxEVT_MENU, &MainFrame::onSaveBankNoNames, this, kIdSaveBankNoNames);
    Bind(wxEVT_MENU, &MainFrame::onLoadEffect, this, kIdLoadEffect);
    Bind(wxEVT_MENU, &MainFrame::onSaveEffect, this, kIdSaveEffect);
    Bind(wxEVT_MENU, &MainFrame::onExportWave, this, kIdExportWave);
    Bind(wxEVT_MENU, &MainFrame::onExportCsv, this, kIdExportCsv);
    Bind(wxEVT_MENU, &MainFrame::onExportVt2, this, kIdExportVt2);
    Bind(wxEVT_MENU, &MainFrame::onImportPsg, this, kIdImportPsg);
    Bind(wxEVT_MENU, &MainFrame::onImportVtx, this, kIdImportVtx);
    Bind(wxEVT_MENU, &MainFrame::onImportVgm, this, kIdImportVgm);
    Bind(wxEVT_MENU, &MainFrame::onMultiLoadBank, this, kIdMultiLoadBank);
    Bind(wxEVT_MENU, &MainFrame::onMultiSaveBank, this, kIdMultiSaveBank);

    Bind(wxEVT_MENU, &MainFrame::onEditCut, this, kIdEditCut);
    Bind(wxEVT_MENU, &MainFrame::onEditCopy, this, kIdEditCopy);
    Bind(wxEVT_MENU, &MainFrame::onEditPaste, this, kIdEditPaste);
    Bind(wxEVT_MENU, &MainFrame::onEditDelete, this, kIdEditDelete);
    Bind(wxEVT_MENU, &MainFrame::onEditSelectAll, this, kIdEditSelectAll);
    Bind(wxEVT_MENU, &MainFrame::onEditUnselectAll, this, kIdEditUnselectAll);
    Bind(wxEVT_MENU, &MainFrame::onEditInverseSelection, this, kIdEditInverseSelection);

    Bind(wxEVT_MENU,
         [this](wxCommandEvent& event) { setPianoWindowVisible(event.IsChecked()); },
         kIdViewPiano);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { periodLinear_ = true; refreshView(); }, kIdViewLinear);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { periodLinear_ = false; refreshView(); }, kIdViewLog);

    Bind(wxEVT_MENU, [this](wxCommandEvent&) { setCurrentEffect(0); }, kIdFirstEffect);
    Bind(wxEVT_MENU, &MainFrame::onPrevEffect, this, kIdPrevEffect);
    Bind(wxEVT_MENU, &MainFrame::onNextEffect, this, kIdNextEffect);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { setCurrentEffect(bank_.effectCount() - 1); }, kIdLastEffect);
    Bind(wxEVT_MENU, &MainFrame::onAddEffect, this, kIdAddEffect);
    Bind(wxEVT_MENU, &MainFrame::onInsertEffect, this, kIdInsertEffect);
    Bind(wxEVT_MENU, &MainFrame::onDeleteEffect, this, kIdDeleteEffect);

    Bind(wxEVT_MENU,
         [this](wxCommandEvent&) {
             const auto& fx = bank_.effect(currentEffect_);
             const auto realLen = bank_.effectRealLength(currentEffect_);
             std::vector<AudioEngine::FrameData> frames;
             frames.reserve(realLen);
             for (std::size_t i = 0; i < realLen; ++i) {
                 const auto& cell = fx.frames[i];
                 frames.push_back({cell.tone, cell.noise, cell.volume, cell.toneEnable, cell.noiseEnable});
             }
             audioEngine_.play(frames);
         },
         kIdPlayEffect);

    Bind(wxEVT_MENU,
         [this](wxCommandEvent&) {
             audioEngine_.stop();
         },
         kIdStopEffect);

        Bind(wxEVT_MENU,
            [this](wxCommandEvent&) {
               wxMessageBox("This command is not implemented yet in the wx port.",
                         "Not implemented",
                         wxOK | wxICON_INFORMATION,
                         this);
            },
            kIdNotImplemented);

    Bind(wxEVT_MENU,
         [this](wxCommandEvent&) {
             // Enumerate devices
             const auto devNames = AudioEngine::enumerateDevices();
             const AudioConfig& cur = audioEngine_.config();

             wxDialog dlg(this, wxID_ANY, "Audio settings",
                          wxDefaultPosition, wxDefaultSize,
                          wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);
             auto* vs = new wxBoxSizer(wxVERTICAL);

             // Output device
             auto* devLabel = new wxStaticText(&dlg, wxID_ANY, "Output device:");
             auto* devChoice = new wxChoice(&dlg, wxID_ANY);
             devChoice->Append("Default");
             for (const auto& n : devNames) devChoice->Append(n);
             devChoice->SetSelection(0);

             // Sample rate
             auto* rateLabel = new wxStaticText(&dlg, wxID_ANY, "Sample rate:");
             auto* rateChoice = new wxChoice(&dlg, wxID_ANY);
             const int kRates[] = {22050, 44100, 48000, 96000};
             int selRate = 0;
             for (int i = 0; i < 4; ++i) {
                 rateChoice->Append(wxString::Format("%d Hz", kRates[i]));
                 if (kRates[i] == cur.sampleRate) selRate = i;
             }
             rateChoice->SetSelection(selRate);

             // Volume
             auto* volLabel  = new wxStaticText(&dlg, wxID_ANY, "Volume:");
             auto* volSlider = new wxSlider(&dlg, wxID_ANY, cur.volume, 0, 100,
                                            wxDefaultPosition, wxDefaultSize,
                                            wxSL_HORIZONTAL | wxSL_LABELS);

             auto* grid = new wxFlexGridSizer(3, 2, 6, 10);
             grid->AddGrowableCol(1, 1);
             grid->Add(devLabel,  0, wxALIGN_CENTER_VERTICAL);
             grid->Add(devChoice, 1, wxEXPAND);
             grid->Add(rateLabel, 0, wxALIGN_CENTER_VERTICAL);
             grid->Add(rateChoice,1, wxEXPAND);
             grid->Add(volLabel,  0, wxALIGN_CENTER_VERTICAL);
             grid->Add(volSlider, 1, wxEXPAND);

             vs->Add(grid, 1, wxEXPAND | wxALL, 12);
             vs->Add(dlg.CreateStdDialogButtonSizer(wxOK | wxCANCEL), 0,
                     wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);
             dlg.SetSizerAndFit(vs);
             dlg.SetMinSize(dlg.FromDIP(wxSize(340, -1)));

             if (dlg.ShowModal() != wxID_OK) return;

             AudioConfig cfg = cur;
             cfg.sampleRate = kRates[rateChoice->GetSelection()];
             cfg.volume     = volSlider->GetValue();
             // Device: selection 0 = default
             cfg.useDefaultDevice = true;
             audioEngine_.reconfigure(cfg);
         },
         kIdAudioSettings);

    Bind(wxEVT_MENU,
         [this](wxCommandEvent&) {
             wxMessageBox("AY Sound FX Editor wx port (work in progress)", "About", wxOK | wxICON_INFORMATION, this);
         },
         wxID_ABOUT);

    Bind(wxEVT_MENU, [this](wxCommandEvent&) { Close(true); }, wxID_EXIT);

    wxAcceleratorEntry entries[10];
    entries[0].Set(wxACCEL_CTRL, static_cast<int>('N'), kIdNewBank);
    entries[1].Set(wxACCEL_CTRL, static_cast<int>('O'), kIdLoadBank);
    entries[2].Set(wxACCEL_CTRL, static_cast<int>('S'), kIdSaveBank);
    entries[3].Set(wxACCEL_CTRL, static_cast<int>('A'), kIdEditSelectAll);
    entries[4].Set(wxACCEL_CTRL, static_cast<int>('I'), kIdEditInverseSelection);
    entries[5].Set(wxACCEL_CTRL, static_cast<int>('X'), kIdEditCut);
    entries[6].Set(wxACCEL_CTRL, static_cast<int>('C'), kIdEditCopy);
    entries[7].Set(wxACCEL_CTRL, static_cast<int>('V'), kIdEditPaste);
    entries[8].Set(wxACCEL_NORMAL, WXK_NUMPAD_ADD, kIdNextEffect);
    entries[9].Set(wxACCEL_NORMAL, WXK_NUMPAD_SUBTRACT, kIdPrevEffect);
    SetAcceleratorTable(wxAcceleratorTable(10, entries));
}

void MainFrame::createContent() {
    auto* panel = new wxPanel(this);
    const int pad = panel->FromDIP(1);
    const int padOuter = panel->FromDIP(4);
    const int toolSide = panel->FromDIP(22);
    auto* rootSizer = new wxBoxSizer(wxVERTICAL);

    auto* topBar = new wxBoxSizer(wxHORIZONTAL);
    playButton_ = new wxBitmapButton(panel,
                                     kIdPlayEffect,
                                     wxBitmapBundle::FromBitmap(MakePlayBitmap(panel)),
                                     wxDefaultPosition,
                                     wxSize(toolSide, toolSide));
    stopButton_ = new wxBitmapButton(panel,
                                     kIdStopEffect,
                                     wxBitmapBundle::FromBitmap(MakeStopBitmap(panel)),
                                     wxDefaultPosition,
                                     wxSize(toolSide, toolSide));
    playButton_->SetToolTip("Play effect [Enter]");
    stopButton_->SetToolTip("Stop effect [Space]");
    topBar->Add(playButton_, 0, wxALL, pad);
    topBar->Add(stopButton_, 0, wxALL, pad);
    pianoButton_ = new wxBitmapToggleButton(panel,
                                            kIdViewPiano,
                                            wxBitmapBundle::FromBitmap(MakePianoBitmap(panel)),
                                            wxDefaultPosition,
                                            wxSize(toolSide, toolSide));
    pianoButton_->SetToolTip("Piano input [Ctrl+. or Ctrl+P]");
    topBar->Add(pianoButton_, 0, wxALL, pad);
    topBar->AddSpacer(panel->FromDIP(8));

    addButton_ = new wxButton(panel, kIdAddEffect, "Add");
    delButton_ = new wxButton(panel, kIdDeleteEffect, "Del");
    firstButton_ = new wxButton(panel, kIdFirstEffect, "<<");
    prevButton_ = new wxButton(panel, kIdPrevEffect, "<");
    nextButton_ = new wxButton(panel, kIdNextEffect, ">");
    lastButton_ = new wxButton(panel, kIdLastEffect, ">>");

    // Sized from the label's actual text extent plus a small fixed padding
    // (floored to the original DIP values) rather than a hardcoded pixel
    // width or wxButton::GetBestSize() — a fixed width tuned for one
    // platform's font metrics clips the label on another (e.g. "Add"
    // truncating to "A..." under the larger native macOS UI font), while
    // GetBestSize() over-corrects: its native macOS button chrome padding is
    // much more generous than the compact toolbar look this app wants.
    const int hPad = panel->FromDIP(14);
    auto textW = [&](const wxString& s) { return panel->GetTextExtent(s).GetWidth(); };
    const int btnH = std::max(panel->FromDIP(22), panel->GetTextExtent("Add").GetHeight() + panel->FromDIP(8));
    const int smallBtnW = std::max(panel->FromDIP(30), std::max(textW("Add"), textW("Del")) + hPad);
    const int navBtnW = std::max(panel->FromDIP(28),
        std::max({textW("<<"), textW("<"), textW(">"), textW(">>")}) + hPad);

    effectNumberText_ = new wxTextCtrl(panel,
                                       wxID_ANY,
                                       "",
                                       wxDefaultPosition,
                                       panel->FromDIP(wxSize(80, btnH)),
                                       wxTE_READONLY);
    effectNameText_ = new wxTextCtrl(panel,
                                     wxID_ANY,
                                     "",
                                     wxDefaultPosition,
                                     panel->FromDIP(wxSize(240, btnH)),
                                     wxTE_PROCESS_ENTER);

    effectNameText_->SetEditable(false);
    effectNameText_->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));

    addButton_->SetMinSize(wxSize(smallBtnW, btnH));
    delButton_->SetMinSize(wxSize(smallBtnW, btnH));
    firstButton_->SetMinSize(wxSize(navBtnW, btnH));
    prevButton_->SetMinSize(wxSize(navBtnW, btnH));
    nextButton_->SetMinSize(wxSize(navBtnW, btnH));
    lastButton_->SetMinSize(wxSize(navBtnW, btnH));

    topBar->Add(addButton_, 0, wxALL, pad);
    topBar->Add(delButton_, 0, wxALL, pad);
    topBar->Add(firstButton_, 0, wxALL, pad);
    topBar->Add(prevButton_, 0, wxALL, pad);
    topBar->Add(effectNumberText_, 0, wxALL | wxALIGN_CENTER_VERTICAL, pad);
    topBar->Add(nextButton_, 0, wxALL, pad);
    topBar->Add(lastButton_, 0, wxALL, pad);
    topBar->Add(effectNameText_, 1, wxALL | wxALIGN_CENTER_VERTICAL | wxEXPAND, pad);
    // Lines up the name field's right edge with the volume bar's right edge
    // below: the editor grid sits padOuter (4 DIP) in from the panel, and
    // ComputeBarLayout() insets the bars a further 8 DIP inside that — 12 DIP
    // total, vs. topBar's own 3 DIP (2 from rootSizer's border + 1 item pad).
    topBar->AddSpacer(panel->FromDIP(9));

    editorPanel_ = new wxPanel(panel);
    editorPanel_->SetBackgroundStyle(wxBG_STYLE_PAINT);

    rootSizer->Add(topBar, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, panel->FromDIP(2));
    rootSizer->Add(editorPanel_, 1, wxEXPAND | wxALL, padOuter);

    Bind(wxEVT_BUTTON,
         [this](wxCommandEvent&) { playEffectFrom(0); },
         kIdPlayEffect);

    Bind(wxEVT_BUTTON,
         [this](wxCommandEvent&) {
             audioEngine_.stop();
         },
         kIdStopEffect);

    Bind(wxEVT_BUTTON, &MainFrame::onAddEffect, this, kIdAddEffect);
    Bind(wxEVT_BUTTON, &MainFrame::onDeleteEffect, this, kIdDeleteEffect);
    Bind(wxEVT_BUTTON, &MainFrame::onPrevEffect, this, kIdPrevEffect);
    Bind(wxEVT_BUTTON, &MainFrame::onNextEffect, this, kIdNextEffect);
    Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { setCurrentEffect(0); }, kIdFirstEffect);
    Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { setCurrentEffect(bank_.effectCount() - 1); }, kIdLastEffect);
    Bind(wxEVT_TOGGLEBUTTON,
         [this](wxCommandEvent& event) { setPianoWindowVisible(event.IsChecked()); },
         kIdViewPiano);
    effectNameText_->Bind(wxEVT_TEXT_ENTER, &MainFrame::onApplyName, this);
    effectNameText_->Bind(wxEVT_LEFT_DOWN,
                          [this](wxMouseEvent& event) {
                              if (effectNameText_) {
                                  effectNameText_->SetEditable(true);
                                  effectNameText_->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
                                  effectNameText_->SetFocus();
                                  effectNameText_->SetInsertionPointEnd();
                              }
                              event.Skip();
                          });
    effectNameText_->Bind(wxEVT_TEXT,
                          [this](wxCommandEvent& event) {
                              if (effectNameText_ && effectNameText_->IsEditable() && currentEffect_ < bank_.effectCount()) {
                                  bank_.effect(currentEffect_).name = effectNameText_->GetValue().ToStdString();
                              }
                              event.Skip();
                          });

    editorPanel_->Bind(wxEVT_PAINT, [this](wxPaintEvent&) {
        wxAutoBufferedPaintDC dc(editorPanel_);
        drawEditor(dc);
    });

    editorPanel_->Bind(wxEVT_SIZE, [this](wxSizeEvent& e) {
        editorPanel_->Refresh();
        e.Skip();
    });

    editorPanel_->Bind(wxEVT_LEFT_DOWN, &MainFrame::onEditorMouseDown, this);
    editorPanel_->Bind(wxEVT_RIGHT_DOWN, &MainFrame::onEditorMouseDown, this);
    editorPanel_->Bind(wxEVT_MOTION, &MainFrame::onEditorMouseMove, this);
    editorPanel_->Bind(wxEVT_LEFT_UP, &MainFrame::onEditorMouseUp, this);
    editorPanel_->Bind(wxEVT_RIGHT_UP, &MainFrame::onEditorMouseUp, this);
    editorPanel_->Bind(wxEVT_MOUSEWHEEL, &MainFrame::onEditorMouseWheel, this);

    // wxEVT_CHAR_HOOK fires on the frame regardless of which child currently
    // holds keyboard focus. This is required because the editor panel grabs
    // focus on mouse-down; a plain wxEVT_KEY_DOWN bound to the frame would then
    // never receive arrow/hex/T/N key presses.
    Bind(wxEVT_CHAR_HOOK, &MainFrame::onKeyDown, this);

    // Fires when the system/app appearance changes (e.g. macOS/Windows dark
    // mode toggle) so the custom-painted editor canvas and hand-drawn toolbar
    // icons — which native theming doesn't reach — can be repainted to match.
    Bind(wxEVT_SYS_COLOUR_CHANGED, &MainFrame::onSysColourChanged, this);

    panel->SetSizer(rootSizer);
}

void MainFrame::refreshView() {
    if (!editorPanel_) {
        return;
    }

    if (currentEffect_ >= bank_.effectCount()) {
        currentEffect_ = 0;
    }

    auto& fx = bank_.effect(currentEffect_);
    if (currentY_ >= fx.frames.size()) {
        currentY_ = fx.frames.size() - 1;
    }
    clampView();

    const auto realLen = bank_.effectRealLength(currentEffect_);

    if (effectNumberText_) {
        effectNumberText_->SetValue(wxString::Format("%03u/%03u",
            static_cast<unsigned>(currentEffect_ + 1),
            static_cast<unsigned>(bank_.effectCount())));
    }

    if (effectNameText_ && (!effectNameText_->HasFocus() || !effectNameText_->IsEditable())) {
        effectNameText_->SetValue(fx.name);
        effectNameText_->SetForegroundColour(isPlaceholderEffectName(fx.name)
            ? wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT)
            : wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
    }

    if (firstButton_) {
        firstButton_->Enable(currentEffect_ > 0);
    }
    if (prevButton_) {
        prevButton_->Enable(currentEffect_ > 0);
    }
    if (nextButton_) {
        nextButton_->Enable(currentEffect_ + 1 < bank_.effectCount());
    }
    if (lastButton_) {
        lastButton_->Enable(currentEffect_ + 1 < bank_.effectCount());
    }
    if (delButton_) {
        delButton_->Enable(bank_.effectCount() > 1);
    }

    wxString summary;
    summary << "Frames used: " << static_cast<int>(realLen)
            << " | Cursor: " << static_cast<int>(currentY_);

    if (GetStatusBar()) {
        SetStatusText(summary);
    }

    if (editorPanel_) {
        editorPanel_->Refresh();
    }

    updateTitle();
}

void MainFrame::updateTitle() {
    const wxString title = wxString::Format("AY Sound FX Editor v0.6 [%s]", bankPath_.filename().string());
    SetTitle(title);
}

void MainFrame::onNewBank(wxCommandEvent& event) {
    (void)event;
    bank_.reset();
    currentEffect_ = 0;
    currentOffset_ = 0;
    currentY_ = 0;
    currentX_ = 0;
    bankPath_ = "noname.afb";
    clipboard_.clear();
    refreshView();
}

void MainFrame::onLoadBank(wxCommandEvent& event) {
    (void)event;

    wxFileDialog dlg(this,
                     "Load bank",
                     wxEmptyString,
                     bankPath_.filename().string(),
                     "AYFX bank (*.afb)|*.afb|All files (*.*)|*.*",
                     wxFD_OPEN | wxFD_FILE_MUST_EXIST);

    if (dlg.ShowModal() != wxID_OK) {
        return;
    }

    const std::filesystem::path selected = dlg.GetPath().ToStdString();
    if (!bank_.loadBank(selected)) {
        wxMessageBox("Can't load bank.", "Error", wxOK | wxICON_ERROR, this);
        return;
    }

    bankPath_ = selected;
    currentEffect_ = 0;
    currentOffset_ = 0;
    currentY_ = 0;
    currentX_ = 0;
    clipboard_.clear();
    refreshView();
}

void MainFrame::onSaveBank(wxCommandEvent& event) {
    (void)event;

    wxFileDialog dlg(this,
                     "Save bank",
                     wxEmptyString,
                     bankPath_.filename().string(),
                     "AYFX bank (*.afb)|*.afb|All files (*.*)|*.*",
                     wxFD_SAVE | wxFD_OVERWRITE_PROMPT);

    if (dlg.ShowModal() != wxID_OK) {
        return;
    }

    const std::filesystem::path selected = dlg.GetPath().ToStdString();
    if (!bank_.saveBank(selected, true)) {
        wxMessageBox("Can't save bank.", "Error", wxOK | wxICON_ERROR, this);
        return;
    }

    bankPath_ = selected;
    refreshView();
}

void MainFrame::onSaveBankNoNames(wxCommandEvent& event) {
    (void)event;

    wxFileDialog dlg(this,
                     "Save bank without names",
                     wxEmptyString,
                     bankPath_.filename().string(),
                     "AYFX bank (*.afb)|*.afb|All files (*.*)|*.*",
                     wxFD_SAVE | wxFD_OVERWRITE_PROMPT);

    if (dlg.ShowModal() != wxID_OK) {
        return;
    }

    const std::filesystem::path selected = dlg.GetPath().ToStdString();
    if (!bank_.saveBank(selected, false)) {
        wxMessageBox("Can't save bank.", "Error", wxOK | wxICON_ERROR, this);
        return;
    }

    bankPath_ = selected;
    refreshView();
}

void MainFrame::onLoadEffect(wxCommandEvent& event) {
    (void)event;

    wxFileDialog dlg(this,
                     "Load current effect",
                     wxEmptyString,
                     "",
                     "AYFX effect (*.afx)|*.afx|All files (*.*)|*.*",
                     wxFD_OPEN | wxFD_FILE_MUST_EXIST);

    if (dlg.ShowModal() != wxID_OK) {
        return;
    }

    const std::filesystem::path selected = dlg.GetPath().ToStdString();
    if (!bank_.loadEffect(currentEffect_, selected)) {
        wxMessageBox("Can't load effect.", "Error", wxOK | wxICON_ERROR, this);
        return;
    }

    currentOffset_ = 0;
    currentY_ = 0;
    currentX_ = 0;
    clipboard_.clear();
    refreshView();
}

void MainFrame::onSaveEffect(wxCommandEvent& event) {
    (void)event;

    const auto defaultName = sanitizeFileName(bank_.effect(currentEffect_).name);

    wxFileDialog dlg(this,
                     "Save current effect",
                     wxEmptyString,
                     defaultName + ".afx",
                     "AYFX effect (*.afx)|*.afx|All files (*.*)|*.*",
                     wxFD_SAVE | wxFD_OVERWRITE_PROMPT);

    if (dlg.ShowModal() != wxID_OK) {
        return;
    }

    const std::filesystem::path selected = dlg.GetPath().ToStdString();
    if (!bank_.saveEffect(currentEffect_, selected)) {
        wxMessageBox("Can't save effect.", "Error", wxOK | wxICON_ERROR, this);
        return;
    }
}

void MainFrame::onExportWave(wxCommandEvent& event) {
    (void)event;

    if (exportAllItem_ && exportAllItem_->IsChecked()) {
        wxDirDialog dlg(this, "Export all effects as WAV files — choose a folder");
        if (dlg.ShowModal() != wxID_OK) {
            return;
        }
        const std::filesystem::path dir = dlg.GetPath().ToStdString();
        const auto bankName = sanitizeFileName(bankPath_.stem().string());

        int exported = 0;
        int failed = 0;
        for (std::size_t i = 0; i < bank_.effectCount(); ++i) {
            if (bank_.effectRealLength(i) == 0) {
                continue;  // matches the original's multi-export: skip empty effects
            }
            const auto name = sanitizeFileName(bank_.effect(i).name);
            const auto path = dir / (wxString::Format("%s_%03zu_%s.wav", bankName, i, name).ToStdString());
            if (exportEffectWave(i, path)) {
                ++exported;
            } else {
                ++failed;
            }
        }

        if (failed > 0) {
            wxMessageBox(wxString::Format("Exported %d effect(s); %d failed.", exported, failed),
                         "Export wave", wxOK | wxICON_WARNING, this);
        } else {
            wxMessageBox(wxString::Format("Exported %d effect(s).", exported),
                         "Export wave", wxOK | wxICON_INFORMATION, this);
        }
        return;
    }

    const auto defaultName = sanitizeFileName(bank_.effect(currentEffect_).name);

    wxFileDialog dlg(this,
                     "Export current effect as WAV",
                     wxEmptyString,
                     defaultName + ".wav",
                     "WAV file (*.wav)|*.wav|All files (*.*)|*.*",
                     wxFD_SAVE | wxFD_OVERWRITE_PROMPT);

    if (dlg.ShowModal() != wxID_OK) {
        return;
    }

    const std::filesystem::path selected = dlg.GetPath().ToStdString();
    if (!exportEffectWave(currentEffect_, selected)) {
        wxMessageBox("Can't export effect (it may be empty).", "Error", wxOK | wxICON_ERROR, this);
    }
}

void MainFrame::onExportCsv(wxCommandEvent& event) {
    (void)event;

    if (exportAllItem_ && exportAllItem_->IsChecked()) {
        wxDirDialog dlg(this, "Export all effects as CSV files — choose a folder");
        if (dlg.ShowModal() != wxID_OK) {
            return;
        }
        const std::filesystem::path dir = dlg.GetPath().ToStdString();
        const auto bankName = sanitizeFileName(bankPath_.stem().string());

        int exported = 0;
        int failed = 0;
        for (std::size_t i = 0; i < bank_.effectCount(); ++i) {
            if (bank_.effectRealLength(i) == 0) {
                continue;  // matches the original's multi-export: skip empty effects
            }
            const auto name = sanitizeFileName(bank_.effect(i).name);
            const auto path = dir / (wxString::Format("%s_%03zu_%s.csv", bankName, i, name).ToStdString());
            if (exportEffectCsv(i, path)) {
                ++exported;
            } else {
                ++failed;
            }
        }

        if (failed > 0) {
            wxMessageBox(wxString::Format("Exported %d effect(s); %d failed.", exported, failed),
                         "Export CSV", wxOK | wxICON_WARNING, this);
        } else {
            wxMessageBox(wxString::Format("Exported %d effect(s).", exported),
                         "Export CSV", wxOK | wxICON_INFORMATION, this);
        }
        return;
    }

    const auto defaultName = sanitizeFileName(bank_.effect(currentEffect_).name);

    wxFileDialog dlg(this,
                     "Export current effect as CSV",
                     wxEmptyString,
                     defaultName + ".csv",
                     "CSV file (*.csv)|*.csv|All files (*.*)|*.*",
                     wxFD_SAVE | wxFD_OVERWRITE_PROMPT);

    if (dlg.ShowModal() != wxID_OK) {
        return;
    }

    const std::filesystem::path selected = dlg.GetPath().ToStdString();
    if (!exportEffectCsv(currentEffect_, selected)) {
        wxMessageBox("Can't export effect (it may be empty).", "Error", wxOK | wxICON_ERROR, this);
    }
}

void MainFrame::onExportVt2(wxCommandEvent& event) {
    (void)event;

    // Base note picker, shown once regardless of scope (matches the
    // original's MExportVT2Click: FormVortexExp is shown up front, and the
    // chosen BaseNote is then reused for every effect in a batch export).
    int baseNote = 5 * 12;  // original default: FormVortexExp::FormCreate sets BaseNote=5*12
    {
        wxDialog dlg(this, wxID_ANY, "Export Vortex Tracker II instrument",
                     wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE);
        auto* vs = new wxBoxSizer(wxVERTICAL);

        static const char* const kNoteNames[12] = {
            "C-", "C#", "D-", "D#", "E-", "F-", "F#", "G-", "G#", "A-", "A#", "B-"};

        auto* noteLabel = new wxStaticText(&dlg, wxID_ANY, wxEmptyString,
                                           wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER_HORIZONTAL);
        auto updateNoteLabel = [&]() {
            noteLabel->SetLabel(wxString::Format("Base note: %s%d", kNoteNames[baseNote % 12], baseNote / 12));
        };
        updateNoteLabel();

        auto* btnDownOct  = new wxButton(&dlg, wxID_ANY, "Octave -");
        auto* btnDownSemi = new wxButton(&dlg, wxID_ANY, "Semi -");
        auto* btnUpSemi   = new wxButton(&dlg, wxID_ANY, "Semi +");
        auto* btnUpOct    = new wxButton(&dlg, wxID_ANY, "Octave +");

        auto adjust = [&](int delta) {
            baseNote = std::clamp(baseNote + delta, 1 * 12, 8 * 12);
            updateNoteLabel();
        };
        btnDownOct->Bind(wxEVT_BUTTON, [&](wxCommandEvent&) { adjust(-12); });
        btnDownSemi->Bind(wxEVT_BUTTON, [&](wxCommandEvent&) { adjust(-1); });
        btnUpSemi->Bind(wxEVT_BUTTON, [&](wxCommandEvent&) { adjust(1); });
        btnUpOct->Bind(wxEVT_BUTTON, [&](wxCommandEvent&) { adjust(12); });

        auto* btnRow = new wxBoxSizer(wxHORIZONTAL);
        btnRow->Add(btnDownOct, 0, wxALL, 3);
        btnRow->Add(btnDownSemi, 0, wxALL, 3);
        btnRow->Add(btnUpSemi, 0, wxALL, 3);
        btnRow->Add(btnUpOct, 0, wxALL, 3);

        vs->Add(noteLabel, 0, wxALIGN_CENTER | wxALL, 12);
        vs->Add(btnRow, 0, wxALIGN_CENTER | wxALL, 6);
        vs->Add(dlg.CreateStdDialogButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxALL, 12);
        dlg.SetSizerAndFit(vs);
        dlg.SetMinSize(dlg.FromDIP(wxSize(280, -1)));

        if (dlg.ShowModal() != wxID_OK) {
            return;
        }
    }

    if (exportAllItem_ && exportAllItem_->IsChecked()) {
        wxDirDialog dlg(this, "Export all effects as VTII samples — choose a folder");
        if (dlg.ShowModal() != wxID_OK) {
            return;
        }
        const std::filesystem::path dir = dlg.GetPath().ToStdString();
        const auto bankName = sanitizeFileName(bankPath_.stem().string());

        int exported = 0;
        int failed = 0;
        for (std::size_t i = 0; i < bank_.effectCount(); ++i) {
            if (bank_.effectRealLength(i) == 0) {
                continue;  // matches the original's multi-export: skip empty effects
            }
            const auto name = sanitizeFileName(bank_.effect(i).name);
            const auto path = dir / (wxString::Format("%s_%03zu_%s.txt", bankName, i, name).ToStdString());
            if (exportEffectVt2(i, path, baseNote)) {
                ++exported;
            } else {
                ++failed;
            }
        }

        if (failed > 0) {
            wxMessageBox(wxString::Format("Exported %d effect(s); %d failed.", exported, failed),
                         "Export VTII sample", wxOK | wxICON_WARNING, this);
        } else {
            wxMessageBox(wxString::Format("Exported %d effect(s).", exported),
                         "Export VTII sample", wxOK | wxICON_INFORMATION, this);
        }
        return;
    }

    const auto defaultName = sanitizeFileName(bank_.effect(currentEffect_).name);

    wxFileDialog dlg(this,
                     "Export Vortex Tracker II instrument",
                     wxEmptyString,
                     defaultName + ".txt",
                     "Text file (*.txt)|*.txt|All files (*.*)|*.*",
                     wxFD_SAVE | wxFD_OVERWRITE_PROMPT);

    if (dlg.ShowModal() != wxID_OK) {
        return;
    }

    const std::filesystem::path selected = dlg.GetPath().ToStdString();
    if (!exportEffectVt2(currentEffect_, selected, baseNote)) {
        wxMessageBox("Can't export effect (it may be empty).", "Error", wxOK | wxICON_ERROR, this);
    }
}

void MainFrame::onImportPsg(wxCommandEvent& event) {
    (void)event;

    wxFileDialog fileDlg(this,
                         "Load PSG file",
                         wxEmptyString,
                         "",
                         "PSG AY register dump (*.psg)|*.psg|All files (*.*)|*.*",
                         wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (fileDlg.ShowModal() != wxID_OK) {
        return;
    }
    const std::filesystem::path selected = fileDlg.GetPath().ToStdString();

    // Channel select, shown after the file picker (matches the original's
    // MImportPSGClick order).
    int channel = -1;
    if (!showAyChannelSelectDialog(channel)) {
        return;
    }

    if (!importEffectPsg(currentEffect_, selected, channel)) {
        wxMessageBox("Can't import PSG file.", "Error", wxOK | wxICON_ERROR, this);
        return;
    }

    currentOffset_ = 0;
    currentY_ = 0;
    currentX_ = 0;
    clipboard_.clear();
    refreshView();
}

void MainFrame::onImportVtx(wxCommandEvent& event) {
    (void)event;

    wxFileDialog fileDlg(this,
                         "Load VTX file",
                         wxEmptyString,
                         "",
                         "Vortex Tracker file (*.vtx)|*.vtx|All files (*.*)|*.*",
                         wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (fileDlg.ShowModal() != wxID_OK) {
        return;
    }
    const std::filesystem::path selected = fileDlg.GetPath().ToStdString();

    int channel = -1;
    if (!showAyChannelSelectDialog(channel)) {
        return;
    }

    if (!importEffectVtx(currentEffect_, selected, channel)) {
        wxMessageBox("Can't import VTX file.", "Error", wxOK | wxICON_ERROR, this);
        return;
    }

    currentOffset_ = 0;
    currentY_ = 0;
    currentX_ = 0;
    clipboard_.clear();
    refreshView();
}

void MainFrame::onImportVgm(wxCommandEvent& event) {
    (void)event;

    wxFileDialog fileDlg(this,
                         "Load VGM file",
                         wxEmptyString,
                         "",
                         "Sega hardware sound log (*.vgm)|*.vgm|All files (*.*)|*.*",
                         wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (fileDlg.ShowModal() != wxID_OK) {
        return;
    }
    const std::filesystem::path selected = fileDlg.GetPath().ToStdString();

    int channel = -1;
    bool mixNoise = true;
    if (!showSnChannelSelectDialog(channel, mixNoise)) {
        return;
    }

    if (!importEffectVgm(currentEffect_, selected, channel, mixNoise)) {
        wxMessageBox("Can't import VGM file.", "Error", wxOK | wxICON_ERROR, this);
        return;
    }

    currentOffset_ = 0;
    currentY_ = 0;
    currentX_ = 0;
    clipboard_.clear();
    refreshView();
}

bool MainFrame::showAyChannelSelectDialog(int& channel) {
    wxDialog dlg(this, wxID_ANY, "Select AY channel for import",
                 wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE);
    auto* vs = new wxBoxSizer(wxVERTICAL);

    auto* radioAuto = new wxRadioButton(&dlg, wxID_ANY, "Auto", wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
    auto* radioA = new wxRadioButton(&dlg, wxID_ANY, "Chn. A");
    auto* radioB = new wxRadioButton(&dlg, wxID_ANY, "Chn. B");
    auto* radioC = new wxRadioButton(&dlg, wxID_ANY, "Chn. C");
    radioAuto->SetValue(true);

    vs->Add(radioAuto, 0, wxALL, 4);
    vs->Add(radioA, 0, wxLEFT | wxRIGHT | wxBOTTOM, 4);
    vs->Add(radioB, 0, wxLEFT | wxRIGHT | wxBOTTOM, 4);
    vs->Add(radioC, 0, wxLEFT | wxRIGHT | wxBOTTOM, 4);
    vs->Add(dlg.CreateStdDialogButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxALL, 8);
    dlg.SetSizerAndFit(vs);

    if (dlg.ShowModal() != wxID_OK) {
        return false;
    }

    if (radioA->GetValue()) {
        channel = 0;
    } else if (radioB->GetValue()) {
        channel = 1;
    } else if (radioC->GetValue()) {
        channel = 2;
    } else {
        channel = -1;
    }
    return true;
}

bool MainFrame::showSnChannelSelectDialog(int& channel, bool& mixNoise) {
    wxDialog dlg(this, wxID_ANY, "Select SN channel for import",
                 wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE);
    auto* vs = new wxBoxSizer(wxVERTICAL);

    auto* radioAuto  = new wxRadioButton(&dlg, wxID_ANY, "Auto", wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
    auto* radioTone0 = new wxRadioButton(&dlg, wxID_ANY, "Tone #0");
    auto* radioTone1 = new wxRadioButton(&dlg, wxID_ANY, "Tone #1");
    auto* radioTone2 = new wxRadioButton(&dlg, wxID_ANY, "Tone #2");
    auto* radioNoise = new wxRadioButton(&dlg, wxID_ANY, "Noise");
    radioAuto->SetValue(true);

    auto* checkMix = new wxCheckBox(&dlg, wxID_ANY, "Mix noise");
    checkMix->SetValue(false);  // matches the original's FormSnChn default (unchecked)

    // Mixing noise into the noise channel's own export doesn't make sense
    // (matches the original disabling this checkbox for that combination).
    auto updateMixEnabled = [=]() { checkMix->Enable(!radioNoise->GetValue()); };
    for (auto* r : {radioAuto, radioTone0, radioTone1, radioTone2, radioNoise}) {
        r->Bind(wxEVT_RADIOBUTTON, [updateMixEnabled](wxCommandEvent&) { updateMixEnabled(); });
    }
    updateMixEnabled();

    vs->Add(radioAuto, 0, wxALL, 4);
    vs->Add(radioTone0, 0, wxLEFT | wxRIGHT | wxBOTTOM, 4);
    vs->Add(radioTone1, 0, wxLEFT | wxRIGHT | wxBOTTOM, 4);
    vs->Add(radioTone2, 0, wxLEFT | wxRIGHT | wxBOTTOM, 4);
    vs->Add(radioNoise, 0, wxLEFT | wxRIGHT | wxBOTTOM, 4);
    vs->Add(checkMix, 0, wxALL, 4);
    vs->Add(dlg.CreateStdDialogButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxALL, 8);
    dlg.SetSizerAndFit(vs);

    if (dlg.ShowModal() != wxID_OK) {
        return false;
    }

    if (radioTone0->GetValue()) {
        channel = 0;
    } else if (radioTone1->GetValue()) {
        channel = 1;
    } else if (radioTone2->GetValue()) {
        channel = 2;
    } else if (radioNoise->GetValue()) {
        channel = 3;
    } else {
        channel = -1;
    }
    mixNoise = checkMix->GetValue() && channel != 3;
    return true;
}

void MainFrame::onMultiLoadBank(wxCommandEvent& event) {
    (void)event;

    wxFileDialog dlg(this,
                     "Multi-load to bank",
                     wxEmptyString,
                     "",
                     "AYFX effect (*.afx)|*.afx|All files (*.*)|*.*",
                     wxFD_OPEN | wxFD_FILE_MUST_EXIST | wxFD_MULTIPLE);

    if (dlg.ShowModal() != wxID_OK) {
        return;
    }

    wxArrayString paths;
    dlg.GetPaths(paths);
    if (paths.IsEmpty()) {
        return;
    }

    // Reuse the last slot if it's empty, otherwise start appending after it
    // (mirrors the original's MFileMultiLoadClick).
    std::size_t target = bank_.effectCount() - 1;
    if (bank_.effectRealLength(target) > 0) {
        ++target;
    }
    const std::size_t firstTarget = target;

    int loaded = 0;
    for (const auto& p : paths) {
        if (target >= bank_.effectCount() && !bank_.addEffect()) {
            break;  // bank full (256 effects)
        }
        if (bank_.loadEffect(target, p.ToStdString())) {
            ++loaded;
        }
        ++target;
    }

    if (loaded < static_cast<int>(paths.size())) {
        wxMessageBox(wxString::Format("Loaded %d of %zu effect(s); bank is full at 256 effects.",
                                       loaded, paths.size()),
                     "Multi-load", wxOK | wxICON_WARNING, this);
    }

    if (loaded > 0) {
        setCurrentEffect(firstTarget);
    }
}

void MainFrame::onMultiSaveBank(wxCommandEvent& event) {
    (void)event;

    wxDirDialog dlg(this, "Multi-save from bank — choose a folder");
    if (dlg.ShowModal() != wxID_OK) {
        return;
    }
    const std::filesystem::path dir = dlg.GetPath().ToStdString();
    const auto bankName = sanitizeFileName(bankPath_.stem().string());

    int saved = 0;
    int failed = 0;
    for (std::size_t i = 0; i < bank_.effectCount(); ++i) {
        if (bank_.effectRealLength(i) == 0) {
            continue;  // matches the original's multi-save: skip empty effects
        }
        const auto name = sanitizeFileName(bank_.effect(i).name);
        const auto path = dir / (wxString::Format("%s_%03zu_%s.afx", bankName, i, name).ToStdString());
        if (bank_.saveEffect(i, path)) {
            ++saved;
        } else {
            ++failed;
        }
    }

    if (failed > 0) {
        wxMessageBox(wxString::Format("Saved %d effect(s); %d failed.", saved, failed),
                     "Multi-save", wxOK | wxICON_WARNING, this);
    } else {
        wxMessageBox(wxString::Format("Saved %d effect(s).", saved),
                     "Multi-save", wxOK | wxICON_INFORMATION, this);
    }
}

void MainFrame::onPrevEffect(wxCommandEvent& event) {
    (void)event;
    if (currentEffect_ > 0) {
        setCurrentEffect(currentEffect_ - 1);
    }
}

void MainFrame::onNextEffect(wxCommandEvent& event) {
    (void)event;
    if (currentEffect_ + 1 < bank_.effectCount()) {
        setCurrentEffect(currentEffect_ + 1);
    }
}

void MainFrame::onAddEffect(wxCommandEvent& event) {
    (void)event;
    if (!bank_.addEffect()) {
        wxMessageBox("Maximum effects reached (256).", "Bank", wxOK | wxICON_WARNING, this);
        return;
    }
    setCurrentEffect(bank_.effectCount() - 1);
}

void MainFrame::onInsertEffect(wxCommandEvent& event) {
    (void)event;
    if (!bank_.insertEffect(currentEffect_)) {
        wxMessageBox("Cannot insert effect.", "Bank", wxOK | wxICON_WARNING, this);
        return;
    }
    refreshView();
}

void MainFrame::onDeleteEffect(wxCommandEvent& event) {
    (void)event;
    if (wxMessageBox("Delete current effect?", "Confirm", wxYES_NO | wxICON_QUESTION, this) != wxYES) {
        return;
    }

    if (!bank_.deleteEffect(currentEffect_)) {
        wxMessageBox("Cannot delete effect.", "Bank", wxOK | wxICON_WARNING, this);
        return;
    }

    if (currentEffect_ >= bank_.effectCount()) {
        currentEffect_ = bank_.effectCount() - 1;
    }

    currentY_ = std::min(currentY_, BankModel::kMaxFrames - 1);
    refreshView();
}

void MainFrame::onApplyName(wxCommandEvent& event) {
    (void)event;

    if (currentEffect_ >= bank_.effectCount() || !effectNameText_) {
        return;
    }

    bank_.effect(currentEffect_).name = effectNameText_->GetValue().ToStdString();
    effectNameText_->SetEditable(false);
    editorPanel_->SetFocus();
    refreshView();  // also restores the name field's resting colour
}

void MainFrame::onEditorMouseDown(wxMouseEvent& event) {
    if (!editorPanel_) {
        return;
    }

    editorPanel_->SetFocus();

    const bool left = event.LeftIsDown();
    const bool right = event.RightIsDown();

    lastMouseX_ = event.GetX();
    lastMouseY_ = event.GetY();
    mouseHold_ = true;

    updateHoldZone(lastMouseX_, lastMouseY_, false);

    if (mouseEditAt(lastMouseX_, lastMouseY_, left, right, false)) {
        refreshView();
    }
}

void MainFrame::onEditorMouseMove(wxMouseEvent& event) {
    if (!mouseHold_) {
        return;
    }

    if (!(event.LeftIsDown() || event.RightIsDown())) {
        return;
    }

    int x = event.GetX();
    int y = event.GetY();
    const bool ctrl = event.ControlDown();

    if (ctrl) {
        x = lastMouseX_;
    }

    updateHoldZone(x, y, ctrl);

    const bool left = event.LeftIsDown();
    const bool right = event.RightIsDown();
    const int lineH = EditorLineHeight(editorPanel_);
    const int delta = std::abs(lastMouseY_ - y);

    bool redraw = false;

    // Interpolate across every line crossed since the previous motion event so
    // fast drags do not skip rows (matches the original FormMouseMove loop).
    if (lastMouseY_ >= 0 && delta >= lineH) {
        int yy = lastMouseY_;
        float xx = static_cast<float>(lastMouseX_);
        const float xs = (static_cast<float>(x) - xx) / static_cast<float>(delta);
        const int ys = (lastMouseY_ < y) ? 1 : -1;

        for (int d = delta; d >= 0; --d) {
            if (mouseEditAt(static_cast<int>(xx), yy, left, right, true)) {
                redraw = true;
            }
            yy += ys;
            xx += xs;
        }
    } else if (mouseEditAt(x, y, left, right, true)) {
        redraw = true;
    }

    if (redraw) {
        refreshView();
    }

    lastMouseX_ = x;
    lastMouseY_ = y;
}

void MainFrame::onEditorMouseUp(wxMouseEvent& event) {
    (void)event;
    mouseHold_ = false;
    curPrevLine_ = -1;
    flagDragColumn_ = -1;
    releaseHoldZone();
}

void MainFrame::onEditorMouseWheel(wxMouseEvent& event) {
    if (event.GetWheelRotation() < 0) {
        if (currentY_ + 1 < BankModel::kMaxFrames) {
            currentY_ = std::min(currentY_ + static_cast<std::size_t>(5), BankModel::kMaxFrames - 1);
            const auto visible = static_cast<std::size_t>(lineOnScreen());
            if (currentY_ >= currentOffset_ + visible) {
                currentOffset_ = currentY_ - visible + 1;
            }
        }
    } else if (currentY_ > 0) {
        currentY_ = (currentY_ > 5) ? currentY_ - 5 : 0;
        if (currentY_ < currentOffset_) {
            currentOffset_ = currentY_;
        }
    }

    clampView();
    refreshView();
}

void MainFrame::onSysColourChanged(wxSysColourChangedEvent& event) {
    event.Skip();  // let native controls restyle themselves too

    if (playButton_) {
        playButton_->SetBitmap(wxBitmapBundle::FromBitmap(MakePlayBitmap(playButton_)));
    }
    if (stopButton_) {
        stopButton_->SetBitmap(wxBitmapBundle::FromBitmap(MakeStopBitmap(stopButton_)));
    }
    if (pianoButton_) {
        pianoButton_->SetBitmap(wxBitmapBundle::FromBitmap(MakePianoBitmap(pianoButton_)));
    }
    if (editorPanel_) {
        editorPanel_->Refresh();
    }
}

void MainFrame::onKeyDown(wxKeyEvent& event) {
    // Let text fields handle their own keystrokes (name/number editing).
    if ((effectNameText_ && effectNameText_->HasFocus()) ||
        (effectNumberText_ && effectNumberText_->HasFocus())) {
        event.Skip();
        return;
    }

    const bool ctrl = event.ControlDown();
    const bool shift = event.ShiftDown();
    wxCommandEvent cmd;

    if (ctrl) {
        switch (event.GetKeyCode()) {
            case 'A':
                onEditSelectAll(cmd);
                return;
            case 'I':
                onEditInverseSelection(cmd);
                return;
            case 'X':
                onEditCut(cmd);
                return;
            case 'C':
                onEditCopy(cmd);
                return;
            case 'V':
                onEditPaste(cmd);
                return;
            case '.':
            case 'P':
                // Two alternatives standing in for the original's tilde/
                // backtick (VK_OEM_3): that's a symbol-row key whose
                // position and even availability shifts across keyboard
                // layouts (e.g. Italian) and compact/laptop keyboards.
                // Period and P are both stable across QWERTY/QWERTZ/AZERTY.
                togglePianoInput();
                return;
            default:
                break;
        }
    }

    const int key = event.GetKeyCode();

    if (key >= '0' && key <= '9') {
        handleHexInput(key - '0');
        refreshView();
        return;
    }

    if (key >= 'A' && key <= 'F') {
        handleHexInput(key - 'A' + 10);
        refreshView();
        return;
    }

    if (shift && (key == WXK_ADD || key == WXK_NUMPAD_ADD || key == '+')) {
        adjustColumn(1);
        refreshView();
        return;
    }

    if (shift && (key == WXK_SUBTRACT || key == WXK_NUMPAD_SUBTRACT || key == '-')) {
        adjustColumn(-1);
        refreshView();
        return;
    }

    switch (key) {
        case WXK_ADD:
        case WXK_NUMPAD_ADD:
        case '+':
            onNextEffect(cmd);
            return;

        case WXK_SUBTRACT:
        case WXK_NUMPAD_SUBTRACT:
        case '-':
            onPrevEffect(cmd);
            return;

        case WXK_UP:
            if (currentY_ > 0) {
                --currentY_;
                if (currentY_ < currentOffset_) {
                    currentOffset_ = currentY_;
                }
            }
            break;

        case WXK_DOWN:
            if (currentY_ + 1 < BankModel::kMaxFrames) {
                ++currentY_;
                const auto visible = static_cast<std::size_t>(lineOnScreen());
                if (currentY_ >= currentOffset_ + visible) {
                    currentOffset_ = currentY_ - visible + 1;
                }
            }
            break;

        case WXK_LEFT:
            currentX_ = (currentX_ + 2) % 3;
            break;

        case WXK_RIGHT:
            currentX_ = (currentX_ + 1) % 3;
            break;

        case WXK_PAGEUP:
            if (currentY_ > static_cast<std::size_t>(lineOnScreen())) {
                currentY_ -= static_cast<std::size_t>(lineOnScreen());
            } else {
                currentY_ = 0;
            }
            if (currentOffset_ > static_cast<std::size_t>(lineOnScreen())) {
                currentOffset_ -= static_cast<std::size_t>(lineOnScreen());
            } else {
                currentOffset_ = 0;
            }
            break;

        case WXK_PAGEDOWN:
            currentY_ = std::min(currentY_ + static_cast<std::size_t>(lineOnScreen()), BankModel::kMaxFrames - 1);
            currentOffset_ += static_cast<std::size_t>(lineOnScreen());
            break;

        case WXK_HOME:
            currentY_ = 0;
            centerView();
            break;

        case WXK_END: {
            const auto len = bank_.effectRealLength(currentEffect_);
            currentY_ = len > 0 ? len - 1 : 0;
            centerView();
            break;
        }

        case WXK_DELETE:
            onEditDelete(cmd);
            return;

        case WXK_INSERT:
            insertFrame(ctrl);
            refreshView();
            return;

        case WXK_RETURN:
        case WXK_NUMPAD_ENTER:
            // Ctrl+Enter plays from the cursor row instead of frame 0,
            // matching the original editor.
            playEffectFrom(ctrl ? currentY_ : 0);
            return;

        case WXK_SPACE: {
            wxCommandEvent stopEvt(wxEVT_BUTTON, kIdStopEffect);
            ProcessWindowEvent(stopEvt);
            return;
        }

        case 'T': {
            auto& cell = bank_.effect(currentEffect_).frames[currentY_];
            cell.toneEnable = !cell.toneEnable;
            break;
        }

        case 'N': {
            auto& cell = bank_.effect(currentEffect_).frames[currentY_];
            cell.noiseEnable = !cell.noiseEnable;
            break;
        }

        default:
            event.Skip();
            return;
    }

    if (shift) {
        bank_.effect(currentEffect_).frames[currentY_].selected = true;
    }

    clampView();
    refreshView();
}

void MainFrame::onEditCopy(wxCommandEvent& event) {
    (void)event;
    copySelection(true);
    refreshView();
}

void MainFrame::onEditCut(wxCommandEvent& event) {
    (void)event;
    copySelection(false);
    if (!deleteSelected()) {
        auto& fx = bank_.effect(currentEffect_);
        fx.frames[currentY_].selected = true;
        deleteSelected();
    }
    refreshView();
}

void MainFrame::onEditPaste(wxCommandEvent& event) {
    (void)event;
    pasteClipboard();
    refreshView();
}

void MainFrame::onEditDelete(wxCommandEvent& event) {
    (void)event;
    if (!deleteSelected()) {
        auto& fx = bank_.effect(currentEffect_);
        fx.frames[currentY_].selected = true;
        deleteSelected();
    }
    refreshView();
}

void MainFrame::onEditSelectAll(wxCommandEvent& event) {
    (void)event;
    auto& fx = bank_.effect(currentEffect_);
    const auto len = bank_.effectRealLength(currentEffect_);
    for (std::size_t i = 0; i < len; ++i) {
        fx.frames[i].selected = true;
    }
    refreshView();
}

void MainFrame::onEditUnselectAll(wxCommandEvent& event) {
    (void)event;
    clearSelection();
    refreshView();
}

void MainFrame::onEditInverseSelection(wxCommandEvent& event) {
    (void)event;
    auto& fx = bank_.effect(currentEffect_);
    const auto len = bank_.effectRealLength(currentEffect_);
    for (std::size_t i = 0; i < len; ++i) {
        fx.frames[i].selected = !fx.frames[i].selected;
    }
    refreshView();
}

std::string MainFrame::sanitizeFileName(const std::string& value) {
    std::string out = value;
    const std::array<char, 9> badChars = {'\\', '/', ':', '*', '?', '"', '<', '>', '|'};

    std::replace_if(
        out.begin(),
        out.end(),
        [&badChars](char c) { return std::find(badChars.begin(), badChars.end(), c) != badChars.end(); },
        '_');

    if (out.empty()) {
        out = "effect";
    }

    return out;
}

bool MainFrame::isPlaceholderEffectName(const std::string& name) {
    if (name.empty()) {
        return true;
    }

    // Matches BankModel::makeDefaultName ("nonameNNN") and ::insertEffect
    // ("insertedNNN") — auto-generated names, not something the user (or an
    // import) actually set.
    auto matchesGeneratedPattern = [&](std::string_view prefix) {
        if (name.size() <= prefix.size() || name.compare(0, prefix.size(), prefix) != 0) {
            return false;
        }
        return std::all_of(name.begin() + static_cast<std::ptrdiff_t>(prefix.size()), name.end(),
                            [](unsigned char c) { return std::isdigit(c) != 0; });
    };

    return matchesGeneratedPattern("noname") || matchesGeneratedPattern("inserted");
}

void MainFrame::setCurrentEffect(std::size_t index) {
    if (index >= bank_.effectCount()) {
        return;
    }

    currentEffect_ = index;
    currentOffset_ = 0;
    currentY_ = 0;
    currentX_ = 0;
    refreshView();
}

int MainFrame::lineOnScreen() const {
    if (!editorPanel_) {
        return 1;
    }

    const auto size = editorPanel_->GetClientSize();
    const int yOff = Dip(editorPanel_, EditorLayout::kYOff);
    const int head = Dip(editorPanel_, EditorLayout::kHead);
    const int lineH = EditorLineHeight(editorPanel_);
    return std::max(1, (size.GetHeight() - yOff - head) / std::max(1, lineH));
}

void MainFrame::clampView() {
    const auto visible = static_cast<std::size_t>(lineOnScreen());
    const auto maxOffset = BankModel::kMaxFrames > visible ? BankModel::kMaxFrames - visible : 0;

    if (currentOffset_ > maxOffset) {
        currentOffset_ = maxOffset;
    }
}

void MainFrame::ensureCursorVisible() {
    const auto visible = static_cast<std::size_t>(lineOnScreen());

    if (currentY_ < currentOffset_) {
        currentOffset_ = currentY_;
    } else if (currentY_ >= currentOffset_ + visible) {
        currentOffset_ = currentY_ - visible + 1;
    }

    clampView();
}

void MainFrame::centerView() {
    const auto visible = static_cast<std::size_t>(lineOnScreen());

    if (currentY_ < currentOffset_ || currentY_ >= currentOffset_ + visible) {
        if (visible > 2) {
            currentOffset_ = currentY_ - (visible / 2);
        } else {
            currentOffset_ = currentY_;
        }
    }

    if (currentOffset_ + visible > BankModel::kMaxFrames) {
        currentOffset_ = BankModel::kMaxFrames - visible;
    }
}

void MainFrame::clearSelection() {
    auto& fx = bank_.effect(currentEffect_);
    for (auto& frame : fx.frames) {
        frame.selected = false;
    }
}

void MainFrame::insertFrame(bool cloneCurrent) {
    auto& fx = bank_.effect(currentEffect_);
    auto& frames = fx.frames;

    for (std::size_t i = frames.size() - 1; i > currentY_; --i) {
        frames[i] = frames[i - 1];
    }

    if (!cloneCurrent) {
        frames[currentY_] = AyfxCell{};
    }
}

bool MainFrame::deleteSelected() {
    auto& fx = bank_.effect(currentEffect_);
    auto& frames = fx.frames;

    bool deleted = false;
    bool done = false;

    while (!done) {
        done = true;
        for (std::size_t i = 0; i < frames.size(); ++i) {
            if (!frames[i].selected) {
                continue;
            }

            for (std::size_t j = i; j + 1 < frames.size(); ++j) {
                frames[j] = frames[j + 1];
            }
            frames.back() = AyfxCell{};
            deleted = true;
            done = false;
            break;
        }
    }

    centerView();
    return deleted;
}

void MainFrame::copySelection(bool clearAfterCopy) {
    clipboard_.clear();

    auto& fx = bank_.effect(currentEffect_);
    for (auto& frame : fx.frames) {
        if (!frame.selected) {
            continue;
        }

        AyfxCell copy = frame;
        if (clearAfterCopy) {
            frame.selected = false;
        }
        copy.selected = false;
        clipboard_.push_back(copy);
    }

    if (clipboard_.empty()) {
        AyfxCell copy = fx.frames[currentY_];
        copy.selected = false;
        clipboard_.push_back(copy);
    }
}

void MainFrame::pasteClipboard() {
    if (clipboard_.empty()) {
        return;
    }

    for (std::size_t i = 0; i < clipboard_.size(); ++i) {
        insertFrame(false);
    }

    auto& fx = bank_.effect(currentEffect_);
    auto& frames = fx.frames;

    const auto len = std::min(clipboard_.size(), frames.size() - currentY_);
    for (std::size_t i = 0; i < len; ++i) {
        frames[currentY_ + i] = clipboard_[i];
        frames[currentY_ + i].selected = false;
    }
}

void MainFrame::adjustColumn(int delta) {
    auto& fx = bank_.effect(currentEffect_);

    for (auto& frame : fx.frames) {
        if (!frame.selected) {
            continue;
        }

        switch (currentX_) {
            case 0:
                frame.tone = static_cast<std::uint16_t>(std::clamp(static_cast<int>(frame.tone) + delta, 0, 4095));
                break;

            case 1:
                frame.noise = static_cast<std::uint8_t>(std::clamp(static_cast<int>(frame.noise) + delta, 0, 31));
                break;

            case 2:
                frame.volume = static_cast<std::uint8_t>(std::clamp(static_cast<int>(frame.volume) + delta, 0, 15));
                break;

            default:
                break;
        }
    }
}

void MainFrame::handleHexInput(int nibble) {
    auto& cell = bank_.effect(currentEffect_).frames[currentY_];

    switch (currentX_) {
        case 0:
            cell.tone = static_cast<std::uint16_t>(((cell.tone << 4) | nibble) & 0x0FFFu);
            break;

        case 1: {
            const int n = ((cell.noise << 4) | nibble) & 0xFF;
            cell.noise = static_cast<std::uint8_t>(n <= 0x1F ? n : nibble);
            break;
        }

        case 2:
            cell.volume = static_cast<std::uint8_t>(nibble & 0x0F);
            break;

        default:
            break;
    }
}

bool MainFrame::mouseEditAt(int x, int y, bool left, bool right, bool hold) {
    auto& fx = bank_.effect(currentEffect_);

    const int xOff = Dip(editorPanel_, EditorLayout::kXOff);
    const int yOff = Dip(editorPanel_, EditorLayout::kYOff);
    const int head = Dip(editorPanel_, EditorLayout::kHead);
    const int lineH = EditorLineHeight(editorPanel_);
    const auto bl = ComputeBarLayout(editorPanel_);
    const int toneOff  = bl.toneOff;
    const int toneW    = bl.toneW;
    const int noiseOff = bl.noiseOff;
    const int noiseW   = bl.noiseW;
    const int volOff   = bl.volOff;
    const int volW     = bl.volW;
    const int tFlagOff = Dip(editorPanel_, EditorLayout::kTFlagOff);
    const int nFlagOff = Dip(editorPanel_, EditorLayout::kNFlagOff);
    const int tColOff = Dip(editorPanel_, EditorLayout::kTColOff);
    const int nColOff = Dip(editorPanel_, EditorLayout::kNColOff);
    const int vColOff = Dip(editorPanel_, EditorLayout::kVColOff);
    const int barPad = Dip(editorPanel_, 2);

    if (y < yOff + head ||
        y >= yOff + head + lineOnScreen() * lineH) {
        return false;
    }

    const std::size_t row = currentOffset_ + static_cast<std::size_t>((y - yOff - head) / lineH);
    if (row >= fx.frames.size()) {
        return false;
    }

    auto& cell = fx.frames[row];
    const int rowY = yOff + head + static_cast<int>(row - currentOffset_) * lineH;

    const wxString posText = wxString::Format("%03X", static_cast<unsigned>(row));
    const wxString toneText = wxString::Format("%03X", static_cast<unsigned>(cell.tone & 0x0FFFu));
    const wxString noiseText = wxString::Format("%02X", static_cast<unsigned>(cell.noise & 0x1Fu));
    const wxString volumeText = wxString::Format("%X", static_cast<unsigned>(cell.volume & 0x0Fu));

    const wxRect posRect = MakeEditorTextRect(editorPanel_, xOff, rowY, posText, lineH);
    // Full-column hit rects for T/N so they are easy to click and drag
    const wxRect toneFlagRect(xOff + tFlagOff, rowY, Dip(editorPanel_, EditorLayout::kNFlagOff - EditorLayout::kTFlagOff), lineH);
    const wxRect noiseFlagRect(xOff + nFlagOff, rowY, Dip(editorPanel_, EditorLayout::kTColOff  - EditorLayout::kNFlagOff), lineH);
    const wxRect toneRect = MakeEditorTextRect(editorPanel_, xOff + tColOff, rowY, toneText, lineH);
    const wxRect noiseRect = MakeEditorTextRect(editorPanel_, xOff + nColOff, rowY, noiseText, lineH);
    const wxRect volumeRect = MakeEditorTextRect(editorPanel_, xOff + vColOff, rowY, volumeText, lineH);

    // Bar drags edit the value but must NOT move the keyboard cursor
    // (currentY_/currentX_); only clicks on the numeric columns do that.
    if ((left || right) && x >= xOff + toneOff - barPad && x <= xOff + toneOff + toneW) {
        const int rel = std::clamp(x - xOff - toneOff, -barPad, toneW);
        if (rel < 0) {
            cell.tone = 0;
        } else {
            cell.tone = static_cast<std::uint16_t>(std::clamp(widthToPeriod(rel, toneW, periodLinear_), 0, 4095));
        }
        return true;
    }

    if ((left || right) && x >= xOff + noiseOff - barPad && x <= xOff + noiseOff + noiseW) {
        const int rel = x - xOff - noiseOff;
        if (rel < 0) {
            cell.noise = 0;
        } else {
            cell.noise = static_cast<std::uint8_t>(std::clamp(1 + (rel * 31) / noiseW, 0, 31));
        }
        return true;
    }

    if ((left || right) && x >= xOff + volOff - barPad && x <= xOff + volOff + volW) {
        const int rel = x - xOff - volOff;
        if (rel < 0) {
            cell.volume = 0;
        } else {
            cell.volume = static_cast<std::uint8_t>(std::clamp(1 + (rel * 15) / volW, 0, 15));
        }
        return true;
    }

    if (toneFlagRect.Contains(x, y)) {
        if (!hold) {
            // left = force ON, right = force OFF; drag paints the same value across rows
            flagDragColumn_ = 0;
            flagDragValue_  = left;
        }
        if (flagDragColumn_ == 0 && (left || right)) {
            cell.toneEnable = flagDragValue_;
            return true;
        }
    }

    if (noiseFlagRect.Contains(x, y)) {
        if (!hold) {
            flagDragColumn_ = 1;
            flagDragValue_  = left;
        }
        if (flagDragColumn_ == 1 && (left || right)) {
            cell.noiseEnable = flagDragValue_;
            return true;
        }
    }

    if (!hold) {
        if (toneRect.Contains(x, y)) {
            currentY_ = row;
            currentX_ = 0;
            return true;
        }

        if (noiseRect.Contains(x, y)) {
            currentY_ = row;
            currentX_ = 1;
            return true;
        }

        if (volumeRect.Contains(x, y)) {
            currentY_ = row;
            currentX_ = 2;
            return true;
        }
    }

    // Pos-column selection: while dragging, only act when the line actually
    // changes, so a held button does not re-toggle the same row repeatedly.
    if (curPrevLine_ != static_cast<int>(row) || !hold) {
        curPrevLine_ = static_cast<int>(row);
        if (posRect.Contains(x, y)) {
            if (left) {
                cell.selected = true;
                return true;
            }
            if (right) {
                cell.selected = false;
                return true;
            }
        }
    }

    return false;
}

void MainFrame::updateHoldZone(int x, int y, bool force) {
    if (!editorPanel_) {
        return;
    }

    bool hold = false;
    int xmin = 0;
    int xmax = 0;

    const int xOff = Dip(editorPanel_, EditorLayout::kXOff);
    const int yOff = Dip(editorPanel_, EditorLayout::kYOff);
    const int head = Dip(editorPanel_, EditorLayout::kHead);
    const int lineH = EditorLineHeight(editorPanel_);
    const int tFlagOff = Dip(editorPanel_, EditorLayout::kTFlagOff);
    const int nFlagOff = Dip(editorPanel_, EditorLayout::kNFlagOff);
    const auto bl = ComputeBarLayout(editorPanel_);
    const int toneOff  = bl.toneOff;
    const int toneW    = bl.toneW;
    const int noiseOff = bl.noiseOff;
    const int noiseW   = bl.noiseW;
    const int volOff   = bl.volOff;
    const int volW     = bl.volW;
    const int barPad = Dip(editorPanel_, 2);

    if (y < yOff + head ||
        y >= yOff + head + lineOnScreen() * lineH) {
        return;
    }

    const std::size_t row = currentOffset_ + static_cast<std::size_t>((y - yOff - head) / lineH);
    if (row >= bank_.effect(currentEffect_).frames.size()) {
        return;
    }

    const auto& cell = bank_.effect(currentEffect_).frames[row];
    const int rowY = yOff + head + static_cast<int>(row - currentOffset_) * lineH;

    const wxRect posRect = MakeEditorTextRect(editorPanel_,
                                              xOff,
                                              rowY,
                                              wxString::Format("%03X", static_cast<unsigned>(row)),
                                              lineH);
    const wxRect toneFlagRect(xOff + tFlagOff, rowY, Dip(editorPanel_, EditorLayout::kNFlagOff - EditorLayout::kTFlagOff), lineH);
    const wxRect noiseFlagRect(xOff + nFlagOff, rowY, Dip(editorPanel_, EditorLayout::kTColOff  - EditorLayout::kNFlagOff), lineH);

    auto zone = [&](int left, int right) {
        if (x >= left && x <= right) {
            xmin = left;
            xmax = right;
            hold = true;
        }
    };

    zone(posRect.GetLeft(), posRect.GetRight());
    zone(toneFlagRect.GetLeft(), toneFlagRect.GetRight());
    zone(noiseFlagRect.GetLeft(), noiseFlagRect.GetRight());
    zone(xOff + toneOff - barPad, xOff + toneOff + toneW);
    zone(xOff + noiseOff - barPad, xOff + noiseOff + noiseW);
    zone(xOff + volOff - barPad, xOff + volOff + volW);

    if (force) {
        xmin = lastMouseX_;
        xmax = lastMouseX_;
        hold = true;
    }

    if (!hold) {
        return;
    }

    wxUnusedVar(xmin);
    wxUnusedVar(xmax);
    editorPanel_->SetCursor(wxCursor(wxCURSOR_PENCIL));
}

void MainFrame::releaseHoldZone() {
    if (editorPanel_) {
        editorPanel_->SetCursor(wxNullCursor);
    }
}

std::vector<AudioEngine::FrameData> MainFrame::buildFrameData(std::size_t effectIndex) const {
    const auto& fx = bank_.effect(effectIndex);
    const auto realLen = bank_.effectRealLength(effectIndex);

    std::vector<AudioEngine::FrameData> frames;
    frames.reserve(realLen);
    for (std::size_t i = 0; i < realLen; ++i) {
        const auto& cell = fx.frames[i];
        frames.push_back({cell.tone, cell.noise, cell.volume, cell.toneEnable, cell.noiseEnable});
    }
    return frames;
}

void MainFrame::playEffectFrom(std::size_t startFrame) {
    auto frames = buildFrameData(currentEffect_);
    if (startFrame >= frames.size()) {
        return;
    }
    frames.erase(frames.begin(), frames.begin() + static_cast<std::ptrdiff_t>(startFrame));
    audioEngine_.play(frames);
}

bool MainFrame::exportEffectWave(std::size_t effectIndex, const std::filesystem::path& path) {
    const auto frames = buildFrameData(effectIndex);
    if (frames.empty()) {
        return false;
    }
    const auto pcm = audioEngine_.renderToPcm(frames);
    return WriteWaveFile(path.string(), pcm, audioEngine_.config().sampleRate);
}

bool MainFrame::exportEffectCsv(std::size_t effectIndex, const std::filesystem::path& path) {
    const auto realLen = bank_.effectRealLength(effectIndex);
    if (realLen == 0) {
        return false;
    }

    std::ofstream out(path);
    if (!out) {
        return false;
    }

    const auto& fx = bank_.effect(effectIndex);
    for (std::size_t i = 0; i < realLen; ++i) {
        const auto& cell = fx.frames[i];
        out << (cell.toneEnable ? 1 : 0) << ',' << (cell.noiseEnable ? 1 : 0) << ','
            << wxString::Format("0x%3.3x,0x%2.2x,0x%1.1x", cell.tone, cell.noise, cell.volume).ToStdString()
            << '\n';
    }
    return static_cast<bool>(out);
}

bool MainFrame::exportEffectVt2(std::size_t effectIndex, const std::filesystem::path& path, int baseNote) {
    auto realLen = bank_.effectRealLength(effectIndex);
    if (realLen == 0) {
        return false;
    }
    if (realLen > 0x3f) {
        realLen = 0x3f;
    }

    static const auto kOffset = BuildVt2OffsetTable();
    const int base = std::clamp(baseNote, 0, static_cast<int>(kOffset.size()) - 1);

    std::ofstream out(path);
    if (!out) {
        return false;
    }

    out << "[Sample]\n";

    const auto& fx = bank_.effect(effectIndex);
    for (std::size_t i = 0; i < realLen; ++i) {
        const auto& cell = fx.frames[i];
        int t = static_cast<int>(cell.tone) - kOffset[base];
        t = std::clamp(t, -1023, 1023);
        const int absT = t < 0 ? -t : t;

        out << (cell.toneEnable ? 'T' : 't') << (cell.noiseEnable ? 'N' : 'n') << "e "
            << (t < 0 ? '-' : '+')
            << wxString::Format("%03X", absT).ToStdString() << "_ +"
            << wxString::Format("%02X", cell.noise).ToStdString() << "_ "
            << wxString::Format("%01X", cell.volume).ToStdString() << "_\n";
    }

    out << "tne +000_ +00_ 0_ L\n";
    return static_cast<bool>(out);
}

bool MainFrame::importEffectPsg(std::size_t effectIndex, const std::filesystem::path& path, int channel) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return false;
    }
    const std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (data.size() < 3 || data[0] != 'P' || data[1] != 'S' || data[2] != 'G') {
        return false;
    }

    auto& fx = bank_.effect(effectIndex);
    auto& outFrames = fx.frames;
    std::fill(outFrames.begin(), outFrames.end(), AyfxCell{});
    const std::size_t maxFrames = outFrames.size();

    const std::size_t size = data.size();
    std::size_t ptr = 16;  // 16-byte header, unparsed beyond the "PSG" signature
    std::size_t pd = 0;

    int tone = 0, noise = 0, volume = 0, nvolume = 0;
    int ltone[3] = {0, 0, 0};
    int lnoise = 0;
    bool lt[3] = {false, false, false};
    bool ln[3] = {false, false, false};
    bool t = false, n = false;
    bool first = true;
    int chan = channel;
    int newchan = channel;
    int icnt = 0;

    auto setToneLo = [&](int ch) {
        ltone[ch] = (ltone[ch] & 0x0F00) | data[ptr + 1];
        if (chan < 0 && ltone[ch] > 0) {
            noise = lnoise; tone = ltone[ch]; t = lt[ch]; n = ln[ch];
        }
        if (chan == ch) tone = ltone[ch];
    };
    auto setToneHi = [&](int ch) {
        ltone[ch] = (ltone[ch] & 0x00FF) | (static_cast<int>(data[ptr + 1]) << 8);
        if (chan < 0 && ltone[ch] > 0) {
            noise = lnoise; tone = ltone[ch]; t = lt[ch]; n = ln[ch];
        }
        if (chan == ch) tone = ltone[ch];
    };
    auto setVolume = [&](int ch) {
        nvolume = data[ptr + 1] & 0x0F;
        if (chan < 0 && nvolume > 0) { newchan = ch; volume = nvolume; }
        if (chan == ch) volume = nvolume;
    };

    while (ptr < size) {
        const std::uint8_t reg = data[ptr];
        switch (reg) {
        case 0:  if (ptr + 1 >= size) { ptr = size; break; } setToneLo(0); ptr += 2; break;
        case 1:  if (ptr + 1 >= size) { ptr = size; break; } setToneHi(0); ptr += 2; break;
        case 2:  if (ptr + 1 >= size) { ptr = size; break; } setToneLo(1); ptr += 2; break;
        case 3:  if (ptr + 1 >= size) { ptr = size; break; } setToneHi(1); ptr += 2; break;
        case 4:  if (ptr + 1 >= size) { ptr = size; break; } setToneLo(2); ptr += 2; break;
        case 5:  if (ptr + 1 >= size) { ptr = size; break; } setToneHi(2); ptr += 2; break;
        case 6:
            if (ptr + 1 >= size) { ptr = size; break; }
            lnoise = data[ptr + 1] & 0x1F;
            if (chan >= 0) noise = lnoise;
            ptr += 2;
            break;
        case 7:
            if (ptr + 1 >= size) { ptr = size; break; }
            {
                const std::uint8_t mixer = data[ptr + 1];
                lt[0] = !(mixer & 0x01); ln[0] = !(mixer & 0x08);
                lt[1] = !(mixer & 0x02); ln[1] = !(mixer & 0x10);
                lt[2] = !(mixer & 0x04); ln[2] = !(mixer & 0x20);
                if (chan >= 0) { t = lt[chan]; n = ln[chan]; }
            }
            ptr += 2;
            break;
        case 8:  if (ptr + 1 >= size) { ptr = size; break; } setVolume(0); ptr += 2; break;
        case 9:  if (ptr + 1 >= size) { ptr = size; break; } setVolume(1); ptr += 2; break;
        case 10: if (ptr + 1 >= size) { ptr = size; break; } setVolume(2); ptr += 2; break;
        case 255:
            if (pd == maxFrames) ptr = size;
            icnt = 1;
            ptr += 1;
            break;
        case 254:
            if (ptr + 1 >= size) { ptr = size; break; }
            icnt = data[ptr + 1] * 4;
            ptr += 2;
            break;
        case 253:
            ptr = size;
            break;
        default:
            ptr += 2;
            break;
        }

        if (newchan >= 0) {
            chan = newchan;
            noise = lnoise;
            tone = ltone[chan];
            t = lt[chan];
            n = ln[chan];
        }

        for (int aa = 0; aa < icnt; ++aa) {
            if (pd == maxFrames) {
                ptr = size;
                break;
            }
            if (first) {
                if (tone == 0 && noise == 0 && volume == 0) {
                    continue;
                }
            }
            first = false;
            AyfxCell cell;
            cell.tone = static_cast<std::uint16_t>(tone);
            cell.noise = static_cast<std::uint8_t>(noise);
            cell.volume = static_cast<std::uint8_t>(volume);
            cell.toneEnable = t;
            cell.noiseEnable = n;
            outFrames[pd++] = cell;
        }
        icnt = 0;
    }

    fx.name = path.stem().string();
    return true;
}

bool MainFrame::importEffectVtx(std::size_t effectIndex, const std::filesystem::path& path, int channel) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return false;
    }
    const std::vector<std::uint8_t> raw((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (raw.size() < 16) {
        return false;
    }
    const bool isAy = raw[0] == 'a' && raw[1] == 'y';
    const bool isYm = raw[0] == 'y' && raw[1] == 'm';
    if (!isAy && !isYm) {
        return false;
    }

    const std::size_t usize = static_cast<std::size_t>(raw[12]) |
                               (static_cast<std::size_t>(raw[13]) << 8) |
                               (static_cast<std::size_t>(raw[14]) << 16) |
                               (static_cast<std::size_t>(raw[15]) << 24);
    if (usize == 0 || usize > 64u * 1024u * 1024u) {
        return false;  // 0 or implausibly large -> treat as corrupt/malformed
    }

    // Header holds 5 NUL-terminated strings (title/author/etc, unused here)
    // before the LH5-compressed register table.
    std::size_t ptr = 16;
    for (int i = 0; i < 5; ++i) {
        while (ptr < raw.size() && raw[ptr] != 0) {
            ++ptr;
        }
        ++ptr;
    }
    if (ptr >= raw.size()) {
        return false;
    }

    const auto decompressed = DecodeLh5(raw.data() + ptr, raw.size() - ptr, usize);
    if (decompressed.size() != usize) {
        return false;
    }

    // Register-major layout: 14 planes of `block` bytes each (registers
    // 0-13), one byte per AY-frame per register. Only registers 0-10 are
    // used (11-13 are envelope, which ayfx effects don't model).
    const int regord[11] = {5, 6, 7, 8, 9, 10, 0, 1, 2, 3, 4};
    const std::size_t block = usize / 14;
    if (block == 0) {
        return false;
    }

    auto& fx = bank_.effect(effectIndex);
    auto& outFrames = fx.frames;
    std::fill(outFrames.begin(), outFrames.end(), AyfxCell{});
    const std::size_t maxFrames = outFrames.size();

    int tone = 0, noise = 0, volume = 0;
    int ltone[3] = {0, 0, 0};
    int lnoise = 0;
    bool lt[3] = {false, false, false};
    bool ln[3] = {false, false, false};
    bool t = false, n = false;
    int chan = channel;
    int newchan = channel;
    std::size_t pd = 0;

    for (std::size_t pp = 0; pp < block; ++pp) {
        for (int aa = 0; aa < 11; ++aa) {
            const int reg = regord[aa];
            const std::uint8_t v = decompressed[pp + static_cast<std::size_t>(reg) * block];
            switch (reg) {
            case 0: ltone[0] = (ltone[0] & 0x0F00) | v; if (chan == 0) tone = ltone[0]; break;
            case 1: ltone[0] = (ltone[0] & 0x00FF) | (static_cast<int>(v) << 8); if (chan == 0) tone = ltone[0]; break;
            case 2: ltone[1] = (ltone[1] & 0x0F00) | v; if (chan == 1) tone = ltone[1]; break;
            case 3: ltone[1] = (ltone[1] & 0x00FF) | (static_cast<int>(v) << 8); if (chan == 1) tone = ltone[1]; break;
            case 4: ltone[2] = (ltone[2] & 0x0F00) | v; if (chan == 2) tone = ltone[2]; break;
            case 5: ltone[2] = (ltone[2] & 0x00FF) | (static_cast<int>(v) << 8); if (chan == 2) tone = ltone[2]; break;
            case 6:
                lnoise = v & 0x1F;
                if (chan >= 0) noise = lnoise;
                break;
            case 7:
                lt[0] = !(v & 0x01); ln[0] = !(v & 0x08);
                lt[1] = !(v & 0x02); ln[1] = !(v & 0x10);
                lt[2] = !(v & 0x04); ln[2] = !(v & 0x20);
                if (chan >= 0) { t = lt[chan]; n = ln[chan]; }
                break;
            case 8: {
                const int nvolume = v & 0x0F;
                if (chan < 0 && nvolume > 0) { newchan = 0; volume = nvolume; }
                if (chan == 0) volume = nvolume;
                break;
            }
            case 9: {
                const int nvolume = v & 0x0F;
                if (chan < 0 && nvolume > 0) { newchan = 1; volume = nvolume; }
                if (chan == 1) volume = nvolume;
                break;
            }
            case 10: {
                const int nvolume = v & 0x0F;
                if (chan < 0 && nvolume > 0) { newchan = 2; volume = nvolume; }
                if (chan == 2) volume = nvolume;
                break;
            }
            default:
                break;
            }

            if (newchan >= 0) {
                chan = newchan;
                noise = lnoise;
                tone = ltone[chan];
                t = lt[chan];
                n = ln[chan];
            }
        }

        if (pd >= maxFrames) {
            break;
        }
        if (chan >= 0) {
            AyfxCell cell;
            cell.tone = static_cast<std::uint16_t>(tone);
            cell.noise = static_cast<std::uint8_t>(noise);
            cell.volume = static_cast<std::uint8_t>(volume);
            cell.toneEnable = t;
            cell.noiseEnable = n;
            outFrames[pd++] = cell;
        }
    }

    fx.name = path.stem().string();
    return true;
}

bool MainFrame::importEffectVgm(std::size_t effectIndex, const std::filesystem::path& path, int channel, bool mixNoise) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return false;
    }
    const std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (data.size() < 0x44 || std::memcmp(data.data(), "Vgm ", 4) != 0) {
        return false;
    }

    auto readInt32 = [&](std::size_t off) -> std::int32_t {
        return static_cast<std::int32_t>(
            static_cast<std::uint32_t>(data[off]) |
            (static_cast<std::uint32_t>(data[off + 1]) << 8) |
            (static_cast<std::uint32_t>(data[off + 2]) << 16) |
            (static_cast<std::uint32_t>(data[off + 3]) << 24));
    };

    const std::int32_t baseFreq = readInt32(0x0c);
    if (baseFreq == 0) {
        return false;  // no SN76489 (PSG) clock -> nothing to import
    }

    // SN76489 emulation state, ported from the original's psg_out_port().
    std::uint8_t chanVol[4] = {0, 0, 0, 0};
    std::int16_t chanDiv[4] = {0, 0, 0, 0};
    int latchedChan = 0;
    int latchedType = 0;  // 0 = tone/noise-control, nonzero = volume

    auto psgOutPort = [&](std::uint8_t val) {
        int chan;
        int div;
        if (val & 0x80) {
            chan = (val >> 5) & 3;
            div = (chanDiv[chan] & 0xFFF0) | (val & 0x0F);
            latchedChan = chan;
            latchedType = val & 0x10;
        } else {
            chan = latchedChan;
            div = (chanDiv[chan] & 0x0F) | ((val & 0x3F) << 4);
        }
        if (latchedType) {
            chanVol[chan] = val & 0x0F;
        } else {
            chanDiv[chan] = static_cast<std::int16_t>(div);
        }
    };

    auto& fx = bank_.effect(effectIndex);
    auto& outFrames = fx.frames;
    std::fill(outFrames.begin(), outFrames.end(), AyfxCell{});
    const std::size_t maxFrames = outFrames.size();

    std::size_t ptr = 0x40;
    long wait = 0;
    bool done = false;
    std::size_t pd = 0;
    int pchan = channel;
    int noiseDiv = 0;

    while (!done && ptr < data.size()) {
        const std::uint8_t op = data[ptr];
        int incr = 0;
        switch (op) {
        case 0x50:
            if (ptr + 1 < data.size()) psgOutPort(data[ptr + 1]);
            incr = 2;
            break;
        case 0x4f:
            incr = 2;
            break;
        case 0x61:
            if (ptr + 2 < data.size()) {
                wait += data[ptr + 1] + (static_cast<int>(data[ptr + 2]) << 8);
            }
            incr = 3;
            break;
        case 0x51:
        case 0x52:
        case 0x53:
        case 0x54:
            incr = 3;
            break;
        case 0x62:
            wait += 735;
            incr = 1;
            break;
        case 0x63:
            wait += 882;
            incr = 1;
            break;
        case 0x66:
            done = true;
            break;
        default:
            // Unrecognised command (e.g. a chip this importer doesn't know,
            // like native AY8910 writes). The original has no length for
            // this case either (incr stays 0) and would spin forever
            // re-reading the same byte -- treat it as end of stream instead
            // of hanging the app.
            done = true;
            break;
        }
        ptr += static_cast<std::size_t>(incr);

        if (wait >= 735) {
            if (pchan < 0) {
                if (chanVol[0] < 15) {
                    pchan = 0;
                } else if (chanVol[1] < 15) {
                    pchan = 1;
                } else if (chanVol[2] < 15) {
                    pchan = 2;
                }
            }

            wait -= 735;

            if (pchan >= 0) {
                const int tvol = 15 - chanVol[pchan];
                int nvol = mixNoise ? (15 - chanVol[3]) : 0;

                if (mixNoise) {
                    switch (chanDiv[3] & 3) {
                    case 0: noiseDiv = 0x1f; break;
                    case 1: noiseDiv = 0x19; break;
                    case 2: noiseDiv = 0x10; break;
                    case 3:
                        noiseDiv = chanDiv[2] >> 1;
                        if (noiseDiv > 63) noiseDiv = 63;
                        break;
                    default: break;
                    }
                }

                int ovol = tvol;
                if (nvol > tvol) ovol = nvol;

                const int frq = (chanDiv[pchan] > 0) ? (baseFreq / (chanDiv[pchan] * 16)) : 100;
                int toneDiv = kAYClockRateSpectrum / 8 / frq;
                toneDiv = std::clamp(toneDiv, 0, 4095);

                if (pd < maxFrames) {
                    AyfxCell cell;
                    cell.tone = static_cast<std::uint16_t>(toneDiv);
                    cell.noise = static_cast<std::uint8_t>(noiseDiv);
                    cell.volume = static_cast<std::uint8_t>(ovol);
                    cell.toneEnable = tvol != 0;
                    cell.noiseEnable = nvol != 0;
                    outFrames[pd++] = cell;
                }
                if (pd >= maxFrames) {
                    done = true;
                }
            }
        }
    }

    fx.name = path.stem().string();
    return true;
}

void MainFrame::togglePianoInput() {
    if (!pianoButton_) {
        return;
    }
    const bool newState = !pianoButton_->GetValue();
    pianoButton_->SetValue(newState);
    wxCommandEvent toggleEvt(wxEVT_TOGGLEBUTTON, kIdViewPiano);
    toggleEvt.SetInt(newState ? 1 : 0);
    ProcessWindowEvent(toggleEvt);
}

void MainFrame::setPianoWindowVisible(bool visible) {
    if (!pianoWindow_) {
        pianoWindow_ = new PianoWindow(this);
    }
    pianoWindow_->Show(visible);
    syncPianoToggleState(visible);
}

void MainFrame::syncPianoToggleState(bool visible) {
    if (pianoButton_) {
        pianoButton_->SetValue(visible);
    }
    if (GetMenuBar()) {
        GetMenuBar()->Check(kIdViewPiano, visible);
    }
}

void MainFrame::enterFromPiano(int tonePeriod, bool setToneEnableFlag, int volume, int step, int fill) {
    auto& fx = bank_.effect(currentEffect_);

    for (int i = 0; i < fill; ++i) {
        const auto row = currentY_ + static_cast<std::size_t>(i);
        if (row >= BankModel::kMaxFrames) {
            break;
        }
        auto& cell = fx.frames[row];
        cell.tone = static_cast<std::uint16_t>(tonePeriod & 0x0FFF);
        if (setToneEnableFlag) {
            cell.toneEnable = true;
        }
        if (volume >= 0) {
            cell.volume = static_cast<std::uint8_t>(volume);
        }
    }

    currentY_ = std::min(currentY_ + static_cast<std::size_t>(step), BankModel::kMaxFrames - 1);
    ensureCursorVisible();
    refreshView();
}

void MainFrame::drawEditor(wxDC& dc) {
    if (!editorPanel_) {
        return;
    }

    const EditorPalette& pal = EditorPalette::current();
    const wxColour& colBackground = pal.background;
    const wxColour& colTextActive = pal.text;

    dc.SetBackground(wxBrush(colBackground));
    dc.Clear();

    const int xOff = Dip(editorPanel_, EditorLayout::kXOff);
    const int yOff = Dip(editorPanel_, EditorLayout::kYOff);
    const int head = Dip(editorPanel_, EditorLayout::kHead);
    const int lineH = EditorLineHeight(editorPanel_);
    const int panelW = editorPanel_->GetClientSize().GetWidth();

    const auto bl = ComputeBarLayout(editorPanel_);
    const int toneOff  = bl.toneOff;
    const int toneW    = bl.toneW;
    const int noiseOff = bl.noiseOff;
    const int noiseW   = bl.noiseW;
    const int volOff   = bl.volOff;
    const int volW     = bl.volW;

    const int tColOff = Dip(editorPanel_, EditorLayout::kTColOff);
    const int nColOff = Dip(editorPanel_, EditorLayout::kNColOff);
    const int vColOff = Dip(editorPanel_, EditorLayout::kVColOff);
    const int tFlagOff = Dip(editorPanel_, EditorLayout::kTFlagOff);
    const int nFlagOff = Dip(editorPanel_, EditorLayout::kNFlagOff);
    // T/N glyphs are drawn off-center within their (deliberately wide, full-
    // column) hit-test boxes, pulled in toward the shared T/N boundary. That
    // boundary is the midpoint of the Pos-to-Per span, so centering each
    // glyph in its own half of the box would put the T-N gap at double the
    // Pos-T/N-Per gaps (two glyph paddings meeting vs. one) — this evens all
    // three out to ~13 DIP instead, without shrinking the click targets.
    const int tFlagTextOff = nFlagOff - Dip(editorPanel_, 13);
    const int nFlagTextOff = nFlagOff + Dip(editorPanel_, 6);

    wxFont bodyFont = MakeEditorFont(EditorPointSize());
    wxFont headerFont = MakeEditorFont(EditorHeaderPointSize());

    // Header row
    dc.SetFont(headerFont);
    dc.SetTextForeground(colTextActive);
    dc.DrawText("Pos", xOff, yOff);
    dc.DrawText("T", xOff + tFlagTextOff, yOff);
    dc.DrawText("N", xOff + nFlagTextOff, yOff);
    dc.DrawText("Per", xOff + tColOff, yOff);
    dc.DrawText("Ns", xOff + nColOff, yOff);
    dc.DrawText("V", xOff + vColOff, yOff);
    dc.DrawText(periodLinear_ ? "Period (linear)" : "Period (log)", xOff + toneOff, yOff);
    dc.DrawText("Noise", xOff + noiseOff, yOff);
    dc.DrawText("Volume", xOff + volOff, yOff);

    dc.SetFont(bodyFont);

    const auto& fx = bank_.effect(currentEffect_);
    const auto realLen = bank_.effectRealLength(currentEffect_);
    const int lines = lineOnScreen();
    const int barPad = Dip(editorPanel_, 2);

    int y = yOff + head;
    for (int row = 0; row < lines; ++row) {
        const std::size_t pp = currentOffset_ + static_cast<std::size_t>(row);
        if (pp >= fx.frames.size()) {
            break;
        }

        const auto& cell = fx.frames[pp];
        const bool active = pp < realLen;

        const wxColour back = cell.selected ? wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT) : colBackground;
        const wxColour textCol = cell.selected ? wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHTTEXT) : colTextActive;

        // Row background
        dc.SetBrush(wxBrush(back));
        dc.SetPen(wxPen(back));
        dc.DrawRectangle(0, y, panelW, lineH);

        dc.SetTextForeground(textCol);
        dc.DrawText(wxString::Format("%03X", static_cast<unsigned>(pp)), xOff, y);
        dc.DrawText(cell.toneEnable ? "T" : "-", xOff + tFlagTextOff, y);
        dc.DrawText(cell.noiseEnable ? "N" : "-", xOff + nFlagTextOff, y);

        auto drawCellText = [&](int col, int x, const wxString& text) {
            if (pp == currentY_ && currentX_ == col) {
                wxCoord tw = 0;
                wxCoord th = 0;
                dc.GetTextExtent(text, &tw, &th);
                dc.SetBrush(wxBrush(pal.cursorBg));
                dc.SetPen(wxPen(pal.cursorBg));
                dc.DrawRectangle(x, y, tw, lineH);
                dc.SetTextForeground(pal.cursorText);
                dc.DrawText(text, x, y);
                dc.SetTextForeground(textCol);
            } else {
                dc.DrawText(text, x, y);
            }
        };

        drawCellText(0, xOff + tColOff, wxString::Format("%03X", static_cast<unsigned>(cell.tone & 0x0FFFu)));
        drawCellText(1, xOff + nColOff, wxString::Format("%02X", static_cast<unsigned>(cell.noise & 0x1Fu)));
        drawCellText(2, xOff + vColOff, wxString::Format("%X", static_cast<unsigned>(cell.volume & 0x0Fu)));

        // Bars: rounded border (macOS-style), fill in the system accent
        // colour; barPad pixels of vertical margin
        const int bh = lineH - barPad * 2;
        const double barRadius = Dip(editorPanel_, 3);
        const double fillRadius = std::max(0.0, barRadius - 1);
        // On a selected row the background is already the accent colour, so
        // the fill switches to the "on-highlight" colour instead (matching
        // how selected list/icon content inverts against its own selection
        // background elsewhere) — otherwise it's the same blue on blue and
        // disappears.
        const wxColour colBarAccent = wxSystemSettings::GetColour(
            cell.selected ? wxSYS_COLOUR_HIGHLIGHTTEXT : wxSYS_COLOUR_HIGHLIGHT);

        dc.SetBrush(wxBrush(back));
        dc.SetPen(wxPen(pal.barBorder));
        dc.DrawRoundedRectangle(xOff + toneOff,  y + barPad, toneW,  bh, barRadius);
        dc.DrawRoundedRectangle(xOff + noiseOff, y + barPad, noiseW, bh, barRadius);
        dc.DrawRoundedRectangle(xOff + volOff,   y + barPad, volW,   bh, barRadius);

        // Fill inside the border when the cell has any non-zero data.
        // Ghost rows (all zeros) are left empty; no separate "dim" colour.
        const bool hasData = cell.tone > 0 || cell.noise > 0 || cell.volume > 0;
        if (hasData) {
            dc.SetPen(*wxTRANSPARENT_PEN);
            dc.SetBrush(wxBrush(colBarAccent));

            const int inner = bh - 2;
            const int innerToneW  = toneW  - 2;
            const int innerNoiseW = noiseW - 2;
            const int innerVolW   = volW   - 2;

            int toneBar = periodToWidth(cell.tone, innerToneW, periodLinear_);
            if (toneBar < Dip(editorPanel_, 2) && cell.tone > 0) { toneBar = Dip(editorPanel_, 2); }
            toneBar = std::clamp(toneBar, 0, innerToneW);
            if (toneBar > 0) {
                dc.DrawRoundedRectangle(xOff + toneOff + 1, y + barPad + 1, toneBar, inner, fillRadius);
            }

            int noiseBar = (cell.noise * innerNoiseW) / 31;
            if (noiseBar < Dip(editorPanel_, 2) && cell.noise > 0) { noiseBar = Dip(editorPanel_, 2); }
            noiseBar = std::clamp(noiseBar, 0, innerNoiseW);
            if (noiseBar > 0) {
                dc.DrawRoundedRectangle(xOff + noiseOff + 1, y + barPad + 1, noiseBar, inner, fillRadius);
            }

            int volBar = (cell.volume * innerVolW) / 15;
            if (volBar < Dip(editorPanel_, 2) && cell.volume > 0) { volBar = Dip(editorPanel_, 2); }
            volBar = std::clamp(volBar, 0, innerVolW);
            if (volBar > 0) {
                dc.DrawRoundedRectangle(xOff + volOff + 1, y + barPad + 1, volBar, inner, fillRadius);
            }
        }

        y += lineH;
    }
}
