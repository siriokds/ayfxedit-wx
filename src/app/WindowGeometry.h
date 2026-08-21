#pragma once

#include "../core/Settings.h"

class wxTopLevelWindow;

// Captures/restores a top-level window's position, size, and maximized
// state across sessions. Ported (simplified) from the user's other project,
// Dual, which tracks geometry live via debounced move/resize events; here
// it's captured once at close instead (see MainFrame's wxEVT_CLOSE_WINDOW
// handler), which is enough for "reopen where you left it" without the
// complexity of live tracking.
//
// The restore side keeps Dual's multi-monitor safety net: a saved rect is
// clamped to fit whichever connected display best overlaps it (falling back
// to the primary display if none do, e.g. a monitor was unplugged since the
// last session), so the window can never reopen with its title bar
// unreachable.

// Captures `win`'s current geometry into `out`. If `win` is currently
// maximized, only `out.maximized` is updated -- x/y/w/h are left as they
// were, since the maximized rect isn't a meaningful "normal" size/position
// to restore into. This means a window that's never been un-maximized since
// its geometry was first captured has no saved normal rect yet; that's fine,
// RestoreWindowGeometry() falls back to the platform default size in that
// case (see below).
void CaptureWindowGeometry(const wxTopLevelWindow& win, WindowGeometry& out);

// Restores `win`'s position/size/maximized state from `geometry`. No-op on
// position/size if geometry.w/h aren't set (e.g. first launch, or a window
// that's only ever been captured while maximized) -- the window keeps
// whatever default size it was constructed with. Maximized state is always
// applied if set, independently of whether a normal rect was restored.
void RestoreWindowGeometry(wxTopLevelWindow& win, const WindowGeometry& geometry);
