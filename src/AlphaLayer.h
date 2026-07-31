// AlphaLayer.h — Yarı saydam, yuvarlatılmış köşeli çizim katmanı.
//
// NEDEN GDI YETMİYOR: FillRect alfa bilmez, RoundRect kenar yumuşatma yapmaz ve
// her şekil için ayrı bir AlphaBlend çağırmak ekran dolusu metin kutusunda
// yüzlerce blit demek. Burada tek bir önceden-çarpılmış BGRA yüzeye elle
// rasterleştirilir, sonra hepsi TEK AlphaBlend ile ekrana iner.
//
// Kenar yumuşatma köşe yaylarında piksel kapsama oranı hesaplanarak yapılır;
// yumuşatmasız 1 piksellik yuvarlak köşeler tırtıklı görünür ve arayüz ucuz
// durur.
#pragma once

#include "Capture.h"

#include <windows.h>

namespace crisp {

class AlphaLayer {
public:
    AlphaLayer() noexcept = default;

    AlphaLayer(const AlphaLayer&) = delete;
    AlphaLayer& operator=(const AlphaLayer&) = delete;

    // Katmanı verilen boyutta hazırlar. origin, katmanın sol-üst köşesinin
    // hedefteki konumudur; çizim koordinatları bu köken çıkarılarak verilir.
    [[nodiscard]] bool Prepare(HDC referenceDc, POINT origin, int width,
                               int height);

    [[nodiscard]] bool valid() const noexcept { return m_surface.Valid(); }

    // Tamamen saydama döndürür.
    void Clear() noexcept;

    // Dolu yuvarlatılmış dikdörtgen.
    void FillRoundRect(RECT rect, COLORREF color, BYTE alpha, int radius) noexcept;

    // İçi boş yuvarlatılmış dikdörtgen.
    void StrokeRoundRect(RECT rect, COLORREF color, BYTE alpha, int radius,
                         int thickness) noexcept;

    // Katmanı hedefe karıştırır.
    void BlendTo(HDC target) const;

private:
    // Tek piksele kapsama oranıyla renk yazar (üzerine karıştırır).
    void BlendPixel(int x, int y, COLORREF color, float coverage) noexcept;

    // Yuvarlatılmış dikdörtgenin (x,y) pikselindeki kapsama oranı [0,1].
    [[nodiscard]] static float RoundRectCoverage(const RECT& rect, int radius,
                                                 int x, int y) noexcept;

    Image m_surface;
    unique_hdc m_dc;
    POINT m_origin{};
};

}  // namespace crisp
