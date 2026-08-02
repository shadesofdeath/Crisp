// App.cpp — Pencere yaşam döngüsü ve mesaj yönlendirme.
// Yakalama akışları AppCapture.cpp, ayar penceresinin sonuçları
// AppSettings.cpp içindedir.
#include "App.h"

#include "AboutWindow.h"
#include "ClipboardImage.h"
#include "Localization.h"
#include "Messages.h"
#include "Ocr.h"
#include "PinWindow.h"
#include "ShellIntegration.h"
#include "Theme.h"
#include "Toast.h"
#include "UploadLog.h"
#include "Util.h"
#include "resource.h"

// Sürüm tek yerde: res/app.rc'deki VERSIONINFO ile elle eşleştirilmesi
// gereken bir ikinci kopya olmasın diye burada tanımlanır.
#define CRISP_VERSION_TEXT L"0.3.0"

#include <commctrl.h>

#include <string>
#include <vector>

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
    controls.dwICC = ICC_STANDARD_CLASSES | ICC_HOTKEY_CLASS;
    ::InitCommonControlsEx(&controls);

    m_settings.Load(SettingsStore::ForApp());
    m_history.SetFolder(HistoryStore::DefaultFolder());

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

    // EXE TAŞINMIŞ OLABİLİR: taşınabilir sürüm başka bir klasöre kopyalandığında
    // kayıtlı komut eski yolu gösterir ve menü öğesi sessizce çalışmaz hâle
    // gelir. Kayıt varsa ve yol tutmuyorsa yeniden yazılır.
    if (m_settings.shellContextMenu && !ShellMenuPathIsCurrent()) {
        ApplyShellMenuSetting();
    }

    // İĞNELER GERİ GELİR. Ekrana iğnelenmiş bir görüntü, bir yeniden başlatma
    // ya da bir güncellemeyle birlikte kayboluyordu — ve hiçbir yere
    // kaydedilmediği için geri getirmenin yolu da yoktu.
    if (const int restored = RestorePins(instance); restored > 0) {
        LogV(L"%d iğne geri yüklendi", restored);
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
                m_tray.SetMenuState(m_settings.HasLastRegion(),
                                    ClipboardHasImage());
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

        // Kendiliğinden başlayan yükleme bitti. Panoyu yazmak ve bildirimi
        // açmak buraya ait; iş parçacığı yalnızca ağı bekliyor.
        case WM_CRISP_UPLOAD_TOAST:
            FinishBackgroundUpload(lParam);
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
            if (wParam == TIMER_COUNTDOWN) {
                if (m_countdown > 1) {
                    --m_countdown;
                    ShowCountdown();
                    return 0;
                }
                ::KillTimer(window, TIMER_COUNTDOWN);
                CloseCaptureToast();
                const HotkeyAction action = m_pendingAction;
                m_pendingAction = HotkeyAction::None;
                m_countdown = 0;
                // BİLDİRİM PENCERESİNİN KAPANMASI BEKLENİR: aynı karede
                // yakalamak, geri sayım penceresini görüntünün içine alırdı.
                ::Sleep(120);
                PerformCapture(action);
                m_busy = false;
                return 0;
            }
            break;
        }

        // Ayarlar penceresi başka bir örnekten açılma isteği gönderebilir;
        // şimdilik yalnızca bölge yakalamayı tetikler.
        // Düzenleyiciye ve iğneye sürüklenen dosyalar buraya gelmez; bu,
        // tepsi simgesinin üstüne bırakılan ya da exe'ye sürüklenen dosyalar
        // için değil, ikinci örneğin gönderdiği "şu dosyayı aç" isteği için.
        case WM_COPYDATA: {
            const auto* data = reinterpret_cast<const COPYDATASTRUCT*>(lParam);
            if (data != nullptr && data->lpData != nullptr && data->cbData > 0) {
                const std::wstring path(static_cast<const wchar_t*>(data->lpData),
                                        data->cbData / sizeof(wchar_t));
                OpenImageFile(path);
            }
            return TRUE;
        }

        case WM_DESTROY:
            // İĞNELER KAPATILMADAN ÖNCE YAZILIR: `CloseAllPins` pencereleri yok
            // ediyor ve onlarla birlikte konumları da gidiyor.
            SaveOpenPins();
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
    // SON BAĞLANTILAR bir komut BLOĞUDUR, tek komut değil: alt menüdeki her
    // satır kendi kimliğini alıyor, dolayısıyla switch'ten önce sınanmalı.
    if (command >= IDM_LINK_FIRST && command <= IDM_LINK_LAST) {
        const std::vector<UploadRecord> records =
            ReadUploadLog(static_cast<size_t>(IDM_LINK_LAST - IDM_LINK_FIRST + 1));
        const size_t index = static_cast<size_t>(command - IDM_LINK_FIRST);
        // Menü açıldığından beri defter değişmiş olabilir — arka planda bir
        // yükleme daha bitmiş olabilir — ve o durumda tıklanan satır kaymıştır.
        if (index < records.size() &&
            CopyTextToClipboard(records[index].link.c_str(), m_window)) {
            const Image none;
            ShowCaptureToast(m_instance, none, Loc::Str(IDS_LINK_COPIED),
                             records[index].link, std::wstring());
        }
        return;
    }

    switch (command) {
        case IDM_LINK_CLEAR:
            (void)ClearUploadLog();
            break;
        case IDM_CAPTURE_REGION:
            RunAction(HotkeyAction::Region);
            break;
        case IDM_CAPTURE_WINDOW:
            RunAction(HotkeyAction::Window);
            break;
        case IDM_CAPTURE_ACTIVE:
            RunAction(HotkeyAction::ActiveWindow);
            break;
        case IDM_CAPTURE_FULLSCREEN:
            RunAction(HotkeyAction::Monitor);
            break;
        case IDM_CAPTURE_ALL:
            RunAction(HotkeyAction::AllMonitors);
            break;
        case IDM_CAPTURE_LAST:
            RunAction(HotkeyAction::LastRegion);
            break;
        case IDM_CAPTURE_DELAYED:
            RunAction(HotkeyAction::Delayed);
            break;
        case IDM_CAPTURE_SCROLL:
            CaptureScrolling();
            break;
        case IDM_DELAYED_WINDOW:
            RunAction(HotkeyAction::DelayedWindow);
            break;
        case IDM_DELAYED_MONITOR:
            RunAction(HotkeyAction::DelayedMonitor);
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
        case IDM_OPEN_CLIPBOARD:
            OpenClipboardImage();
            break;
        case IDM_HISTORY:
            ShowHistory();
            break;
        case IDM_SETTINGS:
            ShowSettings();
            break;
        case IDM_OPEN_FOLDER:
            OpenSaveFolder();
            break;
        case IDM_ABOUT:
            ShowAboutWindow(m_instance);
            break;
        case IDM_EXIT:
            // İĞNELER BURADA KAPATILMAZ. Kapatmayı ve ondan önce diske yazmayı
            // WM_DESTROY yapıyor; burada da kapatmak, kaydedilecek bir şey
            // kalmadan o noktaya varmak demekti — iğneler tam da bu yüzden hiç
            // kaydedilmiyordu.
            CloseCaptureToast();
            ::DestroyWindow(m_window);
            break;
        default:
            break;
    }
}

void App::OnHotkey(int id) {
    if (id == HOTKEY_PRINTSCREEN) {
        RunAction(HotkeyAction::Region);
        return;
    }
    const int slot = id - HOTKEY_SLOT_FIRST;
    if (slot < 0 || slot >= kHotkeySlots) {
        return;
    }
    RunAction(m_settings.hotkeys[slot].action);
}

}  // namespace crisp
