#include "ClockPicker.h"

#include <wx/choice.h>
#include <wx/spinctrl.h>

#include <cstddef>
#include <iterator>

#include "../audio/AudioEngine.h"

namespace {

struct MachinePreset { const char* label; int clockHz; };
const MachinePreset kMachinePresets[] = {
    {"ZX Spectrum", kAYClockRateSpectrum},
    {"MSX", kAYClockRateMsx},
    {"Amstrad CPC", kAYClockRateCpc},
    {"Atari ST", kAYClockRateAtariSt},
};
constexpr int kMachinePresetCount = static_cast<int>(std::size(kMachinePresets));

}  // namespace

const char* MachineNameForClock(int clockHz) {
    for (const auto& p : kMachinePresets) {
        if (p.clockHz == clockHz) return p.label;
    }
    return "Custom";
}

ClockPicker CreateClockPicker(wxWindow* parent, int initialHz) {
    const auto& kPresets = kMachinePresets;
    const int kPresetCount = kMachinePresetCount;

    auto* machineChoice = new wxChoice(parent, wxID_ANY);
    for (const auto& p : kPresets) machineChoice->Append(p.label);
    machineChoice->Append("Custom");

    auto* clockSpin = new wxSpinCtrl(parent, wxID_ANY, wxEmptyString,
                                     wxDefaultPosition, wxDefaultSize,
                                     wxSP_ARROW_KEYS, 100000, 20000000, initialHz);

    int initialSel = kPresetCount;  // Custom
    for (int i = 0; i < kPresetCount; ++i) {
        if (kPresets[i].clockHz == initialHz) { initialSel = i; break; }
    }
    machineChoice->SetSelection(initialSel);

    machineChoice->Bind(wxEVT_CHOICE, [=](wxCommandEvent&) {
        const int sel = machineChoice->GetSelection();
        if (sel >= 0 && sel < kPresetCount) {
            clockSpin->SetValue(kPresets[sel].clockHz);
        }
    });
    clockSpin->Bind(wxEVT_SPINCTRL, [=](wxSpinEvent&) {
        const int v = clockSpin->GetValue();
        int sel = kPresetCount;  // Custom
        for (int i = 0; i < kPresetCount; ++i) {
            if (kPresets[i].clockHz == v) { sel = i; break; }
        }
        machineChoice->SetSelection(sel);
    });

    return {machineChoice, clockSpin};
}
