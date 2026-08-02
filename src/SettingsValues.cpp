// SettingsValues.cpp — Ayar değerlerinin denetimlerle alışverişi.
//
// AYRI DOSYA: pencere yordamı ve ömrüyle aynı dosyada SettingsInput.cpp ev
// kuralının 400 satır sınırını aşıyordu (docs §9). Burada pencere mantığı yok;
// yalnızca "hangi değer hangi denetimde durur" sorusunun cevabı var.
#include "SettingsInternal.h"

#include "History.h"
#include "HotkeyEdit.h"
#include "ImageCodec.h"
#include "Localization.h"
#include "MessageWindow.h"
#include "Theme.h"
#include "Upload.h"
#include "Util.h"
#include "resource.h"

#include <shlobj.h>

#include <string>

namespace crisp {
namespace settings_ui {

std::wstring FormatLifetime(unsigned hours) {
    const bool wholeDays = hours >= 24 && hours % 24 == 0;
    const unsigned value = wholeDays ? hours / 24 : hours;
    const std::wstring format =
        Loc::Str(wholeDays ? IDS_SET_UPLOAD_DAYS : IDS_SET_UPLOAD_HOURS);

    // Biçim dizesi ÇEVİRİ DOSYASINDAN geliyor ve içinde tek bir %u olması
    // gerekiyor. Bir çeviride ikinci bir %u belirirse `swprintf_s` yığından
    // çöp okur; o yüzden sayı doğrudan yazılıp yerine konuyor.
    const size_t at = format.find(L"%u");
    if (at == std::wstring::npos) {
        return format;
    }
    wchar_t number[16];
    ::swprintf_s(number, L"%u", value);
    return format.substr(0, at) + number + format.substr(at + 2);
}

void FillUploadServices(HWND box, State& state) {
    size_t count = 0;
    const UploadServiceInfo* services = UploadServices(count);

    for (size_t i = 0; i < count; ++i) {
        const UploadServiceInfo& info = services[i];

        // AYRAÇ, ÇÜNKÜ LİSTE İKİYE BÖLÜNÜYOR. On servisin hesap istemediğini,
        // üçünün istediğini her satırı tek tek okuyarak anlamak gerekiyordu.
        // Ayracın kimliği BOŞ; seçilemez olduğunu `UpdateUploadKeyState` bilir.
        if (info.startsKeyGroup) {
            const std::wstring divider =
                L"──────  " + Loc::Str(IDS_SET_UPLOAD_NEEDS_KEY) + L"  ──────";
            ::SendMessageW(box, CB_ADDSTRING, 0,
                           reinterpret_cast<LPARAM>(divider.c_str()));
            state.uploadServiceIds.emplace_back();
        }

        std::wstring label;
        if (info.service == UploadService::None) {
            label = Loc::Str(IDS_SET_UPLOAD_NONE);
        } else {
            // SÜRE VE BEDEL ADIN YANINDA DURUYOR. Bir servisi seçtikten sonra
            // "bu neden anahtar istiyor" ya da "bağlantım neden öldü" diye
            // sormak zorunda kalmak, listede iki kelimeyle önlenebilir.
            label = info.displayName;
            label += L"  ·  ";
            label += info.lifetimeHours == 0 ? Loc::Str(IDS_SET_UPLOAD_FOREVER)
                                             : FormatLifetime(info.lifetimeHours);
            // "ücretsiz" AYRAÇLA BİRLİKTE DEĞİL, HER SATIRDA. Liste kapalıyken
            // yalnızca seçili satır görünür ve orada grup başlığı diye bir şey
            // yoktur.
            label += L"  ·  ";
            label += info.needsKey ? info.keyLabel : Loc::Str(IDS_SET_UPLOAD_FREE);
        }

        ::SendMessageW(box, CB_ADDSTRING, 0,
                       reinterpret_cast<LPARAM>(label.c_str()));
        state.uploadServiceIds.emplace_back(info.id);
    }
}

void UpdateUploadKeyState(HWND window, const State& state) {
    LRESULT index =
        ::SendDlgItemMessageW(window, kIdUploadService, CB_GETCURSEL, 0, 0);

    // AYRAÇ SEÇİLEMEZ. Bir combo box'ta her satır seçilebilir; ayraç bir satır
    // olmak zorunda ama bir seçim olmamalı. Üstüne gelinirse bir sonrakine —
    // yani anahtar isteyen ilk servise — kayar.
    if (index >= 0 && static_cast<size_t>(index) < state.uploadServiceIds.size() &&
        state.uploadServiceIds[static_cast<size_t>(index)].empty()) {
        ++index;
        ::SendDlgItemMessageW(window, kIdUploadService, CB_SETCURSEL,
                              static_cast<WPARAM>(index), 0);
    }

    bool needsKey = false;
    if (index >= 0 && static_cast<size_t>(index) < state.uploadServiceIds.size()) {
        const UploadService service =
            UploadServiceFromId(state.uploadServiceIds[static_cast<size_t>(index)]);
        needsKey = UploadServiceOf(service).needsKey;
    }
    const HWND key = ::GetDlgItem(window, kIdUploadKey);
    if (key != nullptr) {
        ::EnableWindow(key, needsKey ? TRUE : FALSE);
    }
}

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

// Kod → listedeki dizin. Ayarda kayıtlı biçim artık listede yoksa (WebP
// kodlayıcısı kaldırılmışsa) ilk öğeye düşer; sessizce yanlış bir satırı
// seçili göstermekten iyidir.
[[nodiscard]] int FormatIndex(const State& state, const std::wstring& format) {
    for (size_t i = 0; i < state.formatCodes.size(); ++i) {
        if (state.formatCodes[i] == format) {
            return static_cast<int>(i);
        }
    }
    return 0;
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
    const RECT lastRegion = state.working.lastRegion;
    const bool shellMenu = state.working.shellContextMenu;
    state.working = Settings{};
    state.working.language = language;
    state.working.theme = theme;
    // Son bölge bir AYAR değil, bir DURUM: "varsayılanlara dön" onu
    // sıfırlarsa kullanıcı sebepsiz yere son seçimini kaybeder.
    state.working.lastRegion = lastRegion;
    // KABUK MENÜSÜ DE KORUNUR: "varsayılanlar" düğmesi Windows'un sağ tık
    // menüsünden bir öğe silecek kadar geniş yorumlanmamalı.
    state.working.shellContextMenu = shellMenu;
    LoadIntoControls(window, state);
}

void ClearHistory(HWND window, const State& state) {
    HistoryStore store;
    store.SetFolder(HistoryStore::DefaultFolder());
    const size_t removed = store.Clear();
    LogV(L"Geçmişten %zu kayıt silindi", removed);
    ShowMessage(state.instance, window, Loc::Str(IDS_SET_HISTORY_CLEARED),
                MessageIcon::Information);
}

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
                          static_cast<WPARAM>(FormatIndex(state, s.saveFormat)),
                          0);

