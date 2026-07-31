// SettingsInput.cpp — Ayarlar penceresinin mesajları, okuma/yazma ve ömrü.
// Denetimler, yerleşim ve çizim SettingsWindow.cpp'dedir.
#include "SettingsInternal.h"

#include "Geometry.h"
#include "History.h"
#include "HotkeyEdit.h"
#include "ImageCodec.h"
#include "Localization.h"
#include "SettingsWindow.h"
#include "Theme.h"
#include "Util.h"
#include "resource.h"

#include <commctrl.h>
#include <shellscalingapi.h>
#include <shlobj.h>

#include <string>

namespace crisp {
namespace settings_ui {
namespace {

constexpr const wchar_t* kWindowClass = L"CrispSettingsWindow";

HWND g_open = nullptr;

void SetCheck(HWND window, int id, bool checked) {
    ::SendDlgItemMessageW(window, id, BM_SETCHECK,
                          checked ? BST_CHECKED : BST_UNCHECKED, 0);
}

[[nodiscard]] bool GetCheck(HWND window, int id) {
    return ::SendDlgItemMessageW(window, id, BM_GETCHECK, 0, 0) == BST_CHECKED;
}

void SetNumber(HWND window, int id, unsigned value) {
    wchar_t text[16];
    ::swprintf_s(text, L"%u", value);
    ::SetDlgItemTextW(window, id, text);
}

[[nodiscard]] unsigned GetNumber(HWND window, int id, unsigned fallback) {
    BOOL ok = FALSE;
    const UINT value = ::GetDlgItemInt(window, id, &ok, FALSE);
    return ok ? value : fallback;
}

[[nodiscard]] std::wstring GetText(HWND window, int id) {
    const HWND control = ::GetDlgItem(window, id);
    if (control == nullptr) {
        return std::wstring();
    }
    const int length = ::GetWindowTextLengthW(control);
    if (length <= 0) {
        return std::wstring();
    }
    std::wstring text(static_cast<size_t>(length), L'\0');
    ::GetWindowTextW(control, text.data(), length + 1);
    return text;
}

void SetHotkey(HWND window, int id, const Hotkey& hotkey) {
    SetHotkeyValue(::GetDlgItem(window, id), hotkey);
}

[[nodiscard]] Hotkey GetHotkey(HWND window, int id) {
    return GetHotkeyValue(::GetDlgItem(window, id));
}

[[nodiscard]] int FormatIndex(const std::wstring& format) noexcept {
    if (format == L"jpg") {
        return 1;
    }
    if (format == L"webp") {
        return 2;
    }
    return 0;
}

[[nodiscard]] const wchar_t* FormatCode(int index) noexcept {
    switch (index) {
        case 1:  return L"jpg";
        case 2:  return L"webp";
        default: return L"png";
    }
}

[[nodiscard]] int ThemeIndex(const std::wstring& theme) noexcept {
    if (theme == L"light") {
        return 1;
    }
    if (theme == L"dark") {
        return 2;
    }
    return 0;
}

[[nodiscard]] const wchar_t* ThemeCode(int index) noexcept {
    switch (index) {
        case 1:  return L"light";
        case 2:  return L"dark";
        default: return L"system";
    }
}

// Klasör seçtirir. IFileOpenDialog + FOS_PICKFOLDERS; SHBrowseForFolder de
// çalışırdı ama Windows 2000'den kalma ağaç görünümüyle açılır ve kullanıcı
// yeni klasör oluşturmak için uğraşır.
[[nodiscard]] bool PickFolder(HWND owner, std::wstring& folder) {
    ComPtr<IFileOpenDialog> dialog;
    if (FAILED(::CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(dialog.GetAddressOf())))) {
        return false;
    }

    DWORD options = 0;
    if (SUCCEEDED(dialog->GetOptions(&options))) {
        (void)dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
    }
    (void)dialog->SetTitle(Loc::Str(IDS_SET_FOLDER_PROMPT).c_str());

    if (FAILED(dialog->Show(owner))) {
        return false;   // kullanıcı iptal etti
    }

