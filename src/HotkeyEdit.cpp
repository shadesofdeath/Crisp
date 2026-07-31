// HotkeyEdit.cpp — bkz. HotkeyEdit.h.
#include "HotkeyEdit.h"

#include <commctrl.h>

namespace crisp {
namespace {

constexpr UINT_PTR kSubclassId = 1;

[[nodiscard]] bool IsModifierKey(unsigned vk) noexcept {
    return vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL ||
           vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT ||
           vk == VK_MENU || vk == VK_LMENU || vk == VK_RMENU ||
           vk == VK_LWIN || vk == VK_RWIN;
}

[[nodiscard]] unsigned CurrentModifiers() noexcept {
    unsigned modifiers = 0;
    if ((::GetKeyState(VK_CONTROL) & 0x8000) != 0) {
        modifiers |= MOD_CONTROL;
    }
    if ((::GetKeyState(VK_MENU) & 0x8000) != 0) {
        modifiers |= MOD_ALT;
    }
    if ((::GetKeyState(VK_SHIFT) & 0x8000) != 0) {
        modifiers |= MOD_SHIFT;
    }
    return modifiers;
}

// Tuşun kullanıcının klavyesindeki ADI. Sabit bir tablo yazmak, Türkçe F
// klavyede ya da AZERTY'de yanlış harf göstermek olurdu.
[[nodiscard]] std::wstring KeyName(unsigned vk) {
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
        case VK_SNAPSHOT:
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

LRESULT CALLBACK HotkeyEditProc(HWND control, UINT message, WPARAM wParam,
                                LPARAM lParam, UINT_PTR, DWORD_PTR) {
    switch (message) {
        case WM_GETDLGCODE:
            // DLGC_WANTTAB VERİLMEZ: Tab'ı da yutsaydık kullanıcı klavyeyle
            // bu kutudan çıkamazdı.
            return DLGC_WANTALLKEYS;

        case WM_KEYDOWN:
        case WM_SYSKEYDOWN: {
            const unsigned vk = static_cast<unsigned>(wParam);
            if (vk == VK_TAB) {
                break;   // gezinme
            }
            if (IsModifierKey(vk)) {
                return 0;   // yalnız değiştirici tuş kısayol değildir
            }

            const unsigned modifiers = CurrentModifiers();

            // Değiştiricisiz Enter/Escape pencereye ait: biri Tamam'a basar,
            // diğeri kapatır. Kısayol olarak yakalamak, kullanıcıyı kutunun
            // içine hapsederdi.
            if (modifiers == 0 && (vk == VK_RETURN || vk == VK_ESCAPE)) {
                break;
            }
            if (vk == VK_BACK || vk == VK_DELETE) {
                Store(control, Hotkey{});
                return 0;
            }

            Hotkey chosen{};
            chosen.key = vk;
            chosen.modifiers = modifiers;
            Store(control, chosen);
            return 0;
        }

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