    // TANINMAYAN SERVİS ADI 0'A, YANİ "YOK"A DÜŞER. Ayar dosyası ileri bir
    // sürümden gelmiş olabilir; bilmediğimiz bir servise görüntü göndermeye
    // çalışmaktansa hiçbir yere göndermemek doğrusu.
    int uploadIndex = 0;
    for (size_t i = 0; i < state.uploadServiceIds.size(); ++i) {
        if (state.uploadServiceIds[i] == s.uploadService) {
            uploadIndex = static_cast<int>(i);
            break;
        }
    }
    ::SendDlgItemMessageW(window, kIdUploadService, CB_SETCURSEL,
                          static_cast<WPARAM>(uploadIndex), 0);
    ::SetDlgItemTextW(window, kIdUploadKey, s.uploadApiKey.c_str());
    UpdateUploadKeyState(window, state);

    SetNumber(window, kIdQuality, s.saveQuality);
    SetNumber(window, kIdHistoryLimit, s.historyLimit);
    SetNumber(window, kIdDelay, s.delaySeconds);

    SetNumber(window, kIdDimStrength, s.dimStrength);
    SetNumber(window, kIdBlurStrength, s.blurStrength);
    SetNumber(window, kIdMosaicStrength, s.mosaicStrength);
    ::SetDlgItemTextW(window, kIdNameFormat, s.fileNameFormat.c_str());
    ::SetDlgItemTextW(window, kIdSubFolder, s.subFolderFormat.c_str());

    SetCheck(window, kIdShellMenu, s.shellContextMenu);
    SetCheck(window, kIdMagnifier, s.showMagnifier);
    SetCheck(window, kIdHighlight, s.showWindowHighlight);
    SetCheck(window, kIdIncludeCursor, s.includeCursor);
    SetCheck(window, kIdPrintScreen, s.printScreenCapture);
    SetCheck(window, kIdShutter, s.playShutterSound);
    SetCheck(window, kIdAfterCopy, s.after.copyToClipboard);
    SetCheck(window, kIdAfterSave, s.after.saveToFile);
    SetCheck(window, kIdAfterPin, s.after.pinToScreen);
    SetCheck(window, kIdAfterEditor, s.after.openEditor);
    SetCheck(window, kIdAfterCopyPath, s.after.copyPathToClipboard);
    SetCheck(window, kIdAfterCopyFile, s.after.copyFileToClipboard);
    SetCheck(window, kIdAfterReveal, s.after.revealInFolder);
    SetCheck(window, kIdAfterOcr, s.after.copyTextViaOcr);
    SetCheck(window, kIdAfterUpload, s.after.uploadImage);
    SetCheck(window, kIdNotify, s.showNotification);

