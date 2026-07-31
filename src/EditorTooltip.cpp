// EditorTooltip.cpp — Araç çubuğu düğmelerinin ipuçları.
//
// NEDEN GEREKLİ: araç çubuğunda yirmi üç düğme var ve hepsi yalnızca simge.
// "Aa", iki dairesel ok ve çapraz oklar ilk kez açan biri için tahmin
// konusuydu; simgeyi seçen kişi ne anlattığını bilir, kullanıcı bilmez.
//
// NEDEN KENDİMİZ ÇİZİYORUZ: standart araç ipucu denetimi (tooltips_class32)
// koyu temaya uymuyor — aynı MessageBoxW gibi kendi rengini kullanıyor ve
// koyu bir araç çubuğunun altında beyaz bir kutu açılıyordu.
#include "EditorInternal.h"

#include "Geometry.h"
#include "Localization.h"
#include "Theme.h"
#include "resource.h"

#include <string>

namespace crisp {
namespace editor {
namespace {

// İmleç düğmenin üstünde bu kadar beklerse ipucu açılır. Daha kısası,
// araç çubuğu boyunca fareyi gezdirirken arka arkaya kutular açardı.
constexpr UINT kDelayMs = 550;

[[nodiscard]] UINT ToolTipId(ToolKind tool) noexcept {
    switch (tool) {
        case ToolKind::Rectangle:   return IDS_TIP_RECTANGLE;
        case ToolKind::Ellipse:     return IDS_TIP_ELLIPSE;
        case ToolKind::Pen:         return IDS_TIP_PEN;
        case ToolKind::Highlighter: return IDS_TIP_HIGHLIGHTER;
        case ToolKind::Text:        return IDS_TIP_TEXT;
        case ToolKind::StepNumber:  return IDS_TIP_STEP;
        case ToolKind::Blur:        return IDS_TIP_BLUR;
        case ToolKind::Mosaic:      return IDS_TIP_MOSAIC;
        case ToolKind::Crop:        return IDS_TIP_CROP;
        default:                    return IDS_TIP_ARROW;
    }
}

[[nodiscard]] UINT ActionTipId(int action) noexcept {
    switch (action) {
        case kActionZoomOut:     return IDS_TIP_ZOOM_OUT;
        case kActionZoomIn:      return IDS_TIP_ZOOM_IN;
        case kActionZoomFit:     return IDS_TIP_ZOOM_FIT;
        case kActionOcr:         return IDS_TIP_OCR;
        case kActionRotateLeft:  return IDS_TIP_ROTATE_LEFT;
        case kActionRotateRight: return IDS_TIP_ROTATE_RIGHT;
        case kActionScale:       return IDS_TIP_SCALE;
        case kActionUndo:        return IDS_TIP_UNDO;
        case kActionRedo:        return IDS_TIP_REDO;
        case kActionClear:       return IDS_TIP_CLEAR;
        case kActionCopy:        return IDS_TIP_COPY;
        case kActionSave:        return IDS_TIP_SAVE;
        default:                 return IDS_TIP_CLOSE;
    }
}

// Kısayolu olan düğmelerde ipucu onu da gösterir; menülerdeki gibi ikinci bir
// sütunda değil, metnin sonunda parantez içinde.
[[nodiscard]] const wchar_t* ShortcutFor(const Button& button) noexcept {
    if (button.kind == ButtonKind::Action) {
        switch (button.action) {
            case kActionUndo:    return L"Ctrl+Z";
            case kActionRedo:    return L"Ctrl+Y";
            case kActionCopy:    return L"Ctrl+C";
            case kActionSave:    return L"Ctrl+S";
            case kActionZoomIn:  return L"Ctrl++";
            case kActionZoomOut: return L"Ctrl+-";
            case kActionZoomFit: return L"Ctrl+0";
            case kActionClose:   return L"Esc";
            default:             return nullptr;
        }
    }
    if (button.kind != ButtonKind::Tool) {
        return nullptr;
    }
    // Araçlar 1..0 sırasıyla; dizi LayoutButtons'taki sırayla aynıdır.
    switch (button.tool) {
        case ToolKind::Arrow:       return L"1";
        case ToolKind::Rectangle:   return L"2";
        case ToolKind::Ellipse:     return L"3";
        case ToolKind::Pen:         return L"4";
        case ToolKind::Highlighter: return L"5";
        case ToolKind::Text:        return L"6";
        case ToolKind::StepNumber:  return L"7";
        case ToolKind::Blur:        return L"8";
        case ToolKind::Mosaic:      return L"9";
        case ToolKind::Crop:        return L"0";
        default:                    return nullptr;
    }
}

}  // namespace

std::wstring TooltipText(const Button& button) {
    std::wstring text;
    switch (button.kind) {
        case ButtonKind::Tool:      text = Loc::Str(ToolTipId(button.tool)); break;
        case ButtonKind::Action:    text = Loc::Str(ActionTipId(button.action)); break;
        case ButtonKind::Color:     text = Loc::Str(IDS_TIP_COLOR); break;
        case ButtonKind::Thickness: text = Loc::Str(IDS_TIP_THICKNESS); break;
        default:                    return std::wstring();
    }

    const wchar_t* shortcut = ShortcutFor(button);
    if (shortcut != nullptr) {
        text += L"  (";
        text += shortcut;
        text += L')';
    }
    return text;
}

void UpdateTooltipHover(HWND window, State& state, int button) {
    if (button == state.tooltipButton) {
        return;
    }
    state.tooltipButton = button;

    const bool wasVisible = state.tooltipVisible;
    state.tooltipVisible = false;
    ::KillTimer(window, kTooltipTimer);

    if (button >= 0) {
        ::SetTimer(window, kTooltipTimer, kDelayMs, nullptr);
    }
    if (wasVisible) {
        ::InvalidateRect(window, nullptr, FALSE);
    }
}

void ShowTooltipNow(HWND window, State& state) {
    ::KillTimer(window, kTooltipTimer);
    if (state.tooltipButton < 0 ||
        state.tooltipButton >= static_cast<int>(state.buttons.size())) {
        return;
    }
    state.tooltipVisible = true;
    ::InvalidateRect(window, nullptr, FALSE);
}

void HideTooltip(HWND window, State& state) {
    ::KillTimer(window, kTooltipTimer);
    state.tooltipButton = -1;
    if (state.tooltipVisible) {
        state.tooltipVisible = false;
        ::InvalidateRect(window, nullptr, FALSE);
    }
}

void DrawTooltip(HDC dc, const State& state, const RECT& client) {
    if (!state.tooltipVisible || state.tooltipButton < 0 ||
        state.tooltipButton >= static_cast<int>(state.buttons.size())) {
        return;
    }
    const Button& button = state.buttons[static_cast<size_t>(state.tooltipButton)];
    const std::wstring text = TooltipText(button);
    if (text.empty()) {
        return;
    }

    const Palette& colors = theme::Colors();
    const unsigned dpi = state.dpi;

    LOGFONTW font{};
    font.lfHeight = -::MulDiv(9, static_cast<int>(dpi), 72);
    font.lfWeight = FW_NORMAL;
    font.lfCharSet = DEFAULT_CHARSET;
    font.lfQuality = CLEARTYPE_QUALITY;
    ::wcscpy_s(font.lfFaceName, L"Segoe UI");
    const HFONT created = ::CreateFontIndirectW(&font);
    if (created == nullptr) {
        return;
    }

    const HGDIOBJ oldFont = ::SelectObject(dc, created);
    RECT measure{0, 0, 0, 0};
    ::DrawTextW(dc, text.c_str(), -1, &measure,
                DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX);

    const int padX = Scale(9, dpi);
    const int padY = Scale(5, dpi);
    const int width = static_cast<int>(geom::Width(measure)) + padX * 2;
    const int height = static_cast<int>(geom::Height(measure)) + padY * 2;

    // Düğmenin ALTINDA ve ortalanmış; ekran kenarına taşarsa içeri çekilir.
    // Üstte gösterilseydi araç çubuğunun dışına, başlık çubuğuna binerdi.
    int left = (button.bounds.left + button.bounds.right) / 2 - width / 2;
    const int top = button.bounds.bottom + Scale(6, dpi);
    const int limit = static_cast<int>(geom::Width(client)) - width - Scale(4, dpi);
    left = left < Scale(4, dpi) ? Scale(4, dpi) : (left > limit ? limit : left);

    const RECT box{left, top, left + width, top + height};
    const int radius = Scale(6, dpi) * 2;

    const HBRUSH fill = ::CreateSolidBrush(colors.surfaceAlt);
    const HPEN edge = ::CreatePen(PS_SOLID, 1, colors.border);
    if (fill != nullptr && edge != nullptr) {
        const HGDIOBJ oldBrush = ::SelectObject(dc, fill);
        const HGDIOBJ oldPen = ::SelectObject(dc, edge);
        ::RoundRect(dc, box.left, box.top, box.right, box.bottom, radius, radius);
        ::SelectObject(dc, oldPen);
        ::SelectObject(dc, oldBrush);
    }
    if (fill != nullptr) {
        ::DeleteObject(fill);
    }
    if (edge != nullptr) {
        ::DeleteObject(edge);
    }

    ::SetBkMode(dc, TRANSPARENT);
    ::SetTextColor(dc, colors.text);
    RECT area = box;
    ::DrawTextW(dc, text.c_str(), -1, &area,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    ::SelectObject(dc, oldFont);
    ::DeleteObject(created);
}

}  // namespace editor
}  // namespace crisp
