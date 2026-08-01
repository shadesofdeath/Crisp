// HotkeyEdit.cpp — bkz. HotkeyEdit.h.
#include "HotkeyEdit.h"

#include <commctrl.h>

namespace crisp {
namespace {

constexpr UINT_PTR kSubclassId = 1;

// Odaktaki kısayol kutusu ve onun için kurulan alçak seviye klavye kancası.
//
// NEDEN KANCA: bir kombinasyonu başka bir uygulama RegisterHotKey ile aldıysa
// Windows tuşu doğrudan ona gönderir; odaktaki kutumuz WM_KEYDOWN'ı HİÇ
// görmez. Kullanıcı tuşa basar, kutuda bir şey belirmez — eskisini Backspace
// ile silmişse kutu boş kalır ve Tamam'a basınca kısayol yok olur. Alçak
// seviye kanca kısayol dağıtımından ÖNCE çalışır ve tuşu yutabilir; böylece
// Crisp'in kendi kısayolları da dahil her kombinasyon yazılabilir.
//
// TEK KUTU ODAKTA OLABİLİR, bu yüzden tek bir kanca yeter.
HHOOK g_hook = nullptr;
HWND g_hookTarget = nullptr;

[[nodiscard]] bool IsModifierKey(unsigned vk) noexcept {
    return vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL ||
           vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT ||
           vk == VK_MENU || vk == VK_LMENU || vk == VK_RMENU ||
           vk == VK_LWIN || vk == VK_RWIN;
}

// GetKeyState DEĞİL GetAsyncKeyState: GetKeyState iş parçacığının SIRADAN
// ALDIĞI son mesaja göre cevap verir. Alçak seviye kancada tuş henüz hiçbir
// sıraya girmemiş olur ve değiştiriciler "basılı değil" görünürdü — Ctrl+Shift+S
// düpedüz "S" olarak yakalanırdı.
[[nodiscard]] unsigned CurrentModifiers() noexcept {
    unsigned modifiers = 0;
    if ((::GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0) {
        modifiers |= MOD_CONTROL;
    }
    if ((::GetAsyncKeyState(VK_MENU) & 0x8000) != 0) {
        modifiers |= MOD_ALT;
    }
    if ((::GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0) {
        modifiers |= MOD_SHIFT;
    }
    return modifiers;
}

// Windows'un ADINI VEREMEDİĞİ ya da YANLIŞ VERDİĞİ tuşlar.
//
// GetKeyNameText tarama koduna bakar ve bu tuşlarda yanılır: Print Screen için
// "Sys Req", ortam tuşları için o tarama kodunda oturan HARFİ ("G", "B"),
// Pause ve F13+ için hiçbir şey döndürür. Değiştiricisiz atanabilen tuşlar
// tam olarak bunlar olduğu için (bkz. HotkeyNeedsModifier) adları da artık
// göz önünde: "G" ya da "0xB3" yazan bir kutu, kullanıcıya hangi tuşa
// bastığını söylemez.
//
// BURADA TABLO DOĞRU: harflerin aksine bu tuşlar klavye düzeninden bağımsızdır
// ve tuş kapağında da böyle yazar.
[[nodiscard]] const wchar_t* FixedKeyName(unsigned vk) noexcept {
    switch (vk) {
        case VK_SNAPSHOT:            return L"Print Screen";
        case VK_PAUSE:               return L"Pause";
        case VK_BROWSER_BACK:        return L"Browser Back";
        case VK_BROWSER_FORWARD:     return L"Browser Forward";
        case VK_BROWSER_REFRESH:     return L"Browser Refresh";
        case VK_BROWSER_STOP:        return L"Browser Stop";
        case VK_BROWSER_SEARCH:      return L"Browser Search";
        case VK_BROWSER_FAVORITES:   return L"Browser Favourites";
        case VK_BROWSER_HOME:        return L"Browser Home";
        case VK_VOLUME_MUTE:         return L"Mute";
        case VK_VOLUME_DOWN:         return L"Volume Down";
        case VK_VOLUME_UP:           return L"Volume Up";
        case VK_MEDIA_NEXT_TRACK:    return L"Next Track";
        case VK_MEDIA_PREV_TRACK:    return L"Previous Track";
        case VK_MEDIA_STOP:          return L"Media Stop";
        case VK_MEDIA_PLAY_PAUSE:    return L"Play / Pause";
        case VK_LAUNCH_MAIL:         return L"Mail";
        case VK_LAUNCH_MEDIA_SELECT: return L"Media";
        case VK_LAUNCH_APP1:         return L"App 1";
        case VK_LAUNCH_APP2:         return L"App 2";
        default:                     return nullptr;
    }
}

// Tuşun kullanıcının klavyesindeki ADI. Harfler için sabit bir tablo yazmak,
// Türkçe F klavyede ya da AZERTY'de yanlış harf göstermek olurdu; düzenden
// bağımsız tuşlar yukarıdaki tablodan gelir.
[[nodiscard]] std::wstring KeyName(unsigned vk) {
    if (const wchar_t* fixed = FixedKeyName(vk); fixed != nullptr) {
        return std::wstring(fixed);
    }
    // F13–F24 hiçbir tarama koduna oturmaz ve adsız kalırdı; numarası zaten
    // adının kendisi.
    if (vk >= VK_F13 && vk <= VK_F24) {
        wchar_t generated[8];
        ::swprintf_s(generated, L"F%u", vk - VK_F1 + 1u);
        return std::wstring(generated);
    }

    UINT scan = ::MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);

    // UZATILMIŞ TUŞ BİTİ: onsuz Home/End/ok tuşları sayısal tuş takımının
    // adlarıyla ("Num 7") görünür.
    switch (vk) {
        case VK_INSERT:
        case VK_DELETE:
        case VK_HOME:
        case VK_END:
        case VK_PRIOR:
        case VK_NEXT:
        case VK_LEFT:
        case VK_RIGHT:
        case VK_UP:
        case VK_DOWN:
        case VK_NUMLOCK:
        case VK_DIVIDE:
            scan |= 0x100u;
            break;
        default:
            break;
    }

    wchar_t name[64] = L"";
    const LONG parameter = static_cast<LONG>(scan << 16);
    if (::GetKeyNameTextW(parameter, name, 64) == 0) {
        return std::wstring();
    }
    return std::wstring(name);
}

void Store(HWND control, const Hotkey& hotkey) {
    ::SetWindowLongPtrW(control, GWLP_USERDATA,
                        static_cast<LONG_PTR>(hotkey.packed()));
    ::SetWindowTextW(control, HotkeyText(hotkey).c_str());
}

// Basılan tuşu kutuya yazar. Dönüş: tuş TÜKETİLDİ mi — kanca onu yutsun,
// pencere yordamı da başkasına devretmesin.
[[nodiscard]] bool Capture(HWND control, unsigned vk) {
    if (vk == VK_TAB) {
        return false;   // gezinme; kutudan klavyeyle çıkılabilmeli
    }
    if (IsModifierKey(vk)) {
        return false;   // yalnız değiştirici tuş kısayol değildir
    }

    const unsigned modifiers = CurrentModifiers();

    // Değiştiricisiz Enter/Escape pencereye ait: biri Tamam'a basar, diğeri
    // kapatır. Kısayol olarak yakalamak, kullanıcıyı kutunun içine hapsederdi.
    if (modifiers == 0 && (vk == VK_RETURN || vk == VK_ESCAPE)) {
        return false;
    }
    if (vk == VK_BACK || vk == VK_DELETE) {
        Store(control, Hotkey{});
        return true;
    }

    // KABUL EDİLMEYECEK BİR BİLEŞİM KUTUYA HİÇ YAZILMAZ. Eskiden yazılırdı ve
    // Settings::Clamp onu Tamam'da sessizce siliyordu: kullanıcı geçerli
    // gördüğü tuşu atıyor, alan boşalıyordu. Uyarı sesi kuralı olay yerinde
    // söyler; kutuda duran eski değer de bozulmadan kalır.
    if (modifiers == 0 && HotkeyNeedsModifier(vk)) {
        ::MessageBeep(MB_ICONWARNING);
        return true;
    }

    Hotkey chosen{};
    chosen.key = vk;
    chosen.modifiers = modifiers;
    Store(control, chosen);
    return true;
}

LRESULT CALLBACK LowLevelKeys(int code, WPARAM wParam, LPARAM lParam) {
    if (code == HC_ACTION && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
        const auto* event = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lParam);
        // GetFocus İŞ PARÇACIĞI BAŞINADIR ve kanca bizim iş parçacığımızda
        // çağrılır: pencere arka plana düştüğünde nullptr döner ve tuşları
        // yutmayı bırakırız. Ayarlar penceresini açık unutmanın bedeli, bütün
        // sistemin klavyesini çalmak olamaz.
        if (event != nullptr && g_hookTarget != nullptr &&
            ::GetFocus() == g_hookTarget &&
            Capture(g_hookTarget, event->vkCode)) {
            return 1;
        }
    }
    return ::CallNextHookEx(nullptr, code, wParam, lParam);
}

void InstallHook(HWND control) {
    g_hookTarget = control;
    if (g_hook == nullptr) {
        g_hook = ::SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelKeys,
                                     ::GetModuleHandleW(nullptr), 0);
    }
}

void RemoveHook(HWND control) {
    if (g_hookTarget != control) {
        return;   // odak zaten başka bir kutuya geçmiş
    }
    g_hookTarget = nullptr;
    if (g_hook != nullptr) {
        ::UnhookWindowsHookEx(g_hook);
        g_hook = nullptr;
    }
}

LRESULT CALLBACK HotkeyEditProc(HWND control, UINT message, WPARAM wParam,
                                LPARAM lParam, UINT_PTR, DWORD_PTR) {
    switch (message) {
        case WM_GETDLGCODE:
            // DLGC_WANTTAB VERİLMEZ: Tab'ı da yutsaydık kullanıcı klavyeyle
            // bu kutudan çıkamazdı.
            return DLGC_WANTALLKEYS;

        case WM_SETFOCUS:
            InstallHook(control);
            break;

        case WM_KILLFOCUS:
            RemoveHook(control);
            break;

        // KANCA KURULAMADIYSA (ilke kısıtları, kanca zaman aşımı) tuşlar
        // buraya düşer: sıradan yol da çalışır durumda kalmalı.
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            if (Capture(control, static_cast<unsigned>(wParam))) {
                return 0;
            }
            break;

        // Metin kutusuna harf yazılmasın; içeriği yalnızca biz koyarız.
        case WM_CHAR:
        case WM_SYSCHAR:
        case WM_PASTE:
        case WM_CUT:
        case WM_CLEAR:
            return 0;

        case WM_CONTEXTMENU:
            return 0;

        case WM_LBUTTONDOWN:
        case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDOWN:
            ::SetFocus(control);
            return 0;

        case WM_NCDESTROY:
            // KANCA HER HÂLÜKÂRDA KALDIRILIR: pencere odaktayken kapatılırsa
            // WM_KILLFOCUS gelmeyebilir ve sahibi ölmüş bir kanca, sistemdeki
            // her tuş vuruşunu boşuna yavaşlatırdı.
            RemoveHook(control);
            ::RemoveWindowSubclass(control, HotkeyEditProc, kSubclassId);
            break;

        default:
            break;
    }
    return ::DefSubclassProc(control, message, wParam, lParam);
}

}  // namespace

