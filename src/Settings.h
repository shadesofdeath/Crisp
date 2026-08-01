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
//
// SON DÖRT ALAN ÇIKTIYI ÜRETMEZ, onunla bir şey yapar: dolayısıyla "hiçbiri
// seçili değil" denetimine girmezler — yalnızca "dosya yolunu kopyala" açık
// olan bir kurulum, kopyalanacak bir yol üretmez.
struct AfterCapture {
    bool copyToClipboard = true;
    bool saveToFile = false;
    bool pinToScreen = false;
    bool openEditor = false;

    bool copyPathToClipboard = false;   // kaydedilen dosyanın tam yolu
    bool copyFileToClipboard = false;   // dosyanın kendisi (CF_HDROP)
    bool revealInFolder = false;        // Explorer'da göster
    bool copyTextViaOcr = false;        // görüntüdeki metni tanı ve panoya koy
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

// Bu tuş, kısayol olabilmek için Ctrl/Alt/Shift İSTER Mİ?
//
// HARFLER VE RAKAMLAR İSTER: değiştiricisiz bir harfi global kısayol yapmak, o
// harfi sistemdeki her metin kutusundan çalmak demektir.
//
// F1–F24, PRINT SCREEN, PAUSE, SCROLL LOCK VE ORTAM TUŞLARI İSTEMEZ: hiçbiri
// metin üretmez ve tam olarak bu iş için vardır. Kuralı onlara da uygulamak,
// sayısal takımı olmayan (TKL) bir klavyede kullanıcının elinde kalan yedek
// tuşları yasaklıyordu — üstelik kısayol kutusu bu tuşları kabul edip
// gösterdiği, Clamp ise Tamam'da sessizce sildiği için alan boşalıyordu.
[[nodiscard]] bool HotkeyNeedsModifier(unsigned key) noexcept;

// Bir kısayolun ne yaptığı.
//
// ESKİDEN DERLEME ZAMANINDA SABİTTİ: HOTKEY_REGION her zaman bölge seçerdi ve
// OCR'a, renk seçiciye, geçmişe kısayol atamanın yolu yoktu. Eylemi ayrı bir
// alan yapmak, aynı listeyi hem kısayollar hem tepsi menüsü için tek yerde
// tutmayı da sağlıyor.
enum class HotkeyAction : unsigned {
    None = 0,
    Region,
    Window,          // imlecin altındaki pencere (kaplama ile)
    ActiveWindow,    // ön plandaki pencere, kaplama açmadan
    Monitor,         // imlecin bulunduğu monitör
    AllMonitors,     // sanal masaüstünün tamamı
    LastRegion,      // en son seçilen dikdörtgen
    Delayed,
    SelectText,
    RegionText,
    PickColor,
    History,
    Count,
};

struct HotkeyBinding {
    Hotkey key;
    HotkeyAction action = HotkeyAction::None;
};

// Kısayol yuvası sayısı. Dördü varsayılan atamalı gelir, ikisi boştur;
// sınırsız bir liste ayarlar penceresini kaydırılabilir yapmayı gerektirirdi
// ve altı yuva, istenen her eylemi karşılamaya fazlasıyla yetiyor.
inline constexpr int kHotkeySlots = 6;

struct Settings {
    std::wstring saveFolder;          // boşsa Resimler\Crisp kullanılır
    // Arayüz dili: "auto" ya da Loc::Languages() tablosundaki bir kod.
    std::wstring language = L"auto";
    // Tema: "system" | "light" | "dark".
    std::wstring theme = L"system";
    // Kayıt biçimi: "png" | "jpg" | "webp".
    std::wstring saveFormat = L"png";
    unsigned saveQuality = 90;        // JPEG/WebP; 1..100

    AfterCapture after{};

    unsigned delaySeconds = 3;        // gecikmeli yakalama; 1..30
    bool showMagnifier = true;
    bool showWindowHighlight = true;
    bool playShutterSound = false;
    // Print Screen tuşu bölge yakalamayı açsın mı?
    bool printScreenCapture = true;
    // Yakalamadan sonra köşede kısa bir bildirim gösterilsin mi?
    bool showNotification = true;
    // Geçmişte saklanacak yakalama sayısı; 0 = geçmiş kapalı.
    unsigned historyLimit = 24;

    // Explorer'ın sağ tık menüsünde "Crisp ile düzenle" görünsün mü?
    //
    // BU ALAN DİSKE YAZILMAZ. Doğruluk kaynağı kayıt defterindeki fiilin
    // KENDİSİDİR: ayrı bir kopya tutmak, kullanıcı kaydı elle sildiğinde ya da
    // exe taşındığında ikisinin birbirini yalanlaması demek olurdu. Load onu
    // fiilin varlığından okur, çağıran değişikliği uygulayarak kaydeder.
    bool shellContextMenu = false;

    // Fare imleci de yakalansın mı? Varsayılan KAPALI: çoğu ekran görüntüsünde
    // imleç gürültüdür ve isteyen açar.
    bool includeCursor = false;

    // Kaplamanın seçim dışını karartma gücü; 0..80 yüzde.
    unsigned dimStrength = 40;

    // Dosya adı ve alt klasör şablonları (bkz. NameFormat.h). Alt klasör boşsa
    // dosyalar doğrudan kayıt klasörüne yazılır.
    std::wstring fileNameFormat = L"Crisp %y-%mo-%d %h-%mi-%s";
    std::wstring subFolderFormat;

    // Bulanıklık ve mozaik şiddeti; 100 = otomatik (alana göre ölçeklenen
    // varsayılan), 10..400 arası kullanıcı çarpanı.
    unsigned blurStrength = 100;
    unsigned mosaicStrength = 100;

    // "Son bölge" yakalamasının hedefi. assigned() yerine boş dikdörtgen
    // denetimi kullanılır: hiç bölge seçilmemişken eylem sessizce hiçbir şey
    // yapmalı, hata vermemeli.
    RECT lastRegion{};

    HotkeyBinding hotkeys[kHotkeySlots] = {
        {{MOD_CONTROL | MOD_SHIFT, 'S'}, HotkeyAction::Region},
        {{MOD_CONTROL | MOD_SHIFT, 'F'}, HotkeyAction::Monitor},
        {{MOD_CONTROL | MOD_SHIFT, 'W'}, HotkeyAction::Window},
        {{MOD_CONTROL | MOD_SHIFT, 'D'}, HotkeyAction::Delayed},
        {{}, HotkeyAction::None},
        {{}, HotkeyAction::None},
    };

    // Değerleri geçerli aralığa çeker. Bozuk bir .ini ya da elle kurcalanmış
    // kayıt defteri uygulamayı çökertmemeli, yalnızca makul değerlere dönmeli.
    void Clamp();

    void Load(const SettingsStore& store);
    [[nodiscard]] bool Save(const SettingsStore& store) const;

    // Ayarlanmamışsa varsayılan kayıt klasörü: Resimler\Crisp.
    [[nodiscard]] std::wstring EffectiveSaveFolder() const;

    [[nodiscard]] bool HasLastRegion() const noexcept {
        return lastRegion.right > lastRegion.left &&
               lastRegion.bottom > lastRegion.top;
    }
};

}  // namespace crisp
