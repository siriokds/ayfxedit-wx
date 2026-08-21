#include "WindowGeometry.h"

#include <wx/display.h>
#include <wx/toplevel.h>

#include <algorithm>

namespace {

constexpr int kTitleBarHeightPx = 32;
constexpr int kMinReachablePx = 120;
// Guards against corrupt/hand-edited settings.json overflowing rect
// arithmetic below, not a real-world screen size limit.
constexpr int kMaxGeometryPx = 32000;

// The work area (usable screen space, excluding menu bar/dock/taskbar) of
// whichever connected display has the largest overlap with `r` -- e.g. the
// display a window was last on. Falls back to the primary display if `r`
// doesn't overlap any of them (that display was unplugged/rearranged since
// the geometry was saved).
wxRect BestWorkAreaFor(const wxRect& r) {
    const unsigned count = wxDisplay::GetCount();
    wxLongLong_t bestArea = 0;
    wxRect best;
    for (unsigned i = 0; i < count; ++i) {
        const wxDisplay display(i);
        if (!display.IsOk()) continue;
        const wxRect work = display.GetClientArea();
        const wxRect hit = work.Intersect(r);
        const wxLongLong_t area = static_cast<wxLongLong_t>(hit.GetWidth()) * hit.GetHeight();
        if (area > bestArea) {
            bestArea = area;
            best = work;
        }
    }
    if (bestArea > 0) return best;
    const wxDisplay primary(0u);
    return primary.IsOk() ? primary.GetClientArea() : wxRect(0, 0, 1280, 800);
}

// A window whose title bar has no reachable strip within `work` can't be
// dragged back on-screen by the user -- this is the actual failure mode
// being guarded against, not just "is any part of the window visible".
bool IsTitleBarReachable(const wxRect& r, const wxRect& work) {
    if (r.GetY() < work.GetY()) return false;
    if (r.GetY() > work.GetBottom()) return false;
    const wxRect bar(r.GetX(), r.GetY(), r.GetWidth(), kTitleBarHeightPx);
    const wxRect hit = work.Intersect(bar);
    if (hit.IsEmpty()) return false;
    return hit.GetWidth() >= std::min(kMinReachablePx, r.GetWidth());
}

// Shrinks `r` to fit within its best-matching display's work area (never
// below `minSize`), then re-clamps its position only if that leaves the
// title bar unreachable -- a window intentionally hanging off an edge is
// left alone rather than being re-centered on every restore.
wxRect FitToDisplays(const wxRect& r, const wxSize& minSize) {
    const wxRect work = BestWorkAreaFor(r);
    int w = std::min(r.GetWidth(), work.GetWidth());
    int h = std::min(r.GetHeight(), work.GetHeight());
    if (minSize.GetWidth() > 0) w = std::max(w, minSize.GetWidth());
    if (minSize.GetHeight() > 0) h = std::max(h, minSize.GetHeight());

    const wxRect resized(r.GetX(), r.GetY(), w, h);
    if (IsTitleBarReachable(resized, work)) {
        return resized;
    }

    const int maxX = std::max(work.GetX(), work.GetX() + work.GetWidth() - w);
    const int maxY = std::max(work.GetY(), work.GetY() + work.GetHeight() - h);
    return wxRect(std::clamp(r.GetX(), work.GetX(), maxX),
                  std::clamp(r.GetY(), work.GetY(), maxY), w, h);
}

}  // namespace

void CaptureWindowGeometry(const wxTopLevelWindow& win, WindowGeometry& out) {
    out.maximized = win.IsMaximized();
    if (out.maximized) {
        return;  // GetRect() would be the maximized bounds, not a "normal" size to restore into
    }
    const wxRect r = win.GetRect();
    out.x = r.GetX();
    out.y = r.GetY();
    out.w = r.GetWidth();
    out.h = r.GetHeight();
    out.posValid = true;
}

void RestoreWindowGeometry(wxTopLevelWindow& win, const WindowGeometry& geometry) {
    if (geometry.w > 0 && geometry.h > 0) {
        const wxRect saved(std::clamp(geometry.x, -kMaxGeometryPx, kMaxGeometryPx),
                           std::clamp(geometry.y, -kMaxGeometryPx, kMaxGeometryPx),
                           std::clamp(geometry.w, 1, kMaxGeometryPx),
                           std::clamp(geometry.h, 1, kMaxGeometryPx));

        if (geometry.posValid) {
            win.SetSize(FitToDisplays(saved, win.GetMinSize()));
        } else {
            // Size was saved but never a real position (e.g. settings.json
            // predates this feature) -- keep the saved size, but centre it
            // on the best-matching display instead of using (0, 0).
            const wxRect work = BestWorkAreaFor(win.GetRect());
            const wxRect centred(work.GetX() + (work.GetWidth() - saved.GetWidth()) / 2,
                                 work.GetY() + (work.GetHeight() - saved.GetHeight()) / 2,
                                 saved.GetWidth(), saved.GetHeight());
            win.SetSize(FitToDisplays(centred, win.GetMinSize()));
        }
    }
    // else: never saved (or only ever saved while maximized) -- keep
    // whichever default size the window was constructed with.

    if (geometry.maximized) {
        win.Maximize(true);
    }
}
