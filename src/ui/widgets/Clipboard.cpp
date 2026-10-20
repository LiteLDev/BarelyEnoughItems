#include "ui/widgets/Clipboard.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#pragma comment(lib, "user32.lib") // clipboard and key state

#include <cstring>

namespace bei::ui::clipboard {

namespace {
/// A top-level window of this process (the game window) to own the clipboard. With a NULL owner
/// EmptyClipboard leaves the clipboard ownerless and SetClipboardData fails.
HWND gameWindow() {
    DWORD pid   = GetCurrentProcessId();
    HWND  fg    = GetForegroundWindow();
    DWORD fgPid = 0;
    if (fg && GetWindowThreadProcessId(fg, &fgPid) && fgPid == pid) return fg;
    struct Search {
        DWORD pid;
        HWND  found;
    } search{pid, nullptr};
    EnumWindows(
        [](HWND hwnd, LPARAM param) -> BOOL {
            auto* s    = reinterpret_cast<Search*>(param);
            DWORD wpid = 0;
            GetWindowThreadProcessId(hwnd, &wpid);
            if (wpid != s->pid || !IsWindowVisible(hwnd) || GetWindow(hwnd, GW_OWNER)) return TRUE;
            s->found = hwnd;
            return FALSE;
        },
        reinterpret_cast<LPARAM>(&search)
    );
    return search.found;
}

/// OpenClipboard fails while another process holds it; retry briefly like most editors do.
bool openClipboard(HWND owner = nullptr) {
    for (int i = 0; i < 5; ++i) {
        if (OpenClipboard(owner)) return true;
        Sleep(2);
    }
    return false;
}

std::string toUtf8(wchar_t const* w, std::size_t len) {
    if (len == 0) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w, static_cast<int>(len), nullptr, 0, nullptr, nullptr);
    if (n <= 0) return {};
    std::string out(static_cast<std::size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, static_cast<int>(len), out.data(), n, nullptr, nullptr);
    return out;
}

std::wstring toUtf16(std::string const& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    if (n <= 0) return {};
    std::wstring out(static_cast<std::size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), n);
    return out;
}
} // namespace

std::string getText() {
    if (!IsClipboardFormatAvailable(CF_UNICODETEXT) || !openClipboard()) return {};
    std::string out;
    if (HANDLE h = GetClipboardData(CF_UNICODETEXT)) {
        if (auto const* w = static_cast<wchar_t const*>(GlobalLock(h))) {
            // The buffer is NUL-terminated; still never read past the allocation.
            std::size_t cap = GlobalSize(h) / sizeof(wchar_t);
            std::size_t len = 0;
            while (len < cap && w[len] != L'\0') ++len;
            out = toUtf8(w, len);
            GlobalUnlock(h);
        }
    }
    CloseClipboard();
    return out;
}

bool setText(std::string const& utf8) {
    std::wstring w     = toUtf16(utf8);
    HWND         owner = gameWindow();
    if (!owner || !openClipboard(owner)) return false;
    bool ok = false;
    EmptyClipboard();
    if (HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, (w.size() + 1) * sizeof(wchar_t))) {
        if (void* dst = GlobalLock(mem)) {
            std::memcpy(dst, w.c_str(), (w.size() + 1) * sizeof(wchar_t));
            GlobalUnlock(mem);
            ok = SetClipboardData(CF_UNICODETEXT, mem) != nullptr;
        }
        if (!ok) GlobalFree(mem); // the system owns the memory only after a successful SetClipboardData
    }
    CloseClipboard();
    return ok;
}

} // namespace bei::ui::clipboard
