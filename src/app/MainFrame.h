#pragma once

#include <wx/frame.h>

#include <filesystem>

#include "../core/BankModel.h"
#include "../audio/AudioEngine.h"

class MainFrame final : public wxFrame {
public:
    MainFrame();

private:
    void createMenu();
    void createContent();
    void refreshView();
    void updateTitle();
    void drawEditor(class wxDC& dc);

    void onNewBank(wxCommandEvent& event);
    void onLoadBank(wxCommandEvent& event);
    void onSaveBank(wxCommandEvent& event);
    void onSaveBankNoNames(wxCommandEvent& event);
    void onLoadEffect(wxCommandEvent& event);
    void onSaveEffect(wxCommandEvent& event);
    void onPrevEffect(wxCommandEvent& event);
    void onNextEffect(wxCommandEvent& event);
    void onAddEffect(wxCommandEvent& event);
    void onInsertEffect(wxCommandEvent& event);
    void onDeleteEffect(wxCommandEvent& event);
    void onApplyName(wxCommandEvent& event);
    void onEditorMouseDown(class wxMouseEvent& event);
    void onEditorMouseMove(class wxMouseEvent& event);
    void onEditorMouseUp(class wxMouseEvent& event);
    void onEditorMouseWheel(class wxMouseEvent& event);
    void onKeyDown(class wxKeyEvent& event);
    void onSysColourChanged(class wxSysColourChangedEvent& event);

    void onEditCopy(wxCommandEvent& event);
    void onEditCut(wxCommandEvent& event);
    void onEditPaste(wxCommandEvent& event);
    void onEditDelete(wxCommandEvent& event);
    void onEditSelectAll(wxCommandEvent& event);
    void onEditUnselectAll(wxCommandEvent& event);
    void onEditInverseSelection(wxCommandEvent& event);

    static std::string sanitizeFileName(const std::string& value);
    void setCurrentEffect(std::size_t index);
    int lineOnScreen() const;
    void clampView();
    void ensureCursorVisible();
    void centerView();
    void clearSelection();
    void insertFrame(bool cloneCurrent);
    bool deleteSelected();
    void copySelection(bool clearAfterCopy);
    void pasteClipboard();
    void adjustColumn(int delta);
    void handleHexInput(int nibble);
    bool mouseEditAt(int x, int y, bool left, bool right, bool hold);
    void updateHoldZone(int x, int y, bool force);
    void releaseHoldZone();

    BankModel bank_;
    AudioEngine audioEngine_;
    std::size_t currentEffect_ = 0;
    std::size_t currentOffset_ = 0;
    std::size_t currentY_ = 0;
    int currentX_ = 0;
    bool mouseHold_ = false;
    int lastMouseX_ = -1;
    int lastMouseY_ = -1;
    int curPrevLine_ = -1;
    // Flag drag-paint: 0=tone, 1=noise, -1=inactive; flagDragValue_=target state
    int flagDragColumn_ = -1;
    bool flagDragValue_ = false;
    std::vector<AyfxCell> clipboard_;
    std::filesystem::path bankPath_ = "noname.afb";

    class wxTextCtrl* effectNameText_ = nullptr;
    class wxTextCtrl* effectNumberText_ = nullptr;
    class wxButton* prevButton_ = nullptr;
    class wxButton* nextButton_ = nullptr;
    class wxButton* firstButton_ = nullptr;
    class wxButton* lastButton_ = nullptr;
    class wxButton* addButton_ = nullptr;
    class wxButton* delButton_ = nullptr;
    class wxToggleButton* pianoButton_ = nullptr;
    class wxBitmapButton* playButton_ = nullptr;
    class wxBitmapButton* stopButton_ = nullptr;
    class wxPanel* editorPanel_ = nullptr;
    class wxMenuItem* exportCurrentItem_ = nullptr;
    class wxMenuItem* exportAllItem_ = nullptr;
    class wxMenuItem* viewLinearItem_ = nullptr;
    class wxMenuItem* viewLogItem_ = nullptr;
    bool periodLinear_ = true;
};