    ComPtr<IShellItem> item;
    if (FAILED(dialog->GetResult(item.GetAddressOf()))) {
        return false;
    }
    PWSTR raw = nullptr;
    if (FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &raw)) || raw == nullptr) {
        return false;
    }
    folder.assign(raw);
    ::CoTaskMemFree(raw);
    return true;
}

void ResetToDefaults(HWND window, State& state) {
    // Dil ve tema KORUNUR: kullanıcı "varsayılanlar"a basınca arayüzün
    // anlamadığı bir dile dönmesi, ayarları geri almaktan çok kaybetmek olurdu.
    const std::wstring language = state.working.language;
    const std::wstring theme = state.working.theme;
    state.working = Settings{};
    state.working.language = language;
    state.working.theme = theme;
    LoadIntoControls(window, state);
}

void ClearHistory(HWND window) {
    HistoryStore store;
    store.SetFolder(HistoryStore::DefaultFolder());
    const size_t removed = store.Clear();
    LogV(L"Geçmişten %zu kayıt silindi", removed);
    ::MessageBoxW(window, Loc::Str(IDS_SET_HISTORY_CLEARED).c_str(),
                  Loc::Str(IDS_APP_TITLE).c_str(), MB_OK | MB_ICONINFORMATION);
}

LRESULT CALLBACK SettingsProc(HWND window, UINT message, WPARAM wParam,
                              LPARAM lParam) {
    auto* state =
        reinterpret_cast<State*>(::GetWindowLongPtrW(window, GWLP_USERDATA));

    switch (message) {
        case WM_NCCREATE: {
            const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
            ::SetWindowLongPtrW(window, GWLP_USERDATA,
                                reinterpret_cast<LONG_PTR>(create->lpCreateParams));
            return ::DefWindowProcW(window, message, wParam, lParam);
        }

        case WM_CREATE:
            if (state != nullptr) {
                BuildControls(window, *state);
                LoadIntoControls(window, *state);
            }
            return 0;

        case WM_PAINT:
            if (state != nullptr) {
                Paint(window, *state);
            }
            return 0;

        case WM_ERASEBKGND:
            return 1;

        // Koyu temada denetim metinleri siyah kalırdı; WM_CTLCOLOR* zemin
        // fırçasını ve metin rengini bizim vermemizi sağlar.
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN: {
            if (state == nullptr) {
                break;
            }
            const HDC dc = reinterpret_cast<HDC>(wParam);
            ::SetTextColor(dc, theme::Colors().text);
            ::SetBkMode(dc, TRANSPARENT);
            return reinterpret_cast<LRESULT>(state->backgroundBrush);
        }

        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX: {
            if (state == nullptr || !theme::IsDark()) {
                break;
            }
            const HDC dc = reinterpret_cast<HDC>(wParam);
            ::SetTextColor(dc, theme::Colors().text);
            ::SetBkColor(dc, theme::Colors().surfaceAlt);
            return reinterpret_cast<LRESULT>(state->backgroundBrush);
        }

        case WM_COMMAND: {
            if (state == nullptr) {
                break;
            }
            switch (LOWORD(wParam)) {
                case kIdOk:
                    ReadFromControls(window, *state);
                    state->working.Clamp();
                    state->accepted = true;
                    ::DestroyWindow(window);
                    return 0;
                case kIdCancel:
                    ::DestroyWindow(window);
                    return 0;
                case kIdReset:
                    ResetToDefaults(window, *state);
                    return 0;
                case kIdHistoryClear:
                    ClearHistory(window);
                    return 0;
                case kIdBrowse: {
                    std::wstring folder = GetText(window, kIdFolder);
                    if (PickFolder(window, folder)) {
                        ::SetDlgItemTextW(window, kIdFolder, folder.c_str());
                    }
                    return 0;
                }
                default:
                    break;
            }
            break;
        }

        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                ::DestroyWindow(window);
                return 0;
            }
            break;

        case WM_DESTROY:
            g_open = nullptr;
            ::PostQuitMessage(0);
            return 0;

        default:
            break;
    }

    return ::DefWindowProcW(window, message, wParam, lParam);
}

