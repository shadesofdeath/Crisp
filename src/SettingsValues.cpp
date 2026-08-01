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
#include "Util.h"
#include "resource.h"

#include <shlobj.h>

#include <string>

namespace crisp {
namespace settings_ui {

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
