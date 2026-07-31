// App.cpp — Pencere yaşam döngüsü ve mesaj yönlendirme.
// Yakalama akışları AppCapture.cpp'dedir.
#include "App.h"

#include "Localization.h"
#include "Messages.h"
#include "Ocr.h"
#include "PinWindow.h"
#include "Theme.h"
#include "Util.h"
#include "resource.h"

// Sürüm tek yerde: res/app.rc'deki VERSIONINFO ile elle eşleştirilmesi
// gereken bir ikinci kopya olmasın diye burada tanımlanır.
#define CRISP_VERSION_TEXT L"0.3.0"

#include <commctrl.h>

#include <string>

namespace crisp {
namespace {

// Görev çubuğu teması kayıt defterinde değişir ve bunun için bir bildirim
// mesajı yoktur. Yoklama aralığı: iki saniyede bir tek bir RegGetValue,
// ölçülemeyecek kadar ucuz.
constexpr UINT kThemePollMs = 2000;

}  // namespace

HWND App::FindExistingInstanceWindow() {
    return ::FindWindowW(kHostWindowClass, kHostWindowTitle);
}

bool App::Initialize(HINSTANCE instance) {
    m_instance = instance;

    INITCOMMONCONTROLSEX controls{};
    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_STANDARD_CLASSES;
    ::InitCommonControlsEx(&controls);

    m_settings.Load(SettingsStore::ForApp());

    // Dil, HER ŞEYDEN ÖNCE kurulur: bundan sonraki her hata iletisi ve menü
    // metni Loc::Str'den gelir ve dil kurulmadan çağrılırsa boş döner.
    Loc::Initialize(instance, m_settings.language);

    // Tema, PENCERE OLUŞTURULMADAN önce kurulur: SetPreferredAppMode
    // sonradan çağrılırsa mevcut pencereler açık temada kalır.
    theme::Initialize(theme::ModeFromString(m_settings.theme.c_str()));

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = &App::WindowProc;
    wc.hInstance = instance;
    wc.lpszClassName = kHostWindowClass;
    wc.hIcon = ::LoadIconW(instance, MAKEINTRESOURCEW(IDI_APP));

    if (::RegisterClassExW(&wc) == 0) {
        LogV(L"Ana pencere sınıfı kaydedilemedi (hata %lu)", ::GetLastError());
        return false;
    }

    // HWND_MESSAGE ÇOCUĞU DEĞİL: yalnızca-mesaj pencereleri global kısayol
    // (WM_HOTKEY) ve kabuk yayın mesajlarını (TaskbarCreated) ALMAZ. Bu yüzden
    // sıradan ama hiç gösterilmeyen bir üst düzey pencere kullanılır.
    m_window = ::CreateWindowExW(WS_EX_TOOLWINDOW, kHostWindowClass,
                                 kHostWindowTitle, WS_POPUP, 0, 0, 0, 0, nullptr,
                                 nullptr, instance, this);
    if (m_window == nullptr) {
        LogV(L"Ana pencere oluşturulamadı (hata %lu)", ::GetLastError());
        return false;
    }

    if (!m_tray.Add(m_window, instance)) {
        return false;
    }

    const int failed = m_hotkeys.Apply(m_window, m_settings);
    if (failed > 0) {
        LogV(L"%d kısayol kaydedilemedi", failed);
    }

    ::SetTimer(m_window, TIMER_THEME, kThemePollMs, nullptr);
    return true;
}

int App::Run() {
    MSG message{};
    while (::GetMessageW(&message, nullptr, 0, 0) > 0) {
        ::TranslateMessage(&message);
        ::DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}

LRESULT CALLBACK App::WindowProc(HWND window, UINT message, WPARAM wParam,
                                 LPARAM lParam) {
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
        ::SetWindowLongPtrW(window, GWLP_USERDATA,
                            reinterpret_cast<LONG_PTR>(create->lpCreateParams));
        return ::DefWindowProcW(window, message, wParam, lParam);
    }

    auto* app = reinterpret_cast<App*>(::GetWindowLongPtrW(window, GWLP_USERDATA));
    if (app == nullptr) {
        return ::DefWindowProcW(window, message, wParam, lParam);
    }
    return app->HandleMessage(window, message, wParam, lParam);
}

LRESULT App::HandleMessage(HWND window, UINT message, WPARAM wParam,
                           LPARAM lParam) {
    // Explorer yeniden başladığında tepsi simgesi silinir; kabuk bunu bir
    // yayın mesajıyla duyurur ve simge yeniden eklenmelidir.
    if (message == TrayIcon::TaskbarCreatedMessage()) {
        m_tray.Restore();
        return 0;
    }

    switch (message) {
        case WM_CRISP_TRAY: {
            // NOTIFYICON_VERSION_4 ile olay kodu lParam'ın alt sözcüğündedir.
            const UINT event = LOWORD(lParam);
            if (event == WM_RBUTTONUP || event == WM_CONTEXTMENU) {
                const int command = m_tray.ShowMenu(window);
                if (command != 0) {
                    OnCommand(command);
                }
            } else if (event == WM_LBUTTONUP) {
                OnCommand(IDM_CAPTURE_REGION);
            }
            return 0;
        }

        case WM_HOTKEY:
            OnHotkey(static_cast<int>(wParam));
            return 0;

        case WM_COMMAND:
            OnCommand(LOWORD(wParam));
            return 0;

        // Kullanıcı Windows'un tema ayarını değiştirdiğinde kabuk bunu
        // yayımlar. lParam "ImmersiveColorSet" olmayan bildirimler bizi
        // ilgilendirmez; her WM_SETTINGCHANGE'de menü temasını boşaltmak
        // gereksiz iş olurdu.
        case WM_SETTINGCHANGE: {
            const auto* section = reinterpret_cast<const wchar_t*>(lParam);
            if (section != nullptr && ::wcscmp(section, L"ImmersiveColorSet") == 0) {
                (void)theme::RefreshFromSystem();
                m_tray.RefreshTheme();
            }
            return 0;
        }

        case WM_TIMER: {
            if (wParam == TIMER_THEME) {
                m_tray.RefreshTheme();
                return 0;
            }
            if (wParam == TIMER_DELAY) {
                ::KillTimer(window, TIMER_DELAY);
                m_busy = false;
                CaptureRegionOrWindow(false);
                return 0;
            }
            break;
        }

        // Ayarlar penceresi başka bir örnekten açılma isteği gönderebilir;
        // şimdilik yalnızca bölge yakalamayı tetikler.
        case WM_DESTROY:
            CloseAllPins();
            m_hotkeys.UnregisterAll();
            m_tray.Remove();
            ::PostQuitMessage(0);
            return 0;

        default:
            break;
    }

    return ::DefWindowProcW(window, message, wParam, lParam);
}

void App::OnCommand(int command) {
    switch (command) {
        case IDM_CAPTURE_REGION:
            StartCapture(CaptureMode::Region);
            break;
        case IDM_CAPTURE_WINDOW:
            StartCapture(CaptureMode::Window);
            break;
        case IDM_CAPTURE_FULLSCREEN:
            StartCapture(CaptureMode::FullScreen);
            break;
        case IDM_CAPTURE_DELAYED:
            StartCapture(CaptureMode::Delayed);
            break;
        case IDM_SELECT_TEXT:
            SelectTextOnScreen();
            break;
        case IDM_CAPTURE_OCR:
            CaptureTextToClipboard();
            break;
        case IDM_PICK_COLOR:
            PickColorToClipboard();
            break;
        case IDM_OPEN_FOLDER:
            OpenSaveFolder();
            break;
        case IDM_ABOUT: {
            const std::wstring text =
                L"Crisp " CRISP_VERSION_TEXT L"\n\n" + Loc::Str(IDS_ABOUT_BODY) +
                L"\n\n" +
                Loc::Str(IsOcrAvailable() ? IDS_ABOUT_OCR_YES : IDS_ABOUT_OCR_NO) +
                L"\n\nMIT · © 2026 ShadesOfDeath";
            ::MessageBoxW(nullptr, text.c_str(),
                          Loc::Str(IDS_ABOUT_TITLE).c_str(),
                          MB_OK | MB_ICONINFORMATION);
            break;
        }
        case IDM_EXIT:
            CloseAllPins();
            ::DestroyWindow(m_window);
            break;
        default:
            break;
    }
}

void App::OnHotkey(int id) {
    switch (id) {
        case HOTKEY_REGION:
            StartCapture(CaptureMode::Region);
            break;
        case HOTKEY_FULLSCREEN:
            StartCapture(CaptureMode::FullScreen);
            break;
        case HOTKEY_WINDOW:
            StartCapture(CaptureMode::Window);
            break;
        case HOTKEY_DELAYED:
            StartCapture(CaptureMode::Delayed);
            break;
        default:
            break;
    }
}

}  // namespace crisp
