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
        RegisterBundledFonts();
        auto* frame = new MainFrame();
        frame->Show(true);
        return true;
    }
};

wxIMPLEMENT_APP(AyfxApp);