[[nodiscard]] bool EnsureWindowClass(HINSTANCE instance) {
    static bool registered = false;
    if (registered) {
        return true;
    }
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = SettingsProc;
    wc.hInstance = instance;
    wc.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kWindowClass;
    wc.hIcon = ::LoadIconW(instance, MAKEINTRESOURCEW(IDI_APP));
    wc.hbrBackground = nullptr;
    registered = ::RegisterClassExW(&wc) != 0;
    return registered;
}

}  // namespace

void LoadIntoControls(HWND window, const State& state) {
    const Settings& s = state.working;

    int languageIndex = 0;
    for (size_t i = 0; i < state.languageCodes.size(); ++i) {
        if (state.languageCodes[i] == s.language) {
            languageIndex = static_cast<int>(i);
            break;
        }
    }
    ::SendDlgItemMessageW(window, kIdLanguage, CB_SETCURSEL,
                          static_cast<WPARAM>(languageIndex), 0);
    ::SendDlgItemMessageW(window, kIdTheme, CB_SETCURSEL,
                          static_cast<WPARAM>(ThemeIndex(s.theme)), 0);

    ::SetDlgItemTextW(window, kIdFolder, s.EffectiveSaveFolder().c_str());
    ::SendDlgItemMessageW(window, kIdFormat, CB_SETCURSEL,
                          static_cast<WPARAM>(FormatIndex(s.saveFormat)), 0);
    SetNumber(window, kIdQuality, s.saveQuality);
    SetNumber(window, kIdHistoryLimit, s.historyLimit);
    SetNumber(window, kIdDelay, s.delaySeconds);

    SetCheck(window, kIdMagnifier, s.showMagnifier);
    SetCheck(window, kIdHighlight, s.showWindowHighlight);
    SetCheck(window, kIdPrintScreen, s.printScreenCapture);
    SetCheck(window, kIdShutter, s.playShutterSound);
    SetCheck(window, kIdAfterCopy, s.after.copyToClipboard);
    SetCheck(window, kIdAfterSave, s.after.saveToFile);
    SetCheck(window, kIdAfterPin, s.after.pinToScreen);
    SetCheck(window, kIdAfterEditor, s.after.openEditor);
    SetCheck(window, kIdNotify, s.showNotification);

    SetHotkey(window, kIdHotkeyRegion, s.hotkeyRegion);
    SetHotkey(window, kIdHotkeyWindow, s.hotkeyWindow);
    SetHotkey(window, kIdHotkeyFullScreen, s.hotkeyFullScreen);
    SetHotkey(window, kIdHotkeyDelayed, s.hotkeyDelayed);
}

void ReadFromControls(HWND window, State& state) {
    Settings& s = state.working;

    const LRESULT languageIndex =
        ::SendDlgItemMessageW(window, kIdLanguage, CB_GETCURSEL, 0, 0);
    if (languageIndex >= 0 &&
        static_cast<size_t>(languageIndex) < state.languageCodes.size()) {
        s.language = state.languageCodes[static_cast<size_t>(languageIndex)];
    }

    s.theme = ThemeCode(static_cast<int>(
        ::SendDlgItemMessageW(window, kIdTheme, CB_GETCURSEL, 0, 0)));
    s.saveFormat = FormatCode(static_cast<int>(
        ::SendDlgItemMessageW(window, kIdFormat, CB_GETCURSEL, 0, 0)));

    s.saveFolder = GetText(window, kIdFolder);
    s.saveQuality = GetNumber(window, kIdQuality, s.saveQuality);
    s.historyLimit = GetNumber(window, kIdHistoryLimit, s.historyLimit);
    s.delaySeconds = GetNumber(window, kIdDelay, s.delaySeconds);

    s.showMagnifier = GetCheck(window, kIdMagnifier);
    s.showWindowHighlight = GetCheck(window, kIdHighlight);
    s.printScreenCapture = GetCheck(window, kIdPrintScreen);
    s.playShutterSound = GetCheck(window, kIdShutter);
    s.after.copyToClipboard = GetCheck(window, kIdAfterCopy);
    s.after.saveToFile = GetCheck(window, kIdAfterSave);
    s.after.pinToScreen = GetCheck(window, kIdAfterPin);
    s.after.openEditor = GetCheck(window, kIdAfterEditor);
    s.showNotification = GetCheck(window, kIdNotify);

    s.hotkeyRegion = GetHotkey(window, kIdHotkeyRegion);
    s.hotkeyWindow = GetHotkey(window, kIdHotkeyWindow);
    s.hotkeyFullScreen = GetHotkey(window, kIdHotkeyFullScreen);
    s.hotkeyDelayed = GetHotkey(window, kIdHotkeyDelayed);
}

}  // namespace settings_ui

