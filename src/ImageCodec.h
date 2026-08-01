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

enum class ImageFormat {
    Png,
    Jpeg,
    WebP,
};

// Görüntüyü PNG olarak belleğe kodlar. Pano için gereklidir: pano PNG'yi
// dosyaya uğramadan bayt dizisi olarak ister.
[[nodiscard]] bool EncodePng(const Image& image, std::vector<uint8_t>& out);

// PNG'yi diske yazar. Ara dizinler yoksa oluşturulur.
[[nodiscard]] bool SavePng(const Image& image, const std::wstring& path);

// Belirtilen biçimde diske yazar.
// quality yalnızca JPEG ve WebP için anlamlıdır: 1..100.
[[nodiscard]] bool SaveImage(const Image& image, const std::wstring& path,
                             ImageFormat format, unsigned quality);

// Bu makinede biçim için bir kodlayıcı var mı?
//
// WEBP İÇİN GEREKLİ: Windows PNG ve JPEG kodlayıcılarını her zaman taşır ama
// WebP kodlayıcısı isteğe bağlı bir bileşendir ve birçok kurulumda yoktur.
// Ayarlarda seçilebilir bırakıp kaydetme anında başarısız olmak, kullanıcının
// yakalamasını kaybetmesi demek olurdu.
[[nodiscard]] bool IsFormatAvailable(ImageFormat format);

// Uzantıdan biçim; bilinmeyen uzantı PNG'ye düşer.
[[nodiscard]] ImageFormat FormatFromPath(const std::wstring& path) noexcept;

// Biçimin dosya uzantısı, noktasız ("png", "jpg", "webp").
[[nodiscard]] const wchar_t* ExtensionForFormat(ImageFormat format) noexcept;

// Ayar dizesi ⇄ biçim.
[[nodiscard]] ImageFormat FormatFromString(const wchar_t* value) noexcept;

// "<klasör>\<alt klasör>\<ad>.<uzantı>" — çakışırsa " (2)" ekler ve alt
// klasörü oluşturur.
//
// ŞABLONLAR BURADA ÇÖZÜLMEZ: `subFolder` ve `baseName` zaten çözülmüş ve
// temizlenmiş dizelerdir (bkz. NameFormat.h). Kodlayıcının belirteç bilmesi
// gerekmez ve bilmemesi, ikisini ayrı ayrı sınanabilir tutar.
//
// BİÇİM YOKSA PNG'YE DÜŞÜLÜR ve `format` güncellenir: Windows PNG ve JPEG
// kodlayıcılarını her zaman taşır ama WebP kodlayıcısı isteğe bağlı bir
// bileşendir. Seçili biçimde ısrar etmek, kullanıcının yakalamasını
// kaybetmesi demek olurdu — kaydedilmiş bir PNG, kaydedilmemiş bir WebP'den
// her zaman iyidir.
//
// TEK YERDE: hem yakalama akışı hem düzenleyici kaydediyor ve iki ayrı kopya,
// birinde biçim yedeği olup diğerinde olmaması demekti.
[[nodiscard]] std::wstring BuildCapturePath(const std::wstring& folder,
                                            const std::wstring& subFolder,
                                            const std::wstring& baseName,
                                            ImageFormat& format);

// Bellekteki PNG'yi çözer. Kaynak hangi piksel biçiminde olursa olsun sonuç
// 32 bpp BGRA top-down'dır — Image'ın tek biçimi.
[[nodiscard]] bool DecodePng(const uint8_t* data, size_t size, Image& out);

// Diskteki PNG'yi çözer.
[[nodiscard]] bool LoadPng(const std::wstring& path, Image& out);

// Diskteki HERHANGİ bir görüntüyü çözer: WIC'in tanıdığı her biçim (PNG, JPEG,
// BMP, GIF, TIFF, HEIF, WebP...).
//
// LoadPng'DEN AYRI DURUYOR ÇÜNKÜ İŞLERİ FARKLI: LoadPng, geçmişten okurken
// kendi yazdığımız dosyayı okur ve eksik/bozuk bir PNG'yi kabul etmemesi
// gerekir. Bu ise kullanıcının sürükleyip bıraktığı ya da komut satırında
// verdiği rastgele bir dosyayı açar; orada biçim önceden bilinmez.
[[nodiscard]] bool LoadImageFile(const std::wstring& path, Image& out);

}  // namespace crisp
