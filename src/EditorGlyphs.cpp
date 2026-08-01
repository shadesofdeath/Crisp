// EditorGlyphs.cpp — Araç ve eylem simgeleri.
//
// AYRI DOSYA: on araç ve on dört eylem için çizim kodu tek başına iki yüz
// satır. Düğme zeminleriyle aynı dosyada durunca EditorChrome.cpp ev
// kuralının 400 satır sınırını aşıyordu (docs §9).
//
// EYLEM SİMGELERİNİN YANINDA ADI YAZAR. Segoe MDL2 kod noktaları Özel
// Kullanım Alanı'ndadır: kaynakta boş bir kutu gibi görünürler ve hangi
// simgenin seçildiği ancak çalıştırıp bakarak anlaşılırdı. Yanına "// ZoomOut"
// yazmak, o adımı okuyan herkes için ortadan kaldırır.
#include "EditorInternal.h"

#include "Geometry.h"

#include <algorithm>

namespace crisp {
namespace editor {

void DrawToolGlyph(HDC dc, const RECT& box, ToolKind tool, COLORREF color,
                   unsigned dpi) {
    const int cx = box.left + static_cast<int>(geom::Width(box)) / 2;
    const int cy = box.top + static_cast<int>(geom::Height(box)) / 2;
    const int r = Scale(8, dpi);
    const int penWidth = (std::max)(1, Scale(2, dpi));

    const HPEN pen = ::CreatePen(PS_SOLID, penWidth, color);
    if (pen == nullptr) {
        return;
    }
    const HGDIOBJ oldPen = ::SelectObject(dc, pen);
    const HGDIOBJ oldBrush = ::SelectObject(dc, ::GetStockObject(NULL_BRUSH));

    switch (tool) {
        case ToolKind::Arrow: {
            ::MoveToEx(dc, cx - r, cy + r, nullptr);
            ::LineTo(dc, cx + r - Scale(3, dpi), cy - r + Scale(3, dpi));
            const POINT head[3] = {
                {cx + r, cy - r},
                {cx + r - Scale(8, dpi), cy - r + Scale(2, dpi)},
                {cx + r - Scale(2, dpi), cy - r + Scale(8, dpi)}};
            const HBRUSH fill = ::CreateSolidBrush(color);
            if (fill != nullptr) {
                const HGDIOBJ previous = ::SelectObject(dc, fill);
                ::Polygon(dc, head, 3);
                ::SelectObject(dc, previous);
                ::DeleteObject(fill);
            }
            break;
        }
        case ToolKind::Select: {
            // Fare oku: uç sol üstte, kuyruk sağ altta. Noktalar 11 × 17'lik
            // bir kutunun içinde tanımlanır ve kutu düğmenin ortasına oturur.
            //
            // KALEM DEĞİL FIRÇA, ve kalem AÇIKÇA BOŞA ALINIR: dıştaki 2
            // piksellik kalem, 11 piksellik bir okun sivri ucunu ve kuyruk
            // çentiğini yuvarlayıp şekli tanınmaz bir lekeye çeviriyordu —
            // "ikonu bozuk" denen şey buydu.
            const int left = cx - Scale(5, dpi);
            const int top = cy - Scale(8, dpi);
            auto at = [&](int x, int y) {
                return POINT{left + Scale(x, dpi), top + Scale(y, dpi)};
            };
            const POINT arrow[7] = {at(0, 0),  at(0, 15), at(4, 11),
                                    at(6, 16), at(8, 15), at(6, 10),
                                    at(10, 10)};
            const HBRUSH fill = ::CreateSolidBrush(color);
            if (fill != nullptr) {
                const HGDIOBJ previousBrush = ::SelectObject(dc, fill);
                const HGDIOBJ previousPen =
                    ::SelectObject(dc, ::GetStockObject(NULL_PEN));
                // NULL_PEN ile Polygon sağ ve alt kenardan bir piksel eksik
                // çizer; nokta dizisi bunu telafi edecek kadar geniş.
                ::Polygon(dc, arrow, 7);
                ::SelectObject(dc, previousPen);
                ::SelectObject(dc, previousBrush);
                ::DeleteObject(fill);
            }
            break;
        }
        case ToolKind::Line:
            ::MoveToEx(dc, cx - r, cy + r, nullptr);
            ::LineTo(dc, cx + r, cy - r);
            break;
        case ToolKind::Rectangle:
            ::Rectangle(dc, cx - r, cy - r + Scale(2, dpi), cx + r,
                        cy + r - Scale(2, dpi));
            break;
        case ToolKind::Ellipse:
            ::Ellipse(dc, cx - r, cy - r + 1, cx + r, cy + r - 1);
            break;
        case ToolKind::Pen:
            ::MoveToEx(dc, cx - r, cy + r - Scale(2, dpi), nullptr);
            ::LineTo(dc, cx - r / 3, cy - r);
            ::LineTo(dc, cx + r / 3, cy + r - Scale(2, dpi));
            ::LineTo(dc, cx + r, cy - r);
            break;
        case ToolKind::Highlighter: {
            const HPEN thick = ::CreatePen(PS_SOLID, Scale(6, dpi), color);
            if (thick != nullptr) {
                const HGDIOBJ previous = ::SelectObject(dc, thick);
                ::MoveToEx(dc, cx - r, cy + r / 2, nullptr);
                ::LineTo(dc, cx + r, cy - r / 2);
                ::SelectObject(dc, previous);
                ::DeleteObject(thick);
            }
            break;
        }
        case ToolKind::Text:
        case ToolKind::StepNumber: {
            const HFONT created = CreateUiFont(dpi, 12, FW_SEMIBOLD);
            if (created != nullptr) {
                const HGDIOBJ previous = ::SelectObject(dc, created);
                ::SetBkMode(dc, TRANSPARENT);
                ::SetTextColor(dc, color);
                RECT area = box;
                ::DrawTextW(dc, tool == ToolKind::Text ? L"T" : L"1", -1, &area,
                            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                ::SelectObject(dc, previous);
                ::DeleteObject(created);
            }
            break;
        }
        case ToolKind::Blur:
            for (int i = 0; i < 3; ++i) {
                const int radius = r - i * Scale(3, dpi);
                if (radius > 0) {
                    ::Ellipse(dc, cx - radius, cy - radius, cx + radius,
                              cy + radius);
                }
            }
            break;
        case ToolKind::Crop: {
            // İki L: fotoğrafçı kadrajı. Dikdörtgen aracıyla karışmasın diye
            // kapalı bir çerçeve DEĞİL.
            ::MoveToEx(dc, cx - r, cy - r + Scale(4, dpi), nullptr);
            ::LineTo(dc, cx - r, cy + r);
            ::LineTo(dc, cx + r - Scale(4, dpi), cy + r);
            ::MoveToEx(dc, cx + r, cy + r - Scale(4, dpi), nullptr);
            ::LineTo(dc, cx + r, cy - r);
            ::LineTo(dc, cx - r + Scale(4, dpi), cy - r);
            break;
        }
        case ToolKind::Mosaic: {
            const int cell = (std::max)(2, r * 2 / 3);
            const HBRUSH fill = ::CreateSolidBrush(color);
            if (fill != nullptr) {
                for (int row = 0; row < 3; ++row) {
                    for (int col = 0; col < 3; ++col) {
                        if (((row + col) % 2) != 0) {
                            continue;
                        }
                        const RECT cellRect{
                            cx - r + col * cell, cy - r + row * cell,
                            cx - r + (col + 1) * cell - 1,
                            cy - r + (row + 1) * cell - 1};
                        ::FillRect(dc, &cellRect, fill);
                    }
                }
                ::DeleteObject(fill);
            }
            break;
        }
        default:
            break;
    }

    ::SelectObject(dc, oldBrush);
    ::SelectObject(dc, oldPen);
    ::DeleteObject(pen);
}

// Dolgu ve efekt simgeleri ELLE ÇİZİLİR.
//
// SEBEBİ BİR HATA: ikisi için de Segoe MDL2'den ezberden kod noktası seçildi
// ve biri göz, diğeri onay listesi çıktı. Bir boya kovası ile bir kontrast
// dairesi zaten birkaç çizgi; font tahmininden hem daha kısa hem daha kesin.
[[nodiscard]] bool DrawDrawnActionGlyph(HDC dc, const RECT& box, int action,
                                        COLORREF color, unsigned dpi) {
    const int cx = box.left + static_cast<int>(geom::Width(box)) / 2;
    const int cy = box.top + static_cast<int>(geom::Height(box)) / 2;
    const int r = Scale(8, dpi);

    if (action == kActionFill) {
        // Yarısı boş, yarısı dolu bir kare: "dolgu açık mı" sorusunun cevabı.
        const HPEN pen = ::CreatePen(PS_SOLID, (std::max)(1, Scale(2, dpi)), color);
        const HBRUSH brush = ::CreateSolidBrush(color);
        if (pen == nullptr || brush == nullptr) {
            if (pen != nullptr) { ::DeleteObject(pen); }
            if (brush != nullptr) { ::DeleteObject(brush); }
            return true;
        }
        const HGDIOBJ oldPen = ::SelectObject(dc, pen);
        const HGDIOBJ oldBrush = ::SelectObject(dc, ::GetStockObject(NULL_BRUSH));
        ::Rectangle(dc, cx - r, cy - r, cx + r, cy + r);
        ::SelectObject(dc, brush);
        ::SelectObject(dc, ::GetStockObject(NULL_PEN));
        const RECT inner{cx - r + Scale(3, dpi), cy - r + Scale(3, dpi),
                         cx + r - Scale(2, dpi), cy + r - Scale(2, dpi)};
        ::FillRect(dc, &inner, brush);
        ::SelectObject(dc, oldBrush);
        ::SelectObject(dc, oldPen);
        ::DeleteObject(pen);
        ::DeleteObject(brush);
        return true;
    }

    if (action == kActionEffects) {
        // Yarısı dolu daire: parlaklık/kontrast ayarının evrensel simgesi.
        const HPEN pen = ::CreatePen(PS_SOLID, (std::max)(1, Scale(2, dpi)), color);
        const HBRUSH brush = ::CreateSolidBrush(color);
        if (pen == nullptr || brush == nullptr) {
            if (pen != nullptr) { ::DeleteObject(pen); }
            if (brush != nullptr) { ::DeleteObject(brush); }
            return true;
        }
        const HGDIOBJ oldPen = ::SelectObject(dc, pen);
        const HGDIOBJ oldBrush = ::SelectObject(dc, ::GetStockObject(NULL_BRUSH));
        ::Ellipse(dc, cx - r, cy - r, cx + r, cy + r);
        ::SelectObject(dc, brush);
        ::SelectObject(dc, ::GetStockObject(NULL_PEN));
        // Sağ yarım daire: aynı elipsin sağ yarısı, düz kenarı ortada.
        ::Chord(dc, cx - r, cy - r, cx + r, cy + r, cx, cy - r, cx, cy + r);
        ::SelectObject(dc, oldBrush);
        ::SelectObject(dc, oldPen);
        ::DeleteObject(pen);
        ::DeleteObject(brush);
        return true;
    }
    return false;
}

void DrawActionGlyph(HDC dc, const RECT& box, int action, COLORREF color,
                     unsigned dpi) {
    if (DrawDrawnActionGlyph(dc, box, action, color, dpi)) {
        return;
    }

    LOGFONTW font{};
    font.lfHeight = -Scale(15, dpi);
    font.lfWeight = FW_NORMAL;
    font.lfCharSet = DEFAULT_CHARSET;
    // Segoe MDL2 Assets Windows 10/11'de daima vardır ve simgeleri tek
    // renklidir; metin yazı tipiyle çizilen oklar sürümden sürüme kayar.
    ::wcscpy_s(font.lfFaceName, L"Segoe MDL2 Assets");
    const HFONT created = ::CreateFontIndirectW(&font);
    if (created == nullptr) {
        return;
    }

    // SOLA DÖNDÜRME SİMGESİ YOKTUR: Segoe MDL2'de saat yönünde dönen bir ok
    // (E7AD) var, aynası yok. Başka bir kavisli ok seçmek geri al/yinele
    // düğmeleriyle karışırdı; aynı glifi yatayda çevirmek iki düğmeyi
    // birbirinin tam yansıması yapar.
    const wchar_t* glyph = L"";
    bool mirror = false;
    switch (action) {
        case kActionZoomOut:     glyph = L""; break;
        case kActionZoomIn:      glyph = L""; break;
        case kActionZoomFit:     glyph = L""; break;
        case kActionOcr:         glyph = L""; break;
        case kActionRotateLeft:  glyph = L""; mirror = true; break;
        case kActionRotateRight: glyph = L""; break;
        case kActionScale:       glyph = L""; break;
        case kActionEffects:     glyph = L""; break;   // Color
        case kActionFill:        glyph = L""; break;   // ColorFill
        case kActionUndo:        glyph = L""; break;
        case kActionRedo:        glyph = L""; break;
        case kActionClear:       glyph = L""; break;
        case kActionCopy:        glyph = L""; break;
        case kActionSave:        glyph = L""; break;
        case kActionSaveAs:      glyph = L""; break;
        case kActionClose:       glyph = L""; break;
        default: break;
    }

    const HGDIOBJ previous = ::SelectObject(dc, created);
    ::SetBkMode(dc, TRANSPARENT);
    ::SetTextColor(dc, color);

    int previousMode = 0;
    XFORM previousTransform{};
    if (mirror) {
        // Düğmenin kendi orta ekseninde yansıt: x' = (left + right) - x.
        previousMode = ::SetGraphicsMode(dc, GM_ADVANCED);
        ::GetWorldTransform(dc, &previousTransform);
        const XFORM flip{-1.0f, 0.0f, 0.0f, 1.0f,
                         static_cast<FLOAT>(box.left + box.right), 0.0f};
        ::SetWorldTransform(dc, &flip);
    }

    RECT area = box;
    ::DrawTextW(dc, glyph, -1, &area, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    if (mirror) {
        ::SetWorldTransform(dc, &previousTransform);
        ::SetGraphicsMode(dc, previousMode);
    }
    ::SelectObject(dc, previous);
    ::DeleteObject(created);
}

void DrawChevron(HDC dc, POINT centre, COLORREF color, unsigned dpi) {
    const int width = Scale(4, dpi);
    const int height = Scale(2, dpi);
    const HPEN pen = ::CreatePen(PS_SOLID, (std::max)(1, Scale(1, dpi)), color);
    if (pen == nullptr) {
        return;
    }
    const HGDIOBJ oldPen = ::SelectObject(dc, pen);
    ::MoveToEx(dc, centre.x - width, centre.y - height, nullptr);
    ::LineTo(dc, centre.x, centre.y + height);
    ::LineTo(dc, centre.x + width + 1, centre.y - height - 1);
    ::SelectObject(dc, oldPen);
    ::DeleteObject(pen);
}

}  // namespace editor
}  // namespace crisp