bool ShowSettingsWindow(HINSTANCE instance, Settings& settings) {
    using namespace settings_ui;

    if (g_open != nullptr) {
        ::SetForegroundWindow(g_open);
        return false;
    }
    if (!EnsureWindowClass(instance)) {
        return false;
    }

    State state;
    state.target = &settings;
    state.working = settings;

    POINT cursor{};
    ::GetCursorPos(&cursor);
    const HMONITOR monitor = ::MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);

    MONITORINFO info{};
    info.cbSize = sizeof(info);
    RECT work{0, 0, 1920, 1080};
    if (::GetMonitorInfoW(monitor, &info)) {
        work = info.rcWork;
    }

    UINT dpiX = 96;
    UINT dpiY = 96;
    if (FAILED(::GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY))) {
        dpiX = 96;
    }
    state.dpi = dpiX;

    LOGFONTW font{};
    font.lfHeight = -::MulDiv(9, static_cast<int>(state.dpi), 72);
    font.lfWeight = FW_NORMAL;
    font.lfCharSet = DEFAULT_CHARSET;
    font.lfQuality = CLEARTYPE_QUALITY;
    ::wcscpy_s(font.lfFaceName, L"Segoe UI");
    state.font = ::CreateFontIndirectW(&font);
    font.lfHeight = -::MulDiv(10, static_cast<int>(state.dpi), 72);
    font.lfWeight = FW_SEMIBOLD;
    state.groupFont = ::CreateFontIndirectW(&font);
    state.backgroundBrush = ::CreateSolidBrush(theme::Colors().surface);

    // İstemci alanı tam olarak tasarım ölçüsünde olmalı; pencere ölçüsü
    // verilseydi kenarlık ve başlık çubuğu içeriden çalardı ve alt düğmeler
    // kırpılırdı.
    RECT desired{0, 0, Scale(kWidth, state.dpi), Scale(kHeight, state.dpi)};
    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
    ::AdjustWindowRectEx(&desired, style, FALSE, 0);
    const int width = static_cast<int>(geom::Width(desired));
    const int height = static_cast<int>(geom::Height(desired));
    const int x = work.left + (static_cast<int>(geom::Width(work)) - width) / 2;
    const int y = work.top + (static_cast<int>(geom::Height(work)) - height) / 2;

    const HWND window = ::CreateWindowExW(
        WS_EX_DLGMODALFRAME, kWindowClass, Loc::Str(IDS_SETTINGS_TITLE).c_str(),
        style, x, y, width, height, nullptr, nullptr, instance, &state);

    if (window == nullptr) {
        return false;
    }

    g_open = window;
    theme::ApplyToWindow(window);
    ::ShowWindow(window, SW_SHOW);
    ::SetForegroundWindow(window);

    MSG message{};
    while (::GetMessageW(&message, nullptr, 0, 0) > 0) {
        // IsDialogMessage OLMADAN Tab tuşu denetimler arasında dolaşmaz ve
        // pencere klavyeyle kullanılamaz hâle gelir.
        if (!::IsDialogMessageW(window, &message)) {
            ::TranslateMessage(&message);
            ::DispatchMessageW(&message);
        }
    }

    if (state.font != nullptr) {
        ::DeleteObject(state.font);
    }
    if (state.groupFont != nullptr) {
        ::DeleteObject(state.groupFont);
    }
    if (state.backgroundBrush != nullptr) {
        ::DeleteObject(state.backgroundBrush);
    }

    if (state.accepted) {
        settings = state.working;
    }
    return state.accepted;
}

}  // namespace crisp
