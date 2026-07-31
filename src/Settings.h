// Settings.h — Ayarlar ve saklandıkları yer.
//
// İki arka uç vardır ve aralarındaki tek fark bir enum alanıdır: sınıf
// hiyerarşisi, sanal fonksiyon ya da fabrika yoktur. İkisi de aynı
// "ad → değer" modelini paylaşır.
//
// Taşınabilir kip: exe'nin YANINDA Crisp.ini varsa etkindir. Dosyanın var
// olması yeter, içi boş olabilir.
#pragma once

#include <string>

#include <windows.h>

namespace crisp {

class SettingsStore {
public:
    enum class Backend {
        Registry,   // HKCU\Software\ShadesOfDeath\Crisp
        IniFile,    // GetPrivateProfileStringW / WritePrivateProfileStringW
    };

    [[nodiscard]] static bool PortableModeActive();
    [[nodiscard]] static std::wstring PortableIniPath();

    [[nodiscard]] static SettingsStore ForApp();
    [[nodiscard]] static SettingsStore ForFile(std::wstring path);

    [[nodiscard]] Backend Kind() const noexcept { return m_backend; }
    [[nodiscard]] const std::wstring& Path() const noexcept { return m_path; }

    // Değer yoksa ya da tipi yanlışsa out'a DOKUNULMAZ; çağıranın varsayılanı
    // korunur. Dönüş "değer bulundu mu" bilgisidir.
    bool ReadBool(const wchar_t* name, bool& out) const;
    bool ReadUnsigned(const wchar_t* name, unsigned& out) const;
    bool ReadString(const wchar_t* name, std::wstring& out) const;

    // Yazmadan önce bir kez çağrılır: anahtarı ya da dosyayı oluşturur.
    [[nodiscard]] bool Prepare() const;

    bool WriteBool(const wchar_t* name, bool value) const;
    bool WriteUnsigned(const wchar_t* name, unsigned value) const;
    bool WriteString(const wchar_t* name, const std::wstring& value) const;

    // .ini kipinde önbelleğe alınmış yazmaları diske indirir.
    void Flush() const;

private:
    SettingsStore(Backend backend, std::wstring path) noexcept
        : m_backend(backend), m_path(std::move(path)) {}

    Backend m_backend = Backend::Registry;
    std::wstring m_path;   // yalnızca IniFile kipinde doludur
};

// Yakalama sonrası eylemler. Birden fazlası aynı anda açık olabilir; hiçbiri
// açık değilse Clamp panoya kopyalamayı geri açar — sessizce hiçbir şey
// yapmayan bir araç, bozuk bir araçtır.
struct AfterCapture {
    bool copyToClipboard = true;
    bool saveToFile = false;
    bool pinToScreen = false;
    bool openEditor = false;
};

struct Hotkey {
    unsigned modifiers = 0;   // MOD_CONTROL | MOD_SHIFT ...
    unsigned key = 0;         // sanal tuş kodu; 0 = atanmamış

    [[nodiscard]] bool assigned() const noexcept { return key != 0; }
    [[nodiscard]] unsigned packed() const noexcept { return (modifiers << 16) | key; }
    static Hotkey unpack(unsigned value) noexcept {
        Hotkey h{};
        h.modifiers = (value >> 16) & 0xFFFFu;
        h.key = value & 0xFFFFu;
        return h;
    }
};

struct Settings {
    std::wstring saveFolder;          // boşsa Resimler\Crisp kullanılır
    // Arayüz dili: "auto" ya da Loc::Languages() tablosundaki bir kod.
    std::wstring language = L"auto";
    // Tema: "system" | "light" | "dark".
    std::wstring theme = L"system";
    AfterCapture after{};

    unsigned delaySeconds = 3;        // gecikmeli yakalama; 1..30
    bool showMagnifier = true;
    bool showWindowHighlight = true;
    bool playShutterSound = false;

    Hotkey hotkeyRegion{MOD_CONTROL | MOD_SHIFT, 'S'};
    Hotkey hotkeyFullScreen{MOD_CONTROL | MOD_SHIFT, 'F'};
    Hotkey hotkeyWindow{MOD_CONTROL | MOD_SHIFT, 'W'};
    Hotkey hotkeyDelayed{MOD_CONTROL | MOD_SHIFT, 'D'};

    // Değerleri geçerli aralığa çeker. Bozuk bir .ini ya da elle kurcalanmış
    // kayıt defteri uygulamayı çökertmemeli, yalnızca makul değerlere dönmeli.
    void Clamp();

    void Load(const SettingsStore& store);
    [[nodiscard]] bool Save(const SettingsStore& store) const;

    // Ayarlanmamışsa varsayılan kayıt klasörü: Resimler\Crisp.
    [[nodiscard]] std::wstring EffectiveSaveFolder() const;
};

}  // namespace crisp
