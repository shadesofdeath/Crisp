// Settings.cpp — Ayarların anlamı: varsayılanlar, doğrulama ve göç.
// Nerede saklandıkları SettingsStore.cpp'dedir.
#include "Settings.h"

#include "ShellIntegration.h"
#include "Util.h"

#include <shlobj.h>

#include <cstdio>

namespace crisp {
namespace {


[[nodiscard]] unsigned ClampUnsigned(unsigned value, unsigned lo, unsigned hi) noexcept {
    return value < lo ? lo : (value > hi ? hi : value);
}

[[nodiscard]] std::wstring PicturesFolder() {
    PWSTR raw = nullptr;
    if (FAILED(::SHGetKnownFolderPath(FOLDERID_Pictures, 0, nullptr, &raw))) {
        return std::wstring();
    }
    std::wstring path{raw};
    ::CoTaskMemFree(raw);
    return path;
}

}  // namespace


// ---------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------

bool HotkeyTypesCharacters(unsigned key) noexcept {
    // İşlev tuşları: F1'den F24'e kesintisiz bir aralık.
    if (key >= VK_F1 && key <= VK_F24) {
        return false;
    }
    // 0xA6–0xB7: tarayıcı, ses ve ortam tuşlarıyla "posta"/"hesap makinesi"
    // gibi başlatıcılar. Hiçbiri metin üretmez ve çoğu klavyede zaten başka
    // bir işi yoktur.
    if (key >= VK_BROWSER_BACK && key <= VK_LAUNCH_APP2) {
        return false;
    }
    switch (key) {
        case VK_SNAPSHOT:   // Print Screen — bu iş için var olan tuş
        case VK_PAUSE:
        case VK_SCROLL:
        // Gezinme ve düzenleme takımı. Hiçbiri karakter üretmez, ama eskiden
        // bu listede yoktular ve bu yüzden "değiştirici ister" sayılıyorlardı.
        // Insert, Home, End ve Page Up/Down, TKL bir klavyede tek başına
        // bağlamak için en uygun tuşlar; yazarken kaybedilen bir şey olmaz.
        case VK_INSERT:
        case VK_HOME:
        case VK_END:
        case VK_PRIOR:
        case VK_NEXT:
        case VK_LEFT:
        case VK_RIGHT:
        case VK_UP:
        case VK_DOWN:
        case VK_APPS:       // içerik menüsü tuşu
        case VK_CANCEL:     // Ctrl+Break
            return false;
        default:
            return true;
    }
}

void Settings::Clamp() {
    delaySeconds = ClampUnsigned(delaySeconds, 1u, 30u);

    // Bilinmeyen ya da boş dil kodu "auto"ya döner. Doğrulama Localization'a
    // sorulmaz: Settings crisp_core'da, Localization uygulama katmanında ve
    // çekirdeğin arayüz katmanına bağımlı olması yön kuralını bozardı.
    // Buradaki kontrol biçimsel; gerçek eşleme Loc tarafında yapılır.
    if (language.empty()) {
        language = L"auto";
    }
    if (theme != L"light" && theme != L"dark") {
        theme = L"system";
    }
    if (saveFormat != L"jpg" && saveFormat != L"webp") {
        saveFormat = L"png";
    }
    saveQuality = ClampUnsigned(saveQuality, 1u, 100u);

    // Üst sınır keyfi değil: geçmiş penceresi açılırken her kaydı çözdüğü
    // için yüzlerce kayıt açılışı fark edilir biçimde yavaşlatırdı. 0 ise
    // geçmiş kapalıdır ve bu geçerli bir seçimdir.
    historyLimit = ClampUnsigned(historyLimit, 0u, 200u);

    // Hiçbir eylem seçili değilse araç yakalar ve sonucu sessizce atar.
    // Kullanıcı bunu isteyerek yapmış olamaz; en beklenen davranışa dönülür.
    if (!after.copyToClipboard && !after.saveToFile && !after.pinToScreen &&
        !after.openEditor) {
        after.copyToClipboard = true;
    }

    dimStrength = ClampUnsigned(dimStrength, 0u, 80u);
    blurStrength = ClampUnsigned(blurStrength, 10u, 400u);
    mosaicStrength = ClampUnsigned(mosaicStrength, 10u, 400u);

    // BOŞ ŞABLON DOSYAYI ADSIZ BIRAKIRDI. Kullanıcı alanı temizlediyse
    // varsayılana dönmek, "Crisp .png" gibi bir dosyaya yazmaktan iyidir.
    if (fileNameFormat.empty()) {
        fileNameFormat = L"Crisp %y-%mo-%d %h-%mi-%s";
    }

    // DEĞİŞTİRİCİSİZ KISAYOL ARTIK SİLİNMİYOR.
    //
    // Burada, dosyadan okunan her kısayol için "Ctrl/Alt/Shift yoksa ve tuş
    // metin üretiyorsa sil" kuralı işliyordu. Kural iyi niyetliydi ve yanlış
    // yerdeydi: kullanıcı ayarlar penceresinde tuşu seçmiş, Tamam demiş, dosya
    // yazılmış — ve bir sonraki açılışta alan boş bulunmuştu. Tek tuşla ekran
    // görüntüsü almak isteyen biri, isteğinin sessizce geri alındığını
    // görüyordu.
    //
    // Bedeli olan bir seçim, bedeli anlatılıp kullanıcıya bırakılır: ayarlar
    // penceresindeki ipucu satırı hangi tuşların yazmaktan alınacağını söyler.
    // Bkz. HotkeyTypesCharacters.
    for (HotkeyBinding& binding : hotkeys) {
        if (static_cast<unsigned>(binding.action) >=
            static_cast<unsigned>(HotkeyAction::Count)) {
            binding.action = HotkeyAction::None;
        }
        // EYLEMSİZ BİR TUŞ SİLİNMEZ, yalnızca kaydedilmez (bkz. Hotkeys::Apply):
        // kombinasyon RegisterHotKey'e hiç verilmediği için başka bir
        // uygulamadan çalınmış olmaz. Eskiden burada siliniyordu ve eylemini
        // henüz seçmemiş kullanıcının tuşu, Tamam'a basar basmaz kayboluyordu.
    }
}

void Settings::Load(const SettingsStore& store) {
    store.ReadString(L"SaveFolder", saveFolder);
    store.ReadString(L"UploadService", uploadService);
    store.ReadString(L"UploadApiKey", uploadApiKey);
    store.ReadString(L"Language", language);
    store.ReadString(L"Theme", theme);
    store.ReadString(L"SaveFormat", saveFormat);
    store.ReadUnsigned(L"SaveQuality", saveQuality);

    store.ReadBool(L"CopyToClipboard", after.copyToClipboard);
    store.ReadBool(L"SaveToFile", after.saveToFile);
    store.ReadBool(L"PinToScreen", after.pinToScreen);
    store.ReadBool(L"OpenEditor", after.openEditor);
    store.ReadBool(L"CopyPathToClipboard", after.copyPathToClipboard);
    store.ReadBool(L"CopyFileToClipboard", after.copyFileToClipboard);
    store.ReadBool(L"RevealInFolder", after.revealInFolder);
    store.ReadBool(L"CopyTextViaOcr", after.copyTextViaOcr);

    store.ReadUnsigned(L"DelaySeconds", delaySeconds);
    store.ReadBool(L"ShowMagnifier", showMagnifier);
    store.ReadBool(L"ShowWindowHighlight", showWindowHighlight);
    store.ReadBool(L"PlayShutterSound", playShutterSound);
    store.ReadBool(L"PrintScreenCapture", printScreenCapture);
    store.ReadBool(L"ShowNotification", showNotification);
    store.ReadUnsigned(L"HistoryLimit", historyLimit);
    store.ReadBool(L"IncludeCursor", includeCursor);
    // KAYIT DEFTERİNDEKİ FİİLDEN okunur, kendi anahtarımızdan değil; gerekçe
    // Settings.h'de.
    shellContextMenu = IsShellMenuRegistered();
    store.ReadUnsigned(L"DimStrength", dimStrength);
    store.ReadString(L"FileNameFormat", fileNameFormat);
    store.ReadString(L"SubFolderFormat", subFolderFormat);
    store.ReadUnsigned(L"BlurStrength", blurStrength);
    store.ReadUnsigned(L"MosaicStrength", mosaicStrength);

    // KOORDİNATLAR İŞARETLİ: sol üstteki monitör birincil değilse negatif
    // olurlar ve unsigned olarak okunup geri çevrilmezse ekranın dışına düşer.
    auto readCoordinate = [&store](const wchar_t* name, LONG& target) {
        unsigned raw = 0;
        if (store.ReadUnsigned(name, raw)) {
            target = static_cast<LONG>(static_cast<int32_t>(raw));
        }
    };
    readCoordinate(L"LastRegionLeft", lastRegion.left);
    readCoordinate(L"LastRegionTop", lastRegion.top);
    readCoordinate(L"LastRegionRight", lastRegion.right);
    readCoordinate(L"LastRegionBottom", lastRegion.bottom);

    // ESKİ ANAHTARLARDAN GÖÇ: 0.3.0'a kadar kısayollar dört sabit ada
    // yazılıyordu ve eylemleri derleme zamanında sabitti. Yeni yuvalar
    // yoksa eskileri okuyup yerlerine koyarız; yükseltme yapan kullanıcı
    // kısayollarını kaybetmemeli.
    auto readLegacy = [&store](const wchar_t* name, HotkeyBinding& target,
                               HotkeyAction action) {
        unsigned packed = 0;
        if (store.ReadUnsigned(name, packed)) {
            target.key = Hotkey::unpack(packed);
            target.action = action;
        }
    };
    readLegacy(L"HotkeyRegion", hotkeys[0], HotkeyAction::Region);
    readLegacy(L"HotkeyFullScreen", hotkeys[1], HotkeyAction::Monitor);
    readLegacy(L"HotkeyWindow", hotkeys[2], HotkeyAction::Window);
    readLegacy(L"HotkeyDelayed", hotkeys[3], HotkeyAction::Delayed);

    for (int i = 0; i < kHotkeySlots; ++i) {
        wchar_t keyName[32];
        wchar_t actionName[32];
        ::swprintf_s(keyName, L"Hotkey%dKey", i + 1);
        ::swprintf_s(actionName, L"Hotkey%dAction", i + 1);
        unsigned packed = 0;
        unsigned action = 0;
        if (store.ReadUnsigned(keyName, packed)) {
            hotkeys[i].key = Hotkey::unpack(packed);
        }
        if (store.ReadUnsigned(actionName, action)) {
            hotkeys[i].action = static_cast<HotkeyAction>(action);
        }
    }

    Clamp();
}

bool Settings::Save(const SettingsStore& store) const {
    if (!store.Prepare()) {
        return false;
    }

    bool ok = true;
    ok = store.WriteString(L"SaveFolder", saveFolder) && ok;
    ok = store.WriteString(L"UploadService", uploadService) && ok;
    ok = store.WriteString(L"UploadApiKey", uploadApiKey) && ok;
    ok = store.WriteString(L"Language", language) && ok;
    ok = store.WriteString(L"Theme", theme) && ok;
    ok = store.WriteString(L"SaveFormat", saveFormat) && ok;
    ok = store.WriteUnsigned(L"SaveQuality", saveQuality) && ok;

    ok = store.WriteBool(L"CopyToClipboard", after.copyToClipboard) && ok;
    ok = store.WriteBool(L"SaveToFile", after.saveToFile) && ok;
    ok = store.WriteBool(L"PinToScreen", after.pinToScreen) && ok;
    ok = store.WriteBool(L"OpenEditor", after.openEditor) && ok;
    ok = store.WriteBool(L"CopyPathToClipboard", after.copyPathToClipboard) && ok;
    ok = store.WriteBool(L"CopyFileToClipboard", after.copyFileToClipboard) && ok;
    ok = store.WriteBool(L"RevealInFolder", after.revealInFolder) && ok;
    ok = store.WriteBool(L"CopyTextViaOcr", after.copyTextViaOcr) && ok;

    ok = store.WriteUnsigned(L"DelaySeconds", delaySeconds) && ok;
    ok = store.WriteBool(L"ShowMagnifier", showMagnifier) && ok;
    ok = store.WriteBool(L"ShowWindowHighlight", showWindowHighlight) && ok;
    ok = store.WriteBool(L"PlayShutterSound", playShutterSound) && ok;
    ok = store.WriteBool(L"PrintScreenCapture", printScreenCapture) && ok;
    ok = store.WriteBool(L"ShowNotification", showNotification) && ok;
    ok = store.WriteUnsigned(L"HistoryLimit", historyLimit) && ok;
    ok = store.WriteBool(L"IncludeCursor", includeCursor) && ok;
    ok = store.WriteUnsigned(L"DimStrength", dimStrength) && ok;
    ok = store.WriteString(L"FileNameFormat", fileNameFormat) && ok;
    ok = store.WriteString(L"SubFolderFormat", subFolderFormat) && ok;
    ok = store.WriteUnsigned(L"BlurStrength", blurStrength) && ok;
    ok = store.WriteUnsigned(L"MosaicStrength", mosaicStrength) && ok;

    auto writeCoordinate = [&store, &ok](const wchar_t* name, LONG value) {
        ok = store.WriteUnsigned(
                 name, static_cast<unsigned>(static_cast<int32_t>(value))) && ok;
    };
    writeCoordinate(L"LastRegionLeft", lastRegion.left);
    writeCoordinate(L"LastRegionTop", lastRegion.top);
    writeCoordinate(L"LastRegionRight", lastRegion.right);
    writeCoordinate(L"LastRegionBottom", lastRegion.bottom);

    for (int i = 0; i < kHotkeySlots; ++i) {
        wchar_t keyName[32];
        wchar_t actionName[32];
        ::swprintf_s(keyName, L"Hotkey%dKey", i + 1);
        ::swprintf_s(actionName, L"Hotkey%dAction", i + 1);
        ok = store.WriteUnsigned(keyName, hotkeys[i].key.packed()) && ok;
        ok = store.WriteUnsigned(actionName,
                                 static_cast<unsigned>(hotkeys[i].action)) && ok;
    }

    store.Flush();
    return ok;
}

std::wstring Settings::EffectiveSaveFolder() const {
    if (!saveFolder.empty()) {
        return saveFolder;
    }
    std::wstring pictures = PicturesFolder();
    if (pictures.empty()) {
        return ModuleDirectory();
    }
    pictures += L"\\Crisp";
    return pictures;
}

}  // namespace crisp
