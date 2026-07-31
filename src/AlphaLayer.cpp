// AlphaLayer.cpp — bkz. AlphaLayer.h.
#include "AlphaLayer.h"

#include "Geometry.h"
#include "Util.h"

#include <cmath>

namespace crisp {
namespace {

[[nodiscard]] float Clamp01(float value) noexcept {
    return value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
}

}  // namespace

bool AlphaLayer::Prepare(HDC referenceDc, POINT origin, int width, int height) {
    if (width <= 0 || height <= 0) {
        return false;
    }

    if (!m_surface.Valid() || m_surface.Width() != width ||
        m_surface.Height() != height) {
        if (!m_surface.Create(width, height)) {
            return false;
        }
        m_dc.reset(::CreateCompatibleDC(referenceDc));
        if (!m_dc) {
            return false;
        }
        ::SelectObject(m_dc.get(), m_surface.Handle());
    }

    m_origin = origin;
    Clear();
    return true;
}

void AlphaLayer::Clear() noexcept {
    if (m_surface.Valid()) {
        m_surface.Fill(0u);   // önceden çarpılmış: alfa 0 → renk de 0
    }
}

void AlphaLayer::BlendPixel(int x, int y, COLORREF color, float coverage) noexcept {
    if (coverage <= 0.0f) {
        return;
    }
    coverage = Clamp01(coverage);

    const uint32_t existing = m_surface.Pixel(x, y);
    const float existingAlpha = static_cast<float>((existing >> 24) & 0xFFu) / 255.0f;

    // "Over" karışımı, önceden çarpılmış uzayda.
    const float sourceAlpha = coverage;
    const float outAlpha = sourceAlpha + existingAlpha * (1.0f - sourceAlpha);
    if (outAlpha <= 0.0f) {
        return;
    }

    auto channel = [&](uint32_t shift, float sourceValue) -> uint32_t {
        const float existingPremultiplied =
            static_cast<float>((existing >> shift) & 0xFFu) / 255.0f;
        const float out = sourceValue * sourceAlpha +
                          existingPremultiplied * (1.0f - sourceAlpha);
        return static_cast<uint32_t>(Clamp01(out) * 255.0f + 0.5f);
    };

    const uint32_t r = channel(16, static_cast<float>(GetRValue(color)) / 255.0f);
    const uint32_t g = channel(8, static_cast<float>(GetGValue(color)) / 255.0f);
    const uint32_t b = channel(0, static_cast<float>(GetBValue(color)) / 255.0f);
    const uint32_t a = static_cast<uint32_t>(Clamp01(outAlpha) * 255.0f + 0.5f);

    m_surface.SetPixel(x, y, (a << 24) | (r << 16) | (g << 8) | b);
}

float AlphaLayer::RoundRectCoverage(const RECT& rect, int radius, int x,
                                    int y) noexcept {
    // Piksel merkezleri kullanılır; kenarların yarım piksel kayması, kutunun
    // bir tarafının diğerinden kalın görünmesine yol açardı.
    const float px = static_cast<float>(x) + 0.5f;
    const float py = static_cast<float>(y) + 0.5f;

    const float left = static_cast<float>(rect.left);
    const float top = static_cast<float>(rect.top);
    const float right = static_cast<float>(rect.right);
    const float bottom = static_cast<float>(rect.bottom);
    const float r = static_cast<float>(radius);

    if (px < left || px > right || py < top || py > bottom) {
        return 0.0f;
    }
    if (r <= 0.0f) {
        return 1.0f;
    }

    // Hangi köşe bölgesindeyiz? Değilsek tam kapsama.
    float cx = 0.0f;
    float cy = 0.0f;
    if (px < left + r && py < top + r) {
        cx = left + r;
        cy = top + r;
    } else if (px > right - r && py < top + r) {
        cx = right - r;
        cy = top + r;
    } else if (px < left + r && py > bottom - r) {
        cx = left + r;
        cy = bottom - r;
    } else if (px > right - r && py > bottom - r) {
        cx = right - r;
        cy = bottom - r;
    } else {
        return 1.0f;
    }

    const float dx = px - cx;
    const float dy = py - cy;
    const float distance = std::sqrt(dx * dx + dy * dy);

    // Yarıçapın bir piksel içi/dışı arasında doğrusal geçiş: ucuz ama gözle
    // yeterli bir kenar yumuşatma.
    return Clamp01(r + 0.5f - distance);
}

void AlphaLayer::FillRoundRect(RECT rect, COLORREF color, BYTE alpha,
                               int radius) noexcept {
    if (!m_surface.Valid() || alpha == 0) {
        return;
    }

    // Katman koordinatına çevir.
    ::OffsetRect(&rect, -m_origin.x, -m_origin.y);

    const RECT clip{0, 0, m_surface.Width(), m_surface.Height()};
    const RECT area = geom::ClampTo(rect, clip);
    if (geom::IsEmpty(area)) {
        return;
    }

    const int maxRadius =
        static_cast<int>((geom::Width(rect) < geom::Height(rect) ? geom::Width(rect)
                                                                 : geom::Height(rect)) /
                         2);
    if (radius > maxRadius) {
        radius = maxRadius;
    }

    const float scale = static_cast<float>(alpha) / 255.0f;
    for (int y = area.top; y < area.bottom; ++y) {
        for (int x = area.left; x < area.right; ++x) {
            const float coverage = RoundRectCoverage(rect, radius, x, y);
            BlendPixel(x, y, color, coverage * scale);
        }
    }
}

void AlphaLayer::StrokeRoundRect(RECT rect, COLORREF color, BYTE alpha,
                                 int radius, int thickness) noexcept {
    if (!m_surface.Valid() || alpha == 0 || thickness <= 0) {
        return;
    }

    ::OffsetRect(&rect, -m_origin.x, -m_origin.y);

    RECT inner = rect;
    ::InflateRect(&inner, -thickness, -thickness);
    const int innerRadius = radius - thickness > 0 ? radius - thickness : 0;

    const RECT clip{0, 0, m_surface.Width(), m_surface.Height()};
    const RECT area = geom::ClampTo(rect, clip);
    if (geom::IsEmpty(area)) {
        return;
    }

    int outerRadius = radius;
    const int maxRadius =
        static_cast<int>((geom::Width(rect) < geom::Height(rect) ? geom::Width(rect)
                                                                 : geom::Height(rect)) /
                         2);
    if (outerRadius > maxRadius) {
        outerRadius = maxRadius;
    }

    const float scale = static_cast<float>(alpha) / 255.0f;
    for (int y = area.top; y < area.bottom; ++y) {
        for (int x = area.left; x < area.right; ++x) {
            // Çerçeve = dış kapsama − iç kapsama. İki kenarı ayrı ayrı
            // çizmek köşelerde çift boyama ve koyu noktalar üretirdi.
            const float outer = RoundRectCoverage(rect, outerRadius, x, y);
            const float innerCoverage =
                geom::IsEmpty(inner) ? 0.0f
                                     : RoundRectCoverage(inner, innerRadius, x, y);
            BlendPixel(x, y, color, (outer - innerCoverage) * scale);
        }
    }
}

void AlphaLayer::BlendTo(HDC target) const {
    if (!m_surface.Valid() || !m_dc) {
        return;
    }

    BLENDFUNCTION blend{};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;

    ::AlphaBlend(target, m_origin.x, m_origin.y, m_surface.Width(),
                 m_surface.Height(), m_dc.get(), 0, 0, m_surface.Width(),
                 m_surface.Height(), blend);
}

}  // namespace crisp