std::wstring HotkeyText(const Hotkey& hotkey) {
    if (!hotkey.assigned()) {
        return std::wstring(L"—");
    }
    std::wstring text;
    if ((hotkey.modifiers & MOD_CONTROL) != 0) {
        text += L"Ctrl + ";
    }
    if ((hotkey.modifiers & MOD_ALT) != 0) {
        text += L"Alt + ";
    }
    if ((hotkey.modifiers & MOD_SHIFT) != 0) {
        text += L"Shift + ";
    }
    const std::wstring name = KeyName(hotkey.key);
    if (name.empty()) {
        wchar_t fallback[16];
        ::swprintf_s(fallback, L"0x%02X", hotkey.key);
        text += fallback;
    } else {
        text += name;
    }
    return text;
}

void MakeHotkeyEdit(HWND control) {
    if (control == nullptr) {
        return;
    }
    (void)::SetWindowSubclass(control, HotkeyEditProc, kSubclassId, 0);
}

void SetHotkeyValue(HWND control, const Hotkey& hotkey) {
    if (control != nullptr) {
        Store(control, hotkey);
    }
}

Hotkey GetHotkeyValue(HWND control) noexcept {
    if (control == nullptr) {
        return Hotkey{};
    }
    const LONG_PTR packed = ::GetWindowLongPtrW(control, GWLP_USERDATA);
    return Hotkey::unpack(static_cast<unsigned>(packed));
}

}  // namespace crisp
