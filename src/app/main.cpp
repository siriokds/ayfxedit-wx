#include <wx/wx.h>
#include "MainFrame.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

#ifdef _WIN32

struct BundledFontRegistration {
    HANDLE regular = nullptr;
    HANDLE bold = nullptr;

    ~BundledFontRegistration() {
        if (regular) {
            RemoveFontMemResourceEx(regular);
        }
        if (bold) {
            RemoveFontMemResourceEx(bold);
        }
    }
};

bool RegisterBundledFontResource(int resourceId, HANDLE& outHandle) {
    HRSRC resource = FindResourceW(nullptr, MAKEINTRESOURCEW(resourceId), RT_RCDATA);
    if (!resource) {
        return false;
    }

    const DWORD size = SizeofResource(nullptr, resource);
    if (size == 0) {
        return false;
    }

    HGLOBAL loaded = LoadResource(nullptr, resource);
    if (!loaded) {
        return false;
    }

    void* bytes = LockResource(loaded);
    if (!bytes) {
        return false;
    }

    DWORD count = 0;
    outHandle = AddFontMemResourceEx(bytes, size, nullptr, &count);
    return outHandle != nullptr && count > 0;
}

bool RegisterBundledFonts() {
    static BundledFontRegistration registration;
    static bool attempted = false;
    static bool loaded = false;

    if (attempted) {
        return loaded;
    }

    attempted = true;
    const bool regularOk = RegisterBundledFontResource(101, registration.regular);
    const bool boldOk = RegisterBundledFontResource(102, registration.bold);
    loaded = regularOk && boldOk;
    return loaded;
}

#else

bool RegisterBundledFonts() {
    return false;
}

#endif

}  // namespace

class AyfxApp final : public wxApp {
public:
    bool OnInit() override {
        // Deliberately set: without it, wx derives the app name (used by
        // wxStandardPaths for the settings file's directory, among other
        // things) from the executable name, which would silently change if
        // the build target is ever renamed.
        SetAppName("ayfxedit-wx");

#ifdef _WIN32
        // Opt into following the OS light/dark theme; MSW otherwise defaults
        // to light regardless of the system setting (wxWidgets 3.3+).
        //
        // Deliberately NOT called on macOS/GTK: appearance already follows
        // the system there automatically and live (verified — toggling OS
        // dark mode while the app is running restyles it immediately). Calling
        // SetAppearance(System) on macOS was tested and found to pin the
        // window to whatever appearance was active at startup instead,
        // breaking live switching (wxWidgets 3.3.2) — so it's MSW-only here.
        SetAppearance(Appearance::System);
#endif

        RegisterBundledFonts();
        auto* frame = new MainFrame();
        frame->Show(true);
        return true;
    }
};

wxIMPLEMENT_APP(AyfxApp);
