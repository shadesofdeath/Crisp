// SettingsWindow.cpp — Ayarlar penceresinin denetimleri, yerleşimi ve çizimi.
// Mesajlar ve pencere ömrü SettingsInput.cpp'dedir.
#include "SettingsInternal.h"

#include "Geometry.h"
#include "HotkeyEdit.h"
#include "ImageCodec.h"
#include "Localization.h"
#include "Theme.h"
#include "Util.h"
#include "resource.h"

#include <commctrl.h>
#include <uxtheme.h>   // SetWindowTheme — koyu temada standart denetimler

#include <string>

namespace crisp {
namespace settings_ui {
namespace {

// Denetimleri sırayla aşağı doğru yerleştiren küçük bir imleç.
//
// MUTLAK KOORDİNAT YAZMAK YERİNE: her denetime elle y vermek, araya bir satır
// eklendiğinde altındaki her sayının güncellenmesi demekti ve bir tanesini
// unutmak denetimleri üst üste bindirirdi.
class Cursor {
public:
    Cursor(HWND parent, State& state, int left, int top, int width) noexcept
        : m_parent(parent), m_state(state), m_left(left), m_y(top),
          m_width(width) {}

    [[nodiscard]] int y() const noexcept { return m_y; }
    void Advance(int amount) noexcept { m_y += amount; }

    void Group(const wchar_t* text) {
        if (!m_state.groups.empty()) {
            m_y += Scale(kGroupGap, m_state.dpi);
        }
        GroupTitle title;
        title.text = text;
        title.bounds =
            RECT{m_left, m_y, m_left + m_width, m_y + Scale(22, m_state.dpi)};
        m_state.groups.push_back(std::move(title));
        m_y += Scale(28, m_state.dpi);
    }

    HWND Check(int id, const wchar_t* text) {
        const HWND control = ::CreateWindowExW(
            0, L"BUTTON", text, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
            m_left, m_y, m_width, Scale(24, m_state.dpi), m_parent,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr, nullptr);
        m_y += Scale(26, m_state.dpi);
        return control;
    }

    // Kısayol satırı: solda EYLEM açılır kutusu, sağda tuş kutusu.
    //
    // ETİKET YOK ÇÜNKÜ EYLEM ETİKETTİR: "Bölge seç — Ctrl+Shift+S" satırında
    // ayrıca "Kısayol 1" yazmak, kullanıcının umursamadığı bir yuva numarasını
    // öne çıkarırdı.
    void HotkeyRow(int slot) {
        const int actionWidth = Scale(kActionWidth, m_state.dpi);
        const int height = Scale(24, m_state.dpi);
        const HWND action = ::CreateWindowExW(
            0, L"COMBOBOX", L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
            m_left, m_y, actionWidth, Scale(220, m_state.dpi), m_parent,
            reinterpret_cast<HMENU>(
                static_cast<INT_PTR>(kIdHotkeyActionFirst + slot)),
            nullptr, nullptr);
        for (unsigned i = 0; i < static_cast<unsigned>(HotkeyAction::Count); ++i) {
            const std::wstring text =
                Loc::Str(HotkeyActionLabel(static_cast<HotkeyAction>(i)));
            ::SendMessageW(action, CB_ADDSTRING, 0,
                           reinterpret_cast<LPARAM>(text.c_str()));
        }

        const HWND key = ::CreateWindowExW(
            0, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_CENTER | WS_BORDER,
            m_left + actionWidth + Scale(8, m_state.dpi), m_y,
            m_width - actionWidth - Scale(8, m_state.dpi), height, m_parent,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdHotkeyKeyFirst + slot)),
            nullptr, nullptr);
        MakeHotkeyEdit(key);
        m_y += Scale(kRow, m_state.dpi);
    }

