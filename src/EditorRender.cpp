// EditorRender.cpp — bkz. EditorRender.h.
#include "EditorRender.h"

#include "Geometry.h"
#include "ImageEffects.h"
#include "Util.h"

#include <cmath>
#include <cstdio>

namespace crisp {
namespace {

[[nodiscard]] int Scale(int value, unsigned dpi) noexcept {
    return ::MulDiv(value, static_cast<int>(dpi), 96);
}

[[nodiscard]] int StrokeWidth(const Shape& shape, unsigned dpi) noexcept {
    const int width = Scale(shape.thickness, dpi);
    return width < 1 ? 1 : width;
}

// Ok başı: gövdenin ucunda, gövdeyle aynı açıda bir üçgen.
void DrawArrow(HDC dc, const Shape& shape, unsigned dpi) {
    const int width = StrokeWidth(shape, dpi);

    const HPEN pen = ::CreatePen(PS_SOLID, width, shape.color);
    const HBRUSH brush = ::CreateSolidBrush(shape.color);
    if (pen == nullptr || brush == nullptr) {
        if (pen != nullptr) { ::DeleteObject(pen); }
        if (brush != nullptr) { ::DeleteObject(brush); }
        return;
    }

    const double dx = static_cast<double>(shape.end.x - shape.start.x);
    const double dy = static_cast<double>(shape.end.y - shape.start.y);
    const double length = std::sqrt(dx * dx + dy * dy);

    // Sıfır uzunluklu ok çizilmez: açısı tanımsız ve kullanıcı zaten bir şey
    // görmek istemiyor.
    if (length < 1.0) {
        ::DeleteObject(pen);
        ::DeleteObject(brush);
        return;
    }

    const double angle = std::atan2(dy, dx);
    // Ok başı çizgi kalınlığıyla ORANTILI: sabit bir baş, kalın çizgide
    // kaybolur, ince çizgide devasa görünür.
    const double head = static_cast<double>(width) * 4.0 + 6.0;
    const double spread = 0.42;   // radyan

    const POINT tip = shape.end;
    const POINT left{
        static_cast<LONG>(tip.x - head * std::cos(angle - spread)),
        static_cast<LONG>(tip.y - head * std::sin(angle - spread))};
    const POINT right{
        static_cast<LONG>(tip.x - head * std::cos(angle + spread)),
        static_cast<LONG>(tip.y - head * std::sin(angle + spread))};

    // Gövde ok başının içine kadar değil, başın DİBİNE kadar çizilir; yoksa
    // sivri ucun ötesine taşar.
    const POINT bodyEnd{
        static_cast<LONG>(tip.x - head * 0.75 * std::cos(angle)),
        static_cast<LONG>(tip.y - head * 0.75 * std::sin(angle))};

    const HGDIOBJ oldPen = ::SelectObject(dc, pen);
    ::MoveToEx(dc, shape.start.x, shape.start.y, nullptr);
    ::LineTo(dc, bodyEnd.x, bodyEnd.y);

    const HGDIOBJ oldBrush = ::SelectObject(dc, brush);
    const POINT head3[3] = {tip, left, right};
    ::Polygon(dc, head3, 3);

    ::SelectObject(dc, oldBrush);
    ::SelectObject(dc, oldPen);
    ::DeleteObject(pen);
    ::DeleteObject(brush);
}

void DrawOutlineShape(HDC dc, const Shape& shape, unsigned dpi, bool ellipse) {
    const int width = StrokeWidth(shape, dpi);
    const HPEN pen = ::CreatePen(PS_SOLID, width, shape.color);
    if (pen == nullptr) {
        return;
    }

    const HGDIOBJ oldPen = ::SelectObject(dc, pen);
    // İçi boş: dolu bir dikdörtgen altındaki ekran görüntüsünü gizlerdi ve
    // işaretlemenin amacı göstermek, örtmek değil.
    const HGDIOBJ oldBrush = ::SelectObject(dc, ::GetStockObject(NULL_BRUSH));

    const RECT bounds = shape.Bounds();
    if (ellipse) {
        ::Ellipse(dc, bounds.left, bounds.top, bounds.right, bounds.bottom);
    } else {
        ::Rectangle(dc, bounds.left, bounds.top, bounds.right, bounds.bottom);
    }

    ::SelectObject(dc, oldBrush);
    ::SelectObject(dc, oldPen);
    ::DeleteObject(pen);
}

void DrawFreehand(HDC dc, const Shape& shape, unsigned dpi) {
    if (shape.points.size() < 2) {
        return;
    }

    const bool highlighter = (shape.kind == ToolKind::Highlighter);
    // Vurgulayıcı kalın ve saydam olmalı. GDI kalemi alfa bilmediği için
    // saydamlık R2_MASKPEN karışım kipiyle taklit edilir: sonuç gerçek alfa
    // kadar iyi değil ama altındaki metin okunur kalır ki vurgulayıcının
    // bütün amacı bu.
    const int width = highlighter ? StrokeWidth(shape, dpi) * 4
                                  : StrokeWidth(shape, dpi);

    const HPEN pen = ::CreatePen(PS_SOLID, width, shape.color);
    if (pen == nullptr) {
        return;
    }
    const HGDIOBJ oldPen = ::SelectObject(dc, pen);
    const int oldRop = ::SetROP2(dc, highlighter ? R2_MASKPEN : R2_COPYPEN);

    ::MoveToEx(dc, shape.points.front().x, shape.points.front().y, nullptr);
    for (size_t i = 1; i < shape.points.size(); ++i) {
        ::LineTo(dc, shape.points[i].x, shape.points[i].y);
    }

    ::SetROP2(dc, oldRop);
    ::SelectObject(dc, oldPen);
    ::DeleteObject(pen);
}

[[nodiscard]] HFONT CreateShapeFont(const Shape& shape, unsigned dpi, bool bold) {
    LOGFONTW font{};
    // Kalınlık ayarı metin aracında punto olarak yorumlanır: kullanıcı tek bir
    // "boyut" düğmesi görür, aracın ne çizdiğine göre anlamı değişir.
    const int points = 8 + shape.thickness * 3;
    font.lfHeight = -::MulDiv(points, static_cast<int>(dpi), 72);
    font.lfWeight = bold ? FW_BOLD : FW_SEMIBOLD;
    font.lfCharSet = DEFAULT_CHARSET;
    font.lfQuality = CLEARTYPE_QUALITY;
    ::wcscpy_s(font.lfFaceName, L"Segoe UI");
    return ::CreateFontIndirectW(&font);
}

void DrawText(HDC dc, const Shape& shape, unsigned dpi) {
    if (shape.text.empty()) {
        return;
    }
    const HFONT font = CreateShapeFont(shape, dpi, false);
    if (font == nullptr) {
        return;
    }
    const HGDIOBJ oldFont = ::SelectObject(dc, font);
    ::SetBkMode(dc, TRANSPARENT);
    ::SetTextColor(dc, shape.color);

    RECT area{shape.start.x, shape.start.y, shape.start.x + 4000,
              shape.start.y + 4000};
    ::DrawTextW(dc, shape.text.c_str(), -1, &area, DT_NOPREFIX | DT_NOCLIP);

    ::SelectObject(dc, oldFont);
    ::DeleteObject(font);
}

void DrawStepNumber(HDC dc, const Shape& shape, unsigned dpi) {
    const int radius = Scale(11 + shape.thickness * 2, dpi);
    const RECT circle{shape.start.x - radius, shape.start.y - radius,
                      shape.start.x + radius, shape.start.y + radius};

    const HBRUSH brush = ::CreateSolidBrush(shape.color);
    const HPEN pen = ::CreatePen(PS_SOLID, Scale(2, dpi), RGB(255, 255, 255));
    if (brush == nullptr || pen == nullptr) {
        if (brush != nullptr) { ::DeleteObject(brush); }
        if (pen != nullptr) { ::DeleteObject(pen); }
        return;
    }

    const HGDIOBJ oldBrush = ::SelectObject(dc, brush);
    const HGDIOBJ oldPen = ::SelectObject(dc, pen);
    ::Ellipse(dc, circle.left, circle.top, circle.right, circle.bottom);
    ::SelectObject(dc, oldPen);
    ::SelectObject(dc, oldBrush);
    ::DeleteObject(pen);
    ::DeleteObject(brush);

    wchar_t label[8];
    ::swprintf_s(label, L"%d", shape.stepNumber);

    const HFONT font = CreateShapeFont(shape, dpi, true);
    if (font == nullptr) {
        return;
    }
    const HGDIOBJ oldFont = ::SelectObject(dc, font);
    ::SetBkMode(dc, TRANSPARENT);
    ::SetTextColor(dc, RGB(255, 255, 255));
    RECT area = circle;
    ::DrawTextW(dc, label, -1, &area,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    ::SelectObject(dc, oldFont);
    ::DeleteObject(font);
}

void DrawOne(HDC dc, const Shape& shape, unsigned dpi) {
    switch (shape.kind) {
        case ToolKind::Arrow:
            DrawArrow(dc, shape, dpi);
            break;
        case ToolKind::Rectangle:
            DrawOutlineShape(dc, shape, dpi, false);
            break;
        case ToolKind::Ellipse:
            DrawOutlineShape(dc, shape, dpi, true);
            break;
        case ToolKind::Pen:
        case ToolKind::Highlighter:
            DrawFreehand(dc, shape, dpi);
            break;
        case ToolKind::Text:
            DrawText(dc, shape, dpi);
            break;
        case ToolKind::StepNumber:
            DrawStepNumber(dc, shape, dpi);
            break;
        default:
            break;   // efektler burada değil, RenderShapes'in ilk geçişinde
    }
}

}  // namespace

void RenderShapes(Image& image, const std::vector<Shape>& shapes, unsigned dpi) {
    if (!image.Valid()) {
        return;
    }

    // 1. GEÇİŞ — efektler. Piksel üzerinde çalışırlar ve üstlerine çizilecek
    // şekillerden ÖNCE uygulanmalıdır; sonra uygulansaydı bulanıklık, üstüne
    // konmuş oku da yerdi.
    for (const Shape& shape : shapes) {
        const RECT bounds = shape.Bounds();
        if (shape.kind == ToolKind::Blur) {
            // Yarıçap alanla ölçeklenir: küçük bir seçimde 20 piksellik
            // bulanıklık her şeyi tek renge indirirdi.
            const LONG side =
                geom::Width(bounds) < geom::Height(bounds) ? geom::Width(bounds)
                                                           : geom::Height(bounds);
            int radius = static_cast<int>(side / 12);
            if (radius < 3) {
                radius = 3;
            }
            BlurRegion(image, bounds, radius);
        } else if (shape.kind == ToolKind::Mosaic) {
            const LONG side =
                geom::Width(bounds) < geom::Height(bounds) ? geom::Width(bounds)
                                                           : geom::Height(bounds);
            int block = static_cast<int>(side / 10);
            if (block < 4) {
                block = 4;
            }
            MosaicRegion(image, bounds, block);
        }
    }

    // 2. GEÇİŞ — çizimler.
    const window_dc screenDc{nullptr};
    if (!screenDc.valid()) {
        return;
    }
    const unique_hdc memory{::CreateCompatibleDC(screenDc.get())};
    if (!memory) {
        return;
    }
    ::SelectObject(memory.get(), image.Handle());
    ::SetBkMode(memory.get(), TRANSPARENT);

    for (const Shape& shape : shapes) {
        DrawOne(memory.get(), shape, dpi);
    }

    // GDI ALFAYI SIFIRLAR: Rectangle, LineTo ve DrawText 32 bpp bir yüzeye
    // yazarken alfa baytına dokunmaz ve DIB'de 0 kalır. Görüntü PNG olarak
    // kaydedildiğinde çizilen her piksel saydam çıkardı.
    auto* pixels = static_cast<uint32_t*>(image.Bits());
    if (pixels != nullptr) {
        const size_t count =
            static_cast<size_t>(image.Width()) * static_cast<size_t>(image.Height());
        for (size_t i = 0; i < count; ++i) {
            pixels[i] |= 0xFF000000u;
        }
    }
}

void RenderPreview(HDC dc, const Shape& shape, unsigned dpi) {
    if (dc == nullptr) {
        return;
    }
    if (ToolIsEffect(shape.kind) || ToolIsImageOp(shape.kind)) {
        // Efektin önizlemesi kesikli bir çerçeve: gerçek bulanıklığı her fare
        // hareketinde hesaplamak büyük seçimlerde gözle görülür şekilde
        // yavaşlar ve kullanıcının görmesi gereken şey alanın sınırı.
        const HPEN pen = ::CreatePen(PS_DOT, 1, RGB(255, 255, 255));
        if (pen == nullptr) {
            return;
        }
        const HGDIOBJ oldPen = ::SelectObject(dc, pen);
        const HGDIOBJ oldBrush = ::SelectObject(dc, ::GetStockObject(NULL_BRUSH));
        const int oldRop = ::SetROP2(dc, R2_XORPEN);
        const RECT bounds = shape.Bounds();
        ::Rectangle(dc, bounds.left, bounds.top, bounds.right, bounds.bottom);
        ::SetROP2(dc, oldRop);
        ::SelectObject(dc, oldBrush);
        ::SelectObject(dc, oldPen);
        ::DeleteObject(pen);
        return;
    }
    DrawOne(dc, shape, dpi);
}

}  // namespace crisp
