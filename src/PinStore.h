// PinStore.h — Açık iğnelerin diskteki kaydı.
//
// İĞNELER ÇIKIŞTA YOK OLUYORDU. Bir şeyi yan yana karşılaştırmak için ekrana
// iğnelemek, sonra bir güncelleme ya da yeniden başlatma gelince hepsini
// kaybetmek demekti — ve iğnelenen görüntü hiçbir yere kaydedilmediği için
// geri getirmenin bir yolu da yoktu.
//
// İNDEKS VAR, ÇÜNKÜ KONUM VAR. Geçmiş klasörü bilinçli olarak indekssiz: orada
// tutulan tek şey görüntünün kendisi ve sıralama dosya tarihinden çıkıyor. Bir
// iğnenin ise nerede durduğu, ne kadar yakınlaştırıldığı ve ne kadar saydam
// olduğu da geri yüklenmeli; bunlar PNG'nin içinde değil.
//
// BİÇİM DÜZ METİN, sekmeyle ayrılmış — uploads.txt ile aynı gerekçe: elle
// silinebilir, elle okunabilir, ve bozulduğunda tek bir satır kaybedilir.
#pragma once

#include <string>
#include <vector>

#include <windows.h>

namespace crisp {

// Diske yazılmış tek bir iğne.
struct PinRecord {
    std::wstring imageFile;   // klasöre göreli dosya adı
    LONG x = 0;
    LONG y = 0;
    int zoom = 100;           // yüzde
    unsigned opacity = 255;   // 0-255
};

// İğnelerin klasörü: %LOCALAPPDATA%\Crisp\Pins
[[nodiscard]] std::wstring PinFolder();

// İndeks dosyasının tam yolu.
[[nodiscard]] std::wstring PinIndexPath();

// İndeksi okur. Dosya yoksa boş liste döner — hata değil, iğne yok demek.
[[nodiscard]] std::vector<PinRecord> ReadPinIndex();

// İndeksi yazar. Boş liste indeksi siler.
[[nodiscard]] bool WritePinIndex(const std::vector<PinRecord>& records);

// Klasördeki her şeyi siler: hem indeks hem görüntüler.
bool ClearPinStore() noexcept;

// --- Satır biçimi, dosyaya dokunmadan sınanabilsin diye açıkta ---------------

[[nodiscard]] std::wstring FormatPinLine(const PinRecord& record);

// Satırı çözer. Bozuk satır false döner ve atlanır; bir iğnenin kaybı,
// dosyanın tamamının reddedilmesinden iyidir.
[[nodiscard]] bool ParsePinLine(const std::wstring& line, PinRecord& out);

}  // namespace crisp