    // Bir grubun altındaki açıklama satırı.
    //
    // KURAL GÖRÜNÜR OLMALI: "Ctrl/Alt/Shift gerekir" kuralını yalnızca uyarı
    // sesiyle öğretmek, kullanıcıyı tuşa basıp hiçbir şey olmamasıyla baş başa
    // bırakıyordu. Yükseklik üç satıra yeter; kırpılmış bir ipucu, hiç
    // olmamasından beterdir ve Almanca/Rusça çeviriler İngilizceden uzun.
    void Note(const wchar_t* text) {
        const int height = Scale(46, m_state.dpi);
        (void)::CreateWindowExW(0, L"STATIC", text,
                                WS_CHILD | WS_VISIBLE | SS_LEFT, m_left,
                                m_y + Scale(4, m_state.dpi), m_width, height,
                                m_parent, nullptr, nullptr, nullptr);
        m_y += height + Scale(4, m_state.dpi);
    }

    // Etiketli bir satır: solda metin, sağda denetim.
    HWND Labelled(int id, const wchar_t* label, const wchar_t* className,
                  DWORD style, int controlWidth = 0) {
        const int labelWidth = Scale(kLabelWidth, m_state.dpi);
        (void)::CreateWindowExW(0, L"STATIC", label,
                                WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE,
                                m_left, m_y, labelWidth,
                                Scale(24, m_state.dpi), m_parent, nullptr, nullptr,
                                nullptr);
        const int width =
            controlWidth > 0 ? controlWidth : m_width - labelWidth;
        const HWND control = ::CreateWindowExW(
            0, className, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | style,
            m_left + labelWidth, m_y, width, Scale(24, m_state.dpi), m_parent,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr, nullptr);
        m_y += Scale(kRow, m_state.dpi);
        return control;
    }

private:
    HWND m_parent;
    State& m_state;
    int m_left;
    int m_y;
    int m_width;
};

void ApplyFont(HWND parent, HFONT font) {
    for (HWND child = ::GetWindow(parent, GW_CHILD); child != nullptr;
         child = ::GetWindow(child, GW_HWNDNEXT)) {
        ::SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    }
}

// Koyu temada standart denetimler açık kalır. Görsel stil sınıflarını
// değiştirmek, açılır kutunun okunu ve düğme kenarlığını da koyulaştırır;
// yalnızca WM_CTLCOLOR* ile zemin boyamak bunları açık bırakırdı.
void ApplyDarkTheme(HWND parent) {
    if (!theme::IsDark()) {
        return;
    }
    for (HWND child = ::GetWindow(parent, GW_CHILD); child != nullptr;
         child = ::GetWindow(child, GW_HWNDNEXT)) {
        wchar_t className[64] = L"";
        ::GetClassNameW(child, className, 64);
        if (::_wcsicmp(className, L"COMBOBOX") == 0 ||
            ::_wcsicmp(className, L"EDIT") == 0 ||
            ::_wcsicmp(className, HOTKEY_CLASSW) == 0) {
            (void)::SetWindowTheme(child, L"DarkMode_CFD", nullptr);
        } else {
            (void)::SetWindowTheme(child, L"DarkMode_Explorer", nullptr);
        }
    }
}

}  // namespace

int Scale(int value, unsigned dpi) noexcept {
    return ::MulDiv(value, static_cast<int>(dpi), 96);
}

UINT HotkeyActionLabel(HotkeyAction action) noexcept {
    switch (action) {
        case HotkeyAction::Region:       return IDS_ACT_REGION;
        case HotkeyAction::Window:       return IDS_ACT_WINDOW;
        case HotkeyAction::ActiveWindow: return IDS_ACT_ACTIVE_WINDOW;
        case HotkeyAction::Monitor:      return IDS_ACT_MONITOR;
        case HotkeyAction::AllMonitors:  return IDS_ACT_ALL_MONITORS;
        case HotkeyAction::LastRegion:   return IDS_ACT_LAST_REGION;
        case HotkeyAction::Delayed:      return IDS_ACT_DELAYED;
        case HotkeyAction::SelectText:   return IDS_ACT_SELECT_TEXT;
        case HotkeyAction::RegionText:   return IDS_ACT_REGION_TEXT;
        case HotkeyAction::PickColor:    return IDS_ACT_PICK_COLOR;
        case HotkeyAction::History:      return IDS_ACT_HISTORY;
        default:                         return IDS_ACT_NONE;
    }
}

