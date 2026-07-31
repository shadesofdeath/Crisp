// Theme.cpp — bkz. Theme.h.
#include "Theme.h"

#include "Util.h"

#include <dwmapi.h>
#include <uxtheme.h>   // SetWindowTheme — kaydırma çubuğunun görsel stil sınıfı

namespace crisp {
namespace theme {
namespace {

// --- uxtheme ordinal'leri ----------------------------------------------------
// Hiçbiri belgelenmemiştir; yalnızca ordinal numarasıyla dışa aktarılırlar ve
// ada göre aranamazlar. Numaralar Windows 10 1809'dan beri sabit.
constexpr WORD kOrdAllowDarkModeForWindow = 133;
constexpr WORD kOrdAppMode = 135;   // 1809: AllowDarkModeForApp, 1903+: SetPreferredAppMode
constexpr WORD kOrdFlushMenuThemes = 136;
constexpr WORD kOrdRefreshImmersiveColorPolicyState = 104;

// 1903 ve sonrasında ordinal 135'in imzası değişti. Hangi sürümde olduğumuzu
// bilmeden çağırmak, 1809'da "AllowDarkModeForApp(BOOL)" yerine
// "SetPreferredAppMode(int)" çağırmak demek olurdu; ikisi de tek int alır,
// bu yüzden pratikte aynı çağrı iki anlamı da karşılar — ama değer eşlemesi
// farklıdır ve doğru olanı seçmek için sürüm gerekir.
enum class PreferredAppMode : int {
    Default = 0,
    AllowDark = 1,
    ForceDark = 2,
    ForceLight = 3,
};

using FnAllowDarkModeForWindow = BOOL(WINAPI*)(HWND, BOOL);
using FnSetPreferredAppMode = PreferredAppMode(WINAPI*)(PreferredAppMode);
using FnAllowDarkModeForApp = BOOL(WINAPI*)(BOOL);
using FnFlushMenuThemes = void(WINAPI*)();
using FnRefreshImmersiveColorPolicyState = void(WINAPI*)();

// Dosya kapsamlı static: ev kuralı global değişkeni yasaklar, .cpp içindeki
// static'e izin verir.
FnAllowDarkModeForWindow g_allowDarkModeForWindow = nullptr;
FnSetPreferredAppMode g_setPreferredAppMode = nullptr;
FnAllowDarkModeForApp g_allowDarkModeForApp = nullptr;
FnFlushMenuThemes g_flushMenuThemes = nullptr;
FnRefreshImmersiveColorPolicyState g_refreshImmersiveColorPolicyState = nullptr;

ThemeMode g_mode = ThemeMode::System;
bool g_dark = false;
bool g_resolved = false;

// GetProcAddress'in ordinal biçimi: yüksek word'ü sıfır olan "sahte" işaretçi.
// MAKEINTRESOURCEA yerine bu kullanılır; `-A` sonlu sembole dokunmadan aynı iş.
[[nodiscard]] LPCSTR Ordinal(WORD value) noexcept {
    return reinterpret_cast<LPCSTR>(static_cast<ULONG_PTR>(value));
}

[[nodiscard]] bool IsAtLeastBuild(DWORD build) noexcept {
    // RtlGetNtVersionNumbers, uyumluluk katmanına takılmaz; GetVersionEx
    // manifest olmadan 6.2 döndürür ve 1903 kontrolü daima başarısız olurdu.
    using FnRtlGetNtVersionNumbers = void(WINAPI*)(DWORD*, DWORD*, DWORD*);
    const HMODULE ntdll = ::GetModuleHandleW(L"ntdll.dll");
    if (ntdll == nullptr) {
        return false;
    }
    const auto query = reinterpret_cast<FnRtlGetNtVersionNumbers>(
        reinterpret_cast<void*>(::GetProcAddress(ntdll, "RtlGetNtVersionNumbers")));
    if (query == nullptr) {
        return false;
    }

    DWORD major = 0;
    DWORD minor = 0;
    DWORD buildNumber = 0;
    query(&major, &minor, &buildNumber);
    buildNumber &= 0x0FFFFFFF;   // üst bitler bayrak
    return major > 10 || (major == 10 && buildNumber >= build);
}

void ResolveOrdinals() {
    if (g_resolved) {
        return;
    }
    g_resolved = true;

    // uxtheme her GUI sürecinde zaten yüklü; LoadLibrary ile ek referans
    // tutmaya gerek yok.
    const HMODULE ux = ::GetModuleHandleW(L"uxtheme.dll");
    if (ux == nullptr) {
        LogV(L"uxtheme.dll yüklü değil; koyu mod atlanıyor");
        return;
    }

    g_allowDarkModeForWindow = reinterpret_cast<FnAllowDarkModeForWindow>(
        reinterpret_cast<void*>(
            ::GetProcAddress(ux, Ordinal(kOrdAllowDarkModeForWindow))));
    g_flushMenuThemes = reinterpret_cast<FnFlushMenuThemes>(
        reinterpret_cast<void*>(::GetProcAddress(ux, Ordinal(kOrdFlushMenuThemes))));
    g_refreshImmersiveColorPolicyState =
        reinterpret_cast<FnRefreshImmersiveColorPolicyState>(
            reinterpret_cast<void*>(::GetProcAddress(
                ux, Ordinal(kOrdRefreshImmersiveColorPolicyState))));

    const FARPROC appMode = ::GetProcAddress(ux, Ordinal(kOrdAppMode));
    if (appMode != nullptr) {
        // 1903 (build 18362) ordinal 135'i SetPreferredAppMode yaptı.
        if (IsAtLeastBuild(18362)) {
            g_setPreferredAppMode =
                reinterpret_cast<FnSetPreferredAppMode>(reinterpret_cast<void*>(appMode));
        } else {
            g_allowDarkModeForApp =
                reinterpret_cast<FnAllowDarkModeForApp>(reinterpret_cast<void*>(appMode));
        }
    }
}

[[nodiscard]] bool ReadSystemDark() noexcept {
    // AppsUseLightTheme uygulama pencerelerinin temasıdır; menüler ve
    // iletişim kutuları buna uyar. SystemUsesLightTheme görev çubuğunundur ve
    // ikisi bağımsız ayarlanabilir — tepsi simgesi orayı, biz burayı kullanırız.
    DWORD value = 1;
    DWORD size = sizeof(value);
    const LSTATUS status = ::RegGetValueW(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &value, &size);
    if (status != ERROR_SUCCESS) {
        return false;   // anahtar yoksa açık tema (Windows varsayılanı)
    }
    return value == 0;
}

[[nodiscard]] bool ResolveDark(ThemeMode mode) noexcept {
    switch (mode) {
        case ThemeMode::Light:
            return false;
        case ThemeMode::Dark:
            return true;
        case ThemeMode::System:
        default:
            return ReadSystemDark();
    }
}

// Sistemin çizdiği yüzeyleri (menü, iletişim kutusu) güncel kipe getirir.
void ApplyToSystemSurfaces() {
    ResolveOrdinals();

    if (g_setPreferredAppMode != nullptr) {
        // ForceDark/ForceLight yerine AllowDark: AllowDark'ta yüksek kontrast
        // açıkken Windows kendi renklerini korur, Force onları ezerdi.
        g_setPreferredAppMode(g_dark ? PreferredAppMode::ForceDark
                                     : PreferredAppMode::ForceLight);
    } else if (g_allowDarkModeForApp != nullptr) {
        g_allowDarkModeForApp(g_dark ? TRUE : FALSE);
    }

    if (g_refreshImmersiveColorPolicyState != nullptr) {
        g_refreshImmersiveColorPolicyState();
    }
    // FlushMenuThemes OLMADAN açık menü teması önbellekte kalır: kip
    // değiştikten sonra açılan ilk menü hâlâ eski renkte gelir.
    if (g_flushMenuThemes != nullptr) {
        g_flushMenuThemes();
    }
}

constexpr Palette kDark{
    RGB(32, 32, 35),     // surface
    RGB(43, 43, 47),     // surfaceAlt
    RGB(63, 63, 70),     // border
    RGB(244, 244, 245),  // text
    RGB(161, 161, 170),  // textDim
    RGB(10, 132, 255),   // accent
};

constexpr Palette kLight{
    RGB(249, 249, 250),
    RGB(240, 240, 242),
    RGB(205, 205, 210),
    RGB(24, 24, 27),
    RGB(90, 90, 100),
    RGB(0, 103, 192),
};

}  // namespace

void Initialize(ThemeMode mode) {
    g_mode = mode;
    g_dark = ResolveDark(mode);
    ApplyToSystemSurfaces();
}

void SetMode(ThemeMode mode) {
    g_mode = mode;
    const bool dark = ResolveDark(mode);
    if (dark == g_dark) {
        return;
    }
    g_dark = dark;
    ApplyToSystemSurfaces();
}

bool RefreshFromSystem() {
    if (g_mode != ThemeMode::System) {
        return false;   // kullanıcı elle sabitlemiş
    }
    const bool dark = ReadSystemDark();
    if (dark == g_dark) {
        return false;
    }
    g_dark = dark;
    ApplyToSystemSurfaces();
    return true;
}

bool IsDark() noexcept { return g_dark; }

const Palette& Colors() noexcept { return g_dark ? kDark : kLight; }

void ApplyToWindow(HWND window) {
    if (window == nullptr) {
        return;
    }
    ResolveOrdinals();

    if (g_allowDarkModeForWindow != nullptr) {
        g_allowDarkModeForWindow(window, g_dark ? TRUE : FALSE);
    }

    // Başlık çubuğu. DWMWA_USE_IMMERSIVE_DARK_MODE 20 numaradır; 20H1 öncesi
    // yapılarda 19'du ve o sürümlerde 20 başarısız olur — ikisi de denenir.
    const BOOL dark = g_dark ? TRUE : FALSE;
    if (FAILED(::DwmSetWindowAttribute(window, 20, &dark, sizeof(dark)))) {
        (void)::DwmSetWindowAttribute(window, 19, &dark, sizeof(dark));
    }

    // KAYDIRMA ÇUBUĞU pencerenin İSTEMCİ DIŞI alanındadır ve onu Windows çizer;
    // AllowDarkModeForWindow ona yetmez. Görsel stil sınıfını "DarkMode_Explorer"
    // yapmak, kabuğun dosya gezgininde kullandığı koyu çubuğu getirir — aksi
    // hâlde koyu bir pencerenin kenarında bembeyaz bir çubuk kalır.
    (void)::SetWindowTheme(window, g_dark ? L"DarkMode_Explorer" : nullptr,
                           nullptr);
}

ThemeMode ModeFromString(const wchar_t* value) noexcept {
    if (value == nullptr) {
        return ThemeMode::System;
    }
    if (::wcscmp(value, L"light") == 0) {
        return ThemeMode::Light;
    }
    if (::wcscmp(value, L"dark") == 0) {
        return ThemeMode::Dark;
    }
    return ThemeMode::System;
}

const wchar_t* ModeToString(ThemeMode mode) noexcept {
    switch (mode) {
        case ThemeMode::Light:
            return L"light";
        case ThemeMode::Dark:
            return L"dark";
        case ThemeMode::System:
        default:
            return L"system";
    }
}

}  // namespace theme
}  // namespace crisp
