#pragma once

#include <wx/frame.h>
#include <wx/font.h>

#include <filesystem>

#include "../core/BankModel.h"
#include "../core/Settings.h"
#include "../audio/AudioEngine.h"

class MainFrame final : public wxFrame {
public:
    MainFrame();

    void enterFromPiano(int tonePeriod, bool setToneEnableFlag, int volume, int step, int fill);
    void syncPianoToggleState(bool visible);

private:
    void createMenu();
    void createContent();
    void refreshView();
    void updateAudioStatus();
    void updateTitle();
    void drawEditor(class wxDC& dc);

    void onNewBank(wxCommandEvent& event);
    void onLoadBank(wxCommandEvent& event);
    void onSaveBank(wxCommandEvent& event);
    void onSaveBankNoNames(wxCommandEvent& event);
    void onLoadEffect(wxCommandEvent& event);
    void onSaveEffect(wxCommandEvent& event);
    void onExportWave(wxCommandEvent& event);
    void onExportCsv(wxCommandEvent& event);
    void onExportVt2(wxCommandEvent& event);
    void onImportPsg(wxCommandEvent& event);
    void onImportVtx(wxCommandEvent& event);
    void onImportVgm(wxCommandEvent& event);
    void onImportWav(wxCommandEvent& event);
    void onImportMp3(wxCommandEvent& event);
    void onReclockEffect(wxCommandEvent& event);
    void onMultiLoadBank(wxCommandEvent& event);
    void onMultiSaveBank(wxCommandEvent& event);
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
    void onAbout(wxCommandEvent& event);
    void onClose(class wxCloseEvent& event);

    // Applies theme/font settings immediately -- called both at startup and
    // as a live preview while the Preferences dialog's Appearance page is
    // being edited (see SettingsDialog's onLiveAppearanceChange callback).
    void applyAppearanceSettings(const Settings& s);

    void onEditCopy(wxCommandEvent& event);
    void onEditCut(wxCommandEvent& event);
    void onEditPaste(wxCommandEvent& event);
    void onEditDelete(wxCommandEvent& event);
    void onEditSelectAll(wxCommandEvent& event);
    void onEditUnselectAll(wxCommandEvent& event);
    void onEditInverseSelection(wxCommandEvent& event);

    static std::string sanitizeFileName(const std::string& value);
    static bool isPlaceholderEffectName(const std::string& name);
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
    std::vector<AudioEngine::FrameData> buildFrameData(std::size_t effectIndex) const;
    void playEffectFrom(std::size_t startFrame);
    bool exportEffectWave(std::size_t effectIndex, const std::filesystem::path& path);
    bool exportEffectCsv(std::size_t effectIndex, const std::filesystem::path& path);
    bool exportEffectVt2(std::size_t effectIndex, const std::filesystem::path& path, int baseNote);
    bool importEffectPsg(std::size_t effectIndex, const std::filesystem::path& path, int channel);
    bool importEffectVtx(std::size_t effectIndex, const std::filesystem::path& path, int channel);
    bool importEffectVgm(std::size_t effectIndex, const std::filesystem::path& path, int channel, bool mixNoise);
    bool importEffectWav(std::size_t effectIndex, const std::filesystem::path& path);
    bool importEffectMp3(std::size_t effectIndex, const std::filesystem::path& path);
    bool importEffectFromPcm(std::size_t effectIndex, const std::filesystem::path& path,
                              const std::int16_t* pcm, unsigned channels, unsigned sampleRate,
                              std::uint64_t totalFrames);
    bool showAyChannelSelectDialog(int& channel);
    bool showSnChannelSelectDialog(int& channel, bool& mixNoise);
    void togglePianoInput();
    void setPianoWindowVisible(bool visible);

    BankModel bank_;
    AudioEngine audioEngine_;
    Settings settings_;
    // Captured before any Appearance override is applied, so "System
    // default" can be restored exactly (rather than guessing at wx's own
    // default UI font).
    wxFont defaultUiFont_;
    std::size_t currentEffect_ = 0;
    std::size_t currentOffset_ = 0;
    std::size_t currentY_ = 0;
    // Running remainder of unconsumed wheel rotation (see onEditorMouseWheel)
    // -- a trackpad's two-finger scroll sends many more, finer-grained
    // wheel events per gesture than a physical mouse wheel notch, so acting
    // on every event's fixed step made it feel far too fast there.
    int wheelAccum_ = 0;
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
    class PianoWindow* pianoWindow_ = nullptr;
};