void BuildControls(HWND window, State& state) {
    const unsigned dpi = state.dpi;
    const int pad = Scale(kPad, dpi);
    const int columnWidth =
        (Scale(kWidth, dpi) - pad * 2 - Scale(kColumnGap, dpi) * 2) / 3;
    const int middleLeft = pad + columnWidth + Scale(kColumnGap, dpi);
    const int rightLeft = middleLeft + columnWidth + Scale(kColumnGap, dpi);

    // --- Sol sütun ----------------------------------------------------------
    Cursor left(window, state, pad, pad, columnWidth);

    left.Group(Loc::Str(IDS_SET_GROUP_GENERAL).c_str());
    const HWND language =
        left.Labelled(kIdLanguage, Loc::Str(IDS_SET_LANGUAGE).c_str(), L"COMBOBOX",
                      CBS_DROPDOWNLIST | WS_VSCROLL);
    for (int i = 0; i < Loc::LanguageCount(); ++i) {
        const Loc::Language& entry = Loc::Languages()[i];
        const std::wstring name =
            entry.endonym != nullptr ? entry.endonym : Loc::Str(IDS_LANG_AUTO);
        ::SendMessageW(language, CB_ADDSTRING, 0,
                       reinterpret_cast<LPARAM>(name.c_str()));
        state.languageCodes.emplace_back(entry.code);
    }

    const HWND themeBox =
        left.Labelled(kIdTheme, Loc::Str(IDS_SET_THEME).c_str(), L"COMBOBOX",
                      CBS_DROPDOWNLIST);
    for (const UINT id : {IDS_SET_THEME_SYSTEM, IDS_SET_THEME_LIGHT,
                          IDS_SET_THEME_DARK}) {
        ::SendMessageW(themeBox, CB_ADDSTRING, 0,
                       reinterpret_cast<LPARAM>(Loc::Str(id).c_str()));
    }

    // KABUK MENÜSÜ "Genel"DE: bir yakalama ayarı değil, Windows'la kurulan bir
    // bağ — kaydetme ya da yakalama gruplarının hiçbirine ait değil.
    (void)left.Check(kIdShellMenu, Loc::Str(IDS_SET_SHELL_MENU).c_str());

    left.Group(Loc::Str(IDS_SET_GROUP_SAVE).c_str());
    (void)left.Labelled(kIdFolder, Loc::Str(IDS_SET_FOLDER).c_str(), L"EDIT",
                        ES_AUTOHSCROLL | WS_BORDER);
    (void)::CreateWindowExW(
        0, L"BUTTON", Loc::Str(IDS_SET_BROWSE).c_str(),
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        pad + columnWidth - Scale(96, dpi), left.y(), Scale(96, dpi),
        Scale(26, dpi), window,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdBrowse)), nullptr,
        nullptr);
    left.Advance(Scale(kRow + 4, dpi));

    const HWND format =
        left.Labelled(kIdFormat, Loc::Str(IDS_SET_FORMAT).c_str(), L"COMBOBOX",
                      CBS_DROPDOWNLIST, Scale(110, dpi));

    // KULLANILAMAYAN BİÇİM LİSTEYE GİRMEZ. Windows WebP için yalnızca çözücü
    // taşır; kodlayıcı isteğe bağlı bir mağaza bileşenidir ve çoğu kurulumda
    // yoktur. Seçeneği listede bırakmak, kullanıcının WebP seçip PNG almasına
    // ve bunu hiç öğrenmemesine yol açıyordu. Denetim çalışma zamanında
    // yapıldığı için, bileşen sonradan kurulursa seçenek kendiliğinden belirir.
    struct FormatEntry {
        const wchar_t* label;
        const wchar_t* code;
        ImageFormat format;
    };
    for (const FormatEntry& entry :
         {FormatEntry{L"PNG", L"png", ImageFormat::Png},
          FormatEntry{L"JPEG", L"jpg", ImageFormat::Jpeg},
          FormatEntry{L"WebP", L"webp", ImageFormat::WebP}}) {
        if (entry.format != ImageFormat::Png && !IsFormatAvailable(entry.format)) {
            continue;
        }
        ::SendMessageW(format, CB_ADDSTRING, 0,
                       reinterpret_cast<LPARAM>(entry.label));
        state.formatCodes.emplace_back(entry.code);
    }
    (void)left.Labelled(kIdQuality, Loc::Str(IDS_SET_QUALITY).c_str(), L"EDIT",
                        ES_NUMBER | WS_BORDER, Scale(70, dpi));
    (void)left.Labelled(kIdNameFormat, Loc::Str(IDS_SET_NAME_FORMAT).c_str(),
                        L"EDIT", ES_AUTOHSCROLL | WS_BORDER);
    (void)left.Labelled(kIdSubFolder, Loc::Str(IDS_SET_SUBFOLDER).c_str(),
                        L"EDIT", ES_AUTOHSCROLL | WS_BORDER);

    left.Group(Loc::Str(IDS_SET_GROUP_HISTORY).c_str());
    (void)left.Labelled(kIdHistoryLimit,
                        Loc::Str(IDS_SET_HISTORY_LIMIT).c_str(), L"EDIT",
                        ES_NUMBER | WS_BORDER, Scale(70, dpi));
    (void)::CreateWindowExW(
        0, L"BUTTON", Loc::Str(IDS_SET_HISTORY_CLEAR).c_str(),
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, pad, left.y(),
        Scale(150, dpi), Scale(26, dpi), window,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdHistoryClear)), nullptr,
        nullptr);

    // --- Orta sütun ---------------------------------------------------------
    Cursor middle(window, state, middleLeft, pad, columnWidth);

    middle.Group(Loc::Str(IDS_SET_GROUP_CAPTURE).c_str());
    (void)middle.Labelled(kIdDelay, Loc::Str(IDS_SET_DELAY).c_str(), L"EDIT",
                          ES_NUMBER | WS_BORDER, Scale(70, dpi));
    (void)middle.Labelled(kIdDimStrength, Loc::Str(IDS_SET_DIM).c_str(), L"EDIT",
                          ES_NUMBER | WS_BORDER, Scale(70, dpi));
    (void)middle.Check(kIdMagnifier, Loc::Str(IDS_SET_MAGNIFIER).c_str());
    (void)middle.Check(kIdHighlight, Loc::Str(IDS_SET_HIGHLIGHT).c_str());
    (void)middle.Check(kIdIncludeCursor, Loc::Str(IDS_SET_CURSOR).c_str());
    (void)middle.Check(kIdPrintScreen, Loc::Str(IDS_SET_PRINTSCREEN).c_str());
    (void)middle.Check(kIdShutter, Loc::Str(IDS_SET_SHUTTER).c_str());

    middle.Group(Loc::Str(IDS_SET_GROUP_ANNOTATE).c_str());
    (void)middle.Labelled(kIdBlurStrength,
                          Loc::Str(IDS_SET_BLUR_STRENGTH).c_str(), L"EDIT",
                          ES_NUMBER | WS_BORDER, Scale(70, dpi));
    (void)middle.Labelled(kIdMosaicStrength,
                          Loc::Str(IDS_SET_MOSAIC_STRENGTH).c_str(), L"EDIT",
                          ES_NUMBER | WS_BORDER, Scale(70, dpi));

    // --- Sağ sütun ----------------------------------------------------------
    Cursor right(window, state, rightLeft, pad, columnWidth);

    right.Group(Loc::Str(IDS_SET_GROUP_AFTER).c_str());
    (void)right.Check(kIdAfterCopy, Loc::Str(IDS_SET_AFTER_COPY).c_str());
    (void)right.Check(kIdAfterSave, Loc::Str(IDS_SET_AFTER_SAVE).c_str());
    (void)right.Check(kIdAfterPin, Loc::Str(IDS_SET_AFTER_PIN).c_str());
    (void)right.Check(kIdAfterEditor, Loc::Str(IDS_SET_AFTER_EDITOR).c_str());
    (void)right.Check(kIdAfterCopyPath, Loc::Str(IDS_SET_AFTER_COPYPATH).c_str());
    (void)right.Check(kIdAfterCopyFile, Loc::Str(IDS_SET_AFTER_COPYFILE).c_str());
    (void)right.Check(kIdAfterReveal, Loc::Str(IDS_SET_AFTER_REVEAL).c_str());
    (void)right.Check(kIdAfterOcr, Loc::Str(IDS_SET_AFTER_OCR).c_str());
    (void)right.Check(kIdNotify, Loc::Str(IDS_SET_NOTIFY).c_str());

    right.Group(Loc::Str(IDS_SET_GROUP_HOTKEYS).c_str());
    for (int slot = 0; slot < kHotkeySlots; ++slot) {
        right.HotkeyRow(slot);
    }
    right.Note(Loc::Str(IDS_SET_HOTKEY_HINT).c_str());

    // --- Alt şerit ----------------------------------------------------------
    const int buttonWidth = Scale(110, dpi);
    const int buttonHeight = Scale(kButtonHeight, dpi);
    const int bottom = Scale(kHeight, dpi) - pad - buttonHeight;

    (void)::CreateWindowExW(
        0, L"BUTTON", Loc::Str(IDS_SET_RESET).c_str(),
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, pad, bottom,
        buttonWidth + Scale(30, dpi), buttonHeight, window,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdReset)), nullptr, nullptr);

    const int okLeft = Scale(kWidth, dpi) - pad - buttonWidth;
    (void)::CreateWindowExW(
        0, L"BUTTON", Loc::Str(IDS_SET_OK).c_str(),
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, okLeft, bottom,
        buttonWidth, buttonHeight, window,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdOk)), nullptr, nullptr);
    (void)::CreateWindowExW(
        0, L"BUTTON", Loc::Str(IDS_SET_CANCEL).c_str(),
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        okLeft - buttonWidth - Scale(10, dpi), bottom, buttonWidth, buttonHeight,
        window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdCancel)), nullptr,
        nullptr);

    ApplyFont(window, state.font);
    ApplyDarkTheme(window);
}

