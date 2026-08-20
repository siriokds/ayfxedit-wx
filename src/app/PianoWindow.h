#pragma once

#include <wx/frame.h>

class MainFrame;

// Floating tool window for entering tone-period values by note instead of
// hex digits. Ported from the original editor's TFormPiano (UnitPiano.cpp/
// .dfm): clicking a key, or pressing its mapped computer key (Z S X D C V G
// B H N J M — the bottom two QWERTY rows), writes the note's period into the
// bank at the main window's cursor, using the same note/octave -> AY period
// lookup tables as the original.
class PianoWindow final : public wxFrame {
public:
    explicit PianoWindow(MainFrame* mainFrame);

private:
    void createContent();
    void onCharHook(class wxKeyEvent& event);
    void onKeyUp(class wxKeyEvent& event);
    void onClose(class wxCloseEvent& event);
    void onKeysPaint(class wxPaintEvent& event);
    void onKeysMouseDown(class wxMouseEvent& event);
    void onKeysMouseUp(class wxMouseEvent& event);

    void enterNote(int note);
    void updateFillEnabled();
    void updateTableLabel();
    int keyAt(int x, int y) const;

    MainFrame* mainFrame_;

    int octave_ = 4;      // 1-8, matches the original's default
    int step_ = 1;        // frames to advance the cursor after entry
    int fill_ = 1;        // consecutive frames to write per entry
    int volume_ = 15;
    int tableIndex_ = 0;  // index into kNoteFreqTables / kNoteFreqTableNames
    int highlightedKey_ = -1;

    class wxSpinCtrl* octaveSpin_ = nullptr;
    class wxSpinCtrl* stepSpin_ = nullptr;
    class wxSpinCtrl* fillSpin_ = nullptr;
    class wxSpinCtrl* volumeSpin_ = nullptr;
    class wxChoice* tableChoice_ = nullptr;
    class wxCheckBox* setToneCheck_ = nullptr;
    class wxCheckBox* linkFillCheck_ = nullptr;
    class wxCheckBox* setVolumeCheck_ = nullptr;
    class wxPanel* keysPanel_ = nullptr;
};
