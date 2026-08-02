// PinPersist.cpp — Açık iğnelerin diske yazılması ve geri yüklenmesi.
//
// AYRI DOSYA: PinWindow.cpp pencerenin davranışını (sürükleme, tekerlek,
// menü) anlatıyor ve 385 satırdı; ikisi bir arada ev kuralının 400 satır
// sınırını aşardı (docs §9).
//
// GÖRÜNTÜLER PNG OLARAK YAZILIR, kayıt defterine ya da tek bir ikili dosyaya
// değil. Kullanıcı klasörü açıp ne saklandığını görebilir, bir iğneyi elle
// silebilir, ya da klasörün tamamını silip her şeyi unutturabilir — aynı
// gerekçe geçmiş klasörü için de yazılı.
#include "PinWindow.h"

#include "ImageCodec.h"
#include "PinStore.h"
#include "Util.h"

#include <cstdio>
#include <string>
#include <vector>

namespace crisp {

// PinWindow.cpp'deki listeye erişim. Başlıkta değil çünkü `PinState` iç bir
// tür; iki dosya arasında paylaşılan tek şey bu iki işlev.
[[nodiscard]] std::vector<PinRecord> CollectOpenPins();
[[nodiscard]] bool WritePinImage(size_t index, const std::wstring& folder,
                                 std::wstring& fileName);

void SaveOpenPins() {
    // ESKİSİ ÖNCE SİLİNİR, YENİSİ SONRA YAZILIR — VE SIRA BU YÜZDEN ÖNEMLİ.
    //
    // Kullanıcı üç iğneden ikisini kapattıysa, kalan tek iğneyi yazmak yetmez:
    // eski iki PNG orada durur ve klasör her oturumda büyür. Ama temizlik
    // `CollectOpenPins`ten SONRA çalıştırılırsa — ki bir süre öyleydi — az önce
    // yazılmış görüntüleri siler ve geriye yalnızca hiçbir dosyayı işaret
    // etmeyen bir indeks kalır.
    (void)ClearPinStore();

    const std::vector<PinRecord> records = CollectOpenPins();
    if (records.empty()) {
        return;
    }
    if (!WritePinIndex(records)) {
        LogV(L"İğneler kaydedilemedi");
    }
}

int RestorePins(HINSTANCE instance) {
    const std::vector<PinRecord> records = ReadPinIndex();
    if (records.empty()) {
        return 0;
    }

    const std::wstring folder = PinFolder();
    int restored = 0;
    for (const PinRecord& record : records) {
        Image image;
        const std::wstring path = folder + L"\\" + record.imageFile;
        if (!LoadImageFile(path, image) || !image.Valid()) {
            LogV(L"İğne geri yüklenemedi: %s", record.imageFile.c_str());
            continue;
        }
        if (PinImageWithView(instance, image, POINT{record.x, record.y},
                             record.zoom, record.opacity)) {
            ++restored;
        }
    }

    // GERİ YÜKLENENLER DİSKTE KALIR. Uygulama bir daha kapandığında
    // `SaveOpenPins` her şeyi yeniden yazacak; burada silmek, açılıştan hemen
    // sonra çöken bir sürümde iğnelerin kaybolması demek olurdu.
    return restored;
}

}  // namespace crisp