void Paint(HWND window, State& state) {
    PAINTSTRUCT paint{};
    const HDC dc = ::BeginPaint(window, &paint);
    if (dc == nullptr) {
        return;
    }

    RECT client{};
    ::GetClientRect(window, &client);
    const Palette& colors = theme::Colors();

    const HBRUSH background = ::CreateSolidBrush(colors.surface);
    if (background != nullptr) {
        ::FillRect(dc, &client, background);
        ::DeleteObject(background);
    }

    ::SetBkMode(dc, TRANSPARENT);
    const HGDIOBJ oldFont = ::SelectObject(dc, state.groupFont);
    ::SetTextColor(dc, colors.accent);
    for (const GroupTitle& group : state.groups) {
        RECT area = group.bounds;
        ::DrawTextW(dc, group.text.c_str(), -1, &area,
                    DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        // Başlığın altına ince bir çizgi: gruplar arasındaki ayrımı boşluk
        // tek başına taşımıyor, sütunlar yan yanayken sınırlar kayboluyordu.
        RECT measure{0, 0, 0, 0};
        ::DrawTextW(dc, group.text.c_str(), -1, &measure,
                    DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX);
        const int lineLeft = group.bounds.left + geom::Width(measure) +
                             Scale(10, state.dpi);
        const int lineY = (group.bounds.top + group.bounds.bottom) / 2;
        const RECT line{lineLeft, lineY, group.bounds.right, lineY + 1};
        const HBRUSH rule = ::CreateSolidBrush(colors.border);
        if (rule != nullptr) {
            ::FillRect(dc, &line, rule);
            ::DeleteObject(rule);
        }
    }
    ::SelectObject(dc, oldFont);
    ::EndPaint(window, &paint);
}

}  // namespace settings_ui
}  // namespace crisp
