// OverlayActions.cpp — bkz. OverlayActions.h.
#include "OverlayActions.h"

#include "Geometry.h"
#include "Localization.h"
#include "OverlayDraw.h"

#include <algorithm>

namespace crisp {
namespace {

using draw::DrawFrame;
using draw::DrawPill;
using draw::FillRectColor;
using draw::kAccent;
using draw::kPanelBack;
using draw::kPanelBorder;
using draw::kTextDim;
using draw::kTextPrimary;
using draw::Scale;
using draw::ToClient;

// Tek bir düğmenin simgesi. Basit geometri; EditorGlyphs.cpp ile aynı üslup.
//
// YEREL `S`: GDI çizim çağrılarının hepsi `int` alıyor, `draw::Scale` ise LONG
// döndürüyor. Her çağrının içine bir static_cast serpiştirmek, üç satırlık bir
// simgeyi okunmaz hâle getiriyordu.
void DrawGlyph(HDC dc, const RECT& box, OverlayAction action, COLORREF color,
               unsigned dpi) {
    const auto S = [dpi](int value) {
        return static_cast<int>(Scale(value, dpi));
    };

    const int cx = static_cast<int>(box.left + geom::Width(box) / 2);
    const int cy = static_cast<int>(box.top + geom::Height(box) / 2);
    const int r = S(7);
    const int pen = (std::max)(1, S(2) - 1);

    const HPEN stroke = ::CreatePen(PS_SOLID, pen, color);
    if (stroke == nullptr) {
        return;
    }
    const HGDIOBJ oldPen = ::SelectObject(dc, stroke);
    const HGDIOBJ oldBrush = ::SelectObject(dc, ::GetStockObject(NULL_BRUSH));

    switch (action) {
        case OverlayAction::Copy: {
            // İki üst üste binmiş kare — panonun evrensel işareti.
            const int off = S(3);
            ::Rectangle(dc, cx - r, cy - r, cx + r - off, cy + r - off);
            ::Rectangle(dc, cx - r + off, cy - r + off, cx + r, cy + r);
            break;
        }
        case OverlayAction::Save: {
            // Aşağı ok + taban çizgisi: "diske indir".
            ::MoveToEx(dc, cx, cy - r, nullptr);
            ::LineTo(dc, cx, cy + S(3));
            ::MoveToEx(dc, cx - S(4), cy - S(1), nullptr);
            ::LineTo(dc, cx, cy + S(3));
            ::LineTo(dc, cx + S(4), cy - S(1));
            ::MoveToEx(dc, cx - r, cy + r, nullptr);
            ::LineTo(dc, cx + r, cy + r);
            break;
        }
        case OverlayAction::Edit: {
            // Kalem: gövde bir çizgi, ucu küçük bir üçgen.
            ::MoveToEx(dc, cx - r + S(2), cy + r - S(2), nullptr);
            ::LineTo(dc, cx + r - S(2), cy - r + S(2));
            const POINT tip[3] = {{cx - r, cy + r},
                                  {cx - r + S(5), cy + r - S(1)},
                                  {cx - r + S(1), cy + r - S(5)}};
            const HBRUSH fill = ::CreateSolidBrush(color);
            if (fill != nullptr) {
                const HGDIOBJ previous = ::SelectObject(dc, fill);
                ::Polygon(dc, tip, 3);
                ::SelectObject(dc, previous);
                ::DeleteObject(fill);
            }
            break;
        }
        case OverlayAction::Ocr: {
            // Üç metin satırı, sonuncusu kısa.
            for (int i = 0; i < 3; ++i) {
                const int y = cy - r + S(3) + i * S(5);
                const int right = i == 2 ? cx + S(1) : cx + r;
                ::MoveToEx(dc, cx - r, y, nullptr);
                ::LineTo(dc, right, y);
            }
            break;
        }
        case OverlayAction::Pin: {
            // Raptiye: yuvarlak baş ve aşağı inen gövde.
            ::Ellipse(dc, cx - S(5), cy - r, cx + S(5), cy - r + S(10));
            ::MoveToEx(dc, cx, cy - r + S(10), nullptr);
            ::LineTo(dc, cx, cy + r);
            break;
        }
        case OverlayAction::Upload: {
            // Yukarı ok: ağa gidiyor.
            ::MoveToEx(dc, cx, cy + r - S(2), nullptr);
            ::LineTo(dc, cx, cy - r + S(2));
            ::MoveToEx(dc, cx - S(4), cy - r + S(6), nullptr);
            ::LineTo(dc, cx, cy - r + S(2));
            ::LineTo(dc, cx + S(4), cy - r + S(6));
            break;
        }
        default:
            break;
    }

    ::SelectObject(dc, oldBrush);
    ::SelectObject(dc, oldPen);
    ::DeleteObject(stroke);
}

}  // namespace

void DrawActionBar(HDC dc, const RECT& screen, unsigned dpi,
                   const ActionButton* buttons, int count, int hovered,
                   HFONT font) {
    if (buttons == nullptr || count <= 0) {
        return;
    }

    const LONG padding = Scale(kActionBarPadding, dpi);
    const RECT first = buttons[0].bounds;
    const RECT last = buttons[count - 1].bounds;
    const RECT bar{first.left - padding, first.top - padding,
                   last.right + padding, last.bottom + padding};

    const RECT client = ToClient(bar, screen);
    FillRectColor(dc, client, kPanelBack);
    DrawFrame(dc, client, 1, kPanelBorder);

    for (int i = 0; i < count; ++i) {
        const RECT box = ToClient(buttons[i].bounds, screen);
        const bool active = (i == hovered);
        if (active) {
            FillRectColor(dc, box, kAccent);
        }
        DrawGlyph(dc, box, buttons[i].action,
                  active ? RGB(255, 255, 255) : kTextDim, dpi);
    }

    if (hovered < 0 || hovered >= count) {
        return;
    }

    // SİMGENİN ADI YAZILIYOR. Altı simge sezgisel olsa bile "bu hangisiydi"
    // sorusu ilk kullanımda mutlaka soruluyor, ve çubuğun altında boş yer var.
    // Adı çubuğun ALTINA koyuyoruz: üstüne koymak seçimin kendisini örterdi.
    const std::wstring name = Loc::Str(buttons[hovered].nameId);
    const RECT box = buttons[hovered].bounds;
    const POINT tip{box.left, bar.bottom + Scale(4, dpi)};
    (void)DrawPill(dc, ToClient(tip, screen), name.c_str(), font, dpi,
                   kTextPrimary);
}

}  // namespace crisp
