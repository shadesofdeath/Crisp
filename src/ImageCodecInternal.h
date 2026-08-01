// ImageCodecInternal.h — Kodlayıcı ve çözücünün paylaştığı WIC kurulumu.
//
// AYRI BAŞLIK: kodlama ve çözme ayrı dosyalarda (ImageCodec.cpp ev kuralının
// 400 satır sınırını aşıyordu, docs §9) ama ikisi de aynı fabrikayı kurar.
#pragma once

#include "Capture.h"
#include "Util.h"

#include <wincodec.h>

namespace crisp {

// WIC fabrikası. COM'un bu iş parçacığında başlatılmış olması gerekir.
[[nodiscard]] bool CreateFactory(ComPtr<IWICImagingFactory>& factory);

// WIC kaynağını 32 bit BGRA'ya çevirip Image'a kopyalar.
[[nodiscard]] bool CopyToImage(IWICImagingFactory* factory,
                               IWICBitmapSource* source, Image& out);

// PNG imzası ve sondaki IEND parçası yerinde mi?
//
// GEREKLİ: WIC yarım bir PNG'yi memnuniyetle çözer ve eksik satırları
// tanımsız bırakır. Kısa okunmuş bir dosya, hata değil YARIM GÖRÜNTÜ olarak
// döner.
[[nodiscard]] bool LooksLikeCompletePng(const uint8_t* data,
                                        size_t size) noexcept;

}  // namespace crisp
