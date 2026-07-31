// ImageCodec.h — PNG kodlama/çözme, Windows Imaging Component ile.
//
// WIC işletim sisteminin parçasıdır; libpng/zlib gibi bir bağımlılık eklemeye
// gerek yoktur ve tek dosya .exe hedefi korunur.
#pragma once

#include "Capture.h"

#include <cstdint>
#include <string>
#include <vector>

namespace crisp {

// Görüntüyü PNG olarak belleğe kodlar. Pano için gereklidir: pano PNG'yi
// dosyaya uğramadan bayt dizisi olarak ister.
[[nodiscard]] bool EncodePng(const Image& image, std::vector<uint8_t>& out);

// PNG'yi diske yazar. Ara dizinler yoksa oluşturulur.
[[nodiscard]] bool SavePng(const Image& image, const std::wstring& path);

// Bellekteki PNG'yi çözer. Kaynak hangi piksel biçiminde olursa olsun sonuç
// 32 bpp BGRA top-down'dır — Image'ın tek biçimi.
[[nodiscard]] bool DecodePng(const uint8_t* data, size_t size, Image& out);

// Diskteki PNG'yi çözer.
[[nodiscard]] bool LoadPng(const std::wstring& path, Image& out);

}  // namespace crisp