    for (int slot = 0; slot < kHotkeySlots; ++slot) {
        ::SendDlgItemMessageW(
            window, kIdHotkeyActionFirst + slot, CB_SETCURSEL,
            static_cast<WPARAM>(s.hotkeys[slot].action), 0);
        SetHotkey(window, kIdHotkeyKeyFirst + slot, s.hotkeys[slot].key);
    }
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
    const LRESULT formatIndex =
        ::SendDlgItemMessageW(window, kIdFormat, CB_GETCURSEL, 0, 0);
    if (formatIndex >= 0 &&
        static_cast<size_t>(formatIndex) < state.formatCodes.size()) {
        s.saveFormat = state.formatCodes[static_cast<size_t>(formatIndex)];
    }

    const LRESULT uploadIndex =
        ::SendDlgItemMessageW(window, kIdUploadService, CB_GETCURSEL, 0, 0);
    if (uploadIndex >= 0 &&
        static_cast<size_t>(uploadIndex) < state.uploadServiceIds.size()) {
        s.uploadService = state.uploadServiceIds[static_cast<size_t>(uploadIndex)];
    }
    // ANAHTAR, SERVİS DEĞİŞSE DE SİLİNMEZ. Imgur'dan ImgBB'ye geçip geri dönen
    // kullanıcı anahtarını yeniden yapıştırmak zorunda kalmamalı; hangi
    // anahtarın hangi servise ait olduğu ise zaten kullanıcının bildiği bir şey
    // ve yanlış servise gönderilen bir anahtar reddedilmekten öteye gitmez.
    s.uploadApiKey = GetText(window, kIdUploadKey);

    s.saveFolder = GetText(window, kIdFolder);
    s.saveQuality = GetNumber(window, kIdQuality, s.saveQuality);
    s.historyLimit = GetNumber(window, kIdHistoryLimit, s.historyLimit);
    s.delaySeconds = GetNumber(window, kIdDelay, s.delaySeconds);

    s.dimStrength = GetNumber(window, kIdDimStrength, s.dimStrength);
    s.blurStrength = GetNumber(window, kIdBlurStrength, s.blurStrength);
    s.mosaicStrength = GetNumber(window, kIdMosaicStrength, s.mosaicStrength);
    s.fileNameFormat = GetText(window, kIdNameFormat);
    s.subFolderFormat = GetText(window, kIdSubFolder);

    s.shellContextMenu = GetCheck(window, kIdShellMenu);
    s.showMagnifier = GetCheck(window, kIdMagnifier);
    s.showWindowHighlight = GetCheck(window, kIdHighlight);
    s.includeCursor = GetCheck(window, kIdIncludeCursor);
    s.printScreenCapture = GetCheck(window, kIdPrintScreen);
    s.playShutterSound = GetCheck(window, kIdShutter);
    s.after.copyToClipboard = GetCheck(window, kIdAfterCopy);
    s.after.saveToFile = GetCheck(window, kIdAfterSave);
    s.after.pinToScreen = GetCheck(window, kIdAfterPin);
    s.after.openEditor = GetCheck(window, kIdAfterEditor);
    s.after.copyPathToClipboard = GetCheck(window, kIdAfterCopyPath);
    s.after.copyFileToClipboard = GetCheck(window, kIdAfterCopyFile);
    s.after.revealInFolder = GetCheck(window, kIdAfterReveal);
    s.after.copyTextViaOcr = GetCheck(window, kIdAfterOcr);
    s.after.uploadImage = GetCheck(window, kIdAfterUpload);
    s.showNotification = GetCheck(window, kIdNotify);

    for (int slot = 0; slot < kHotkeySlots; ++slot) {
        const LRESULT index = ::SendDlgItemMessageW(
            window, kIdHotkeyActionFirst + slot, CB_GETCURSEL, 0, 0);
        if (index >= 0 && index < static_cast<LRESULT>(HotkeyAction::Count)) {
            s.hotkeys[slot].action = static_cast<HotkeyAction>(index);
        }
        s.hotkeys[slot].key = GetHotkey(window, kIdHotkeyKeyFirst + slot);
    }

    // SON BÖLGE AYAR PENCERESİNDE DÜZENLENMEZ ama okunan kopyaya taşınmalı:
    // aksi hâlde ayarları açıp kapatmak en son seçilen bölgeyi silerdi.
    s.lastRegion = state.target != nullptr ? state.target->lastRegion : s.lastRegion;
}

}  // namespace settings_ui
}  // namespace crisp
