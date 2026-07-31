// TrayIcon.cpp — bkz. TrayIcon.h.
#include "TrayIcon.h"

#include "Localization.h"
#include "Messages.h"
#include "Util.h"
#include "resource.h"

#include <string>

// WIN32_LEAN_AND_MEAN nedeniyle ikisi de <windows.h> ile gelmez:
//   shellapi.h — NOTIFYICONDATAW, Shell_NotifyIconW
//   commctrl.h — LoadIconWithScaleDown (comctl32'ye bağlanır)
#include <commctrl.h>
#include <shellapi.h>

namespace crisp {
namespace {

constexpr UINT kIconId = 1;

// Görev çubuğu açık temada mı? SystemUsesLightTheme, kabuğun (görev çubuğu,
// başlat) temasını verir; AppsUseLightTheme uygulama pencerelerinin temasıdır
// ve ikisi bağımsız olarak ayarlanabilir. Tepsi simgesi görev çubuğunun
// üstünde durduğu için doğru olan SystemUsesLightTheme'dir.
[[nodiscard]] bool TaskbarUsesLightTheme() {
    DWORD value = 0;
    DWORD size = sizeof(value);
    const LSTATUS status = ::RegGetValueW(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        L"SystemUsesLightTheme", RRF_RT_REG_DWORD, nullptr, &value, &size);
    if (status != ERROR_SUCCESS) {
        return false;   // anahtar yoksa koyu varsayılır (Windows varsayılanı)
    }
    return value != 0;
}

// Tepsi simgesi için doğru piksel boyutu SM_CXSMICON'dur; sabit 16 vermek
// %150 ölçekli bir ekranda bulanık simge demektir.
[[nodiscard]] HICON LoadTrayIcon(HINSTANCE instance, bool lightTaskbar) {
    // Açık görev çubuğunda KOYU glif okunur, koyu görev çubuğunda BEYAZ.
    const int resourceId = lightTaskbar ? IDI_TRAY_LIGHT : IDI_TRAY_DARK;
    const int size = ::GetSystemMetrics(SM_CXSMICON);

    HICON icon = nullptr;
    const HRESULT hr = ::LoadIconWithScaleDown(
        instance, MAKEINTRESOURCEW(resourceId), size, size, &icon);
    if (SUCCEEDED(hr) && icon != nullptr) {
        return icon;
    }

    // LoadIconWithScaleDown yoksa/başarısızsa klasik yol.
    return static_cast<HICON>(::LoadImageW(instance, MAKEINTRESOURCEW(resourceId),
                                           IMAGE_ICON, size, size, LR_DEFAULTCOLOR));
}

void FillNotifyData(NOTIFYICONDATAW& data, HWND owner) {
    data = NOTIFYICONDATAW{};
    data.cbSize = sizeof(data);
    data.hWnd = owner;
    data.uID = kIconId;
}

}  // namespace

UINT TrayIcon::TaskbarCreatedMessage() noexcept {
    static const UINT message = ::RegisterWindowMessageW(L"TaskbarCreated");
    return message;
}

bool TrayIcon::Add(HWND owner, HINSTANCE instance) {
    m_owner = owner;
    m_instance = instance;
    m_darkTaskbar = !TaskbarUsesLightTheme();
    m_themeKnown = true;

    NOTIFYICONDATAW data{};
    FillNotifyData(data, owner);
    data.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
    data.uCallbackMessage = WM_CRISP_TRAY;
    data.hIcon = LoadTrayIcon(instance, !m_darkTaskbar);
    // szTip 128 karakterlik sabit dizidir; uzun bir çeviri kırpılır, taşmaz.
    const std::wstring tooltip = Loc::Str(IDS_TRAY_TOOLTIP);
    ::wcsncpy_s(data.szTip, tooltip.c_str(), _TRUNCATE);

    if (data.hIcon == nullptr) {
        LogV(L"Tepsi simgesi yüklenemedi");
        return false;
    }

    m_added = ::Shell_NotifyIconW(NIM_ADD, &data) != FALSE;
    if (m_added) {
        // NOTIFYICON_VERSION_4: geri bildirim mesajlarında imleç konumu
        // lParam yerine wParam'da gelir ve çok monitörde doğrudur.
        data.uVersion = NOTIFYICON_VERSION_4;
        ::Shell_NotifyIconW(NIM_SETVERSION, &data);
    } else {
        LogV(L"Shell_NotifyIcon(NIM_ADD) başarısız");
    }

    ::DestroyIcon(data.hIcon);
    return m_added;
}

void TrayIcon::Remove() noexcept {
    if (!m_added) {
        return;
    }
    NOTIFYICONDATAW data{};
    FillNotifyData(data, m_owner);
    ::Shell_NotifyIconW(NIM_DELETE, &data);
    m_added = false;
}

void TrayIcon::UpdateIcon() {
    if (!m_added) {
        return;
    }

    NOTIFYICONDATAW data{};
    FillNotifyData(data, m_owner);
    data.uFlags = NIF_ICON;
    data.hIcon = LoadTrayIcon(m_instance, !m_darkTaskbar);
    if (data.hIcon == nullptr) {
        return;
    }

    ::Shell_NotifyIconW(NIM_MODIFY, &data);
    ::DestroyIcon(data.hIcon);
}

void TrayIcon::RefreshTheme() {
    const bool dark = !TaskbarUsesLightTheme();
    if (m_themeKnown && dark == m_darkTaskbar) {
        return;   // değişiklik yok; kabuğu boşuna meşgul etme
    }
    m_darkTaskbar = dark;
    m_themeKnown = true;
    UpdateIcon();
}

void TrayIcon::Restore() {
    m_added = false;   // Explorer çöktüğünde eski kayıt geçersiz
    // Başarısızlıkta yapacak bir şey yok: Explorer hâlâ ayaktaysa bir sonraki
    // TaskbarCreated yayınında yeniden denenecek.
    if (!Add(m_owner, m_instance)) {
        LogV(L"Tepsi simgesi Explorer yeniden başladıktan sonra eklenemedi");
    }
}

int TrayIcon::ShowMenu(HWND owner) {
    const HMENU menu = ::CreatePopupMenu();
    if (menu == nullptr) {
        return 0;
    }

    // Metinler her açılışta yeniden okunur: dil ayarı değiştiğinde menünün
    // eski dilde kalmaması için önbelleğe alınmazlar.
    auto add = [menu](UINT command, UINT textId, UINT acceleratorId) {
        const std::wstring text = Loc::MenuText(textId, acceleratorId);
        ::AppendMenuW(menu, MF_STRING, command, text.c_str());
    };
    auto separator = [menu]() { ::AppendMenuW(menu, MF_SEPARATOR, 0, nullptr); };

    add(IDM_CAPTURE_REGION, IDS_MENU_REGION, IDS_ACCEL_REGION);
    add(IDM_CAPTURE_WINDOW, IDS_MENU_WINDOW, IDS_ACCEL_WINDOW);
    add(IDM_CAPTURE_FULLSCREEN, IDS_MENU_FULLSCREEN, IDS_ACCEL_FULLSCREEN);
    add(IDM_CAPTURE_DELAYED, IDS_MENU_DELAYED, IDS_ACCEL_DELAYED);
    separator();
    add(IDM_SELECT_TEXT, IDS_MENU_SELECT_TEXT, 0);
    add(IDM_CAPTURE_OCR, IDS_MENU_REGION_TEXT, 0);
    add(IDM_PICK_COLOR, IDS_MENU_PICK_COLOR, 0);
    separator();
    add(IDM_HISTORY, IDS_MENU_HISTORY, 0);
    add(IDM_OPEN_FOLDER, IDS_MENU_OPEN_FOLDER, 0);
    separator();
    add(IDM_ABOUT, IDS_MENU_ABOUT, 0);
    add(IDM_EXIT, IDS_MENU_EXIT, 0);

    POINT cursor{};
    ::GetCursorPos(&cursor);

    // SetForegroundWindow ŞART: onsuz menü, kullanıcı başka bir yere tıkladığında
    // kapanmaz ve ekranda asılı kalır. Belgelenmiş bir kabuk gerekliliğidir.
    ::SetForegroundWindow(owner);

    const int command = ::TrackPopupMenuEx(
        menu, TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY, cursor.x, cursor.y,
        owner, nullptr);

    ::DestroyMenu(menu);
    return command;
}

}  // namespace crisp
