#pragma once

class wxChoice;
class wxSpinCtrl;
class wxWindow;

// Preset + numeric-Hz widget pair shared between the Preferences "Engine"
// page's Machine picker and the reclock tool's source/destination pickers.
// Picking a preset fills in the spin control; hand-editing the spin
// control (to a value not matching any preset) switches the choice to
// "Custom".
struct ClockPicker {
    wxChoice* machineChoice;
    wxSpinCtrl* clockSpin;
};

ClockPicker CreateClockPicker(wxWindow* parent, int initialHz);

// The preset name for a clock value, or "Custom" if it doesn't match one
// of the known machine presets exactly. Used by the status bar summary.
const char* MachineNameForClock(int clockHz);
