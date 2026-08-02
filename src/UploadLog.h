// UploadLog.h — Yüklenen bağlantıların kaydı.
//
// NEDEN AYRI BİR KAYIT, GEÇMİŞİN İÇİNDE DEĞİL: görüntü geçmişi bilerek "indekssiz,
// veritabansız, düz bir PNG klasörü" (bkz. History.h) ve o özelliğin bedeli
// ödenmiş — kullanıcı Gezgin'den bir dosyayı silince geçmişten de silinmiş
// oluyor, senkronize edilecek bir kayıt yok. Bağlantıyı oraya iliştirmek ya bir
// indeks eklemeyi ya PNG'nin içine meta veri yazmayı gerektirirdi; ikisi de o
// özelliği bozar.
//
// Bağlantılar zaten görüntüye değil, ZAMANA bağlı bir liste: "az önce
// yüklediğimin adresi neydi". Kendi dosyasında, düz metin, satır başına bir
// kayıt.
//
// DÜZ METİN, ÇÜNKÜ AYAR DOSYASI DA ÖYLE. Kullanıcı dosyayı açıp okuyabilmeli,
// bir satırı silebilmeli, hepsini seçip başka yere yapıştırabilmeli. Bir
// veritabanı bunların hiçbirini kolaylaştırmazdı.
#pragma once

#include <string>
#include <vector>

namespace crisp {

// Tek bir yükleme.
struct UploadRecord {
    std::wstring link;
    std::wstring service;   // servis kimliği; "catbox" gibi
    long long when = 0;     // Unix saniyesi, UTC
};

// Kayıt dosyasının yolu: %LOCALAPPDATA%\Crisp\uploads.txt
[[nodiscard]] std::wstring UploadLogPath();

// Kaydı sona ekler ve dosyayı en yeni `limit` satıra budar.
//
// BAŞARISIZLIK SESSİZDİR VE BU BİLİNÇLİ: yükleme başarılı olmuşken, defteri
// tutamadık diye kullanıcıya hata göstermek, işe yaramış bir işlemi başarısız
// gibi gösterirdi. Bağlantı zaten panoda.
bool AppendUploadRecord(const UploadRecord& record, size_t limit = 50);

// En yeniden eskiye doğru okur. Dosya yoksa boş liste döner.
[[nodiscard]] std::vector<UploadRecord> ReadUploadLog(size_t limit = 50);

// Kaydı siler. Dosya yoksa da başarı sayılır.
bool ClearUploadLog();

// --- Ayrıştırma; dosya sistemi olmadan sınanabilir ------------------------

// Bir satırı kayda çevirir. Bozuk satır `false` döner ve atlanır: elle
// düzenlenebilir bir dosyada bozuk bir satır beklenen bir durumdur, hata değil.
[[nodiscard]] bool ParseUploadLine(const std::wstring& line, UploadRecord& out);

// Kaydı bir satıra çevirir. Sekmeyle ayrılır; bağlantıda ve servis adında sekme
// bulunamaz, dolayısıyla kaçış gerekmiyor.
[[nodiscard]] std::wstring FormatUploadLine(const UploadRecord& record);

}  // namespace crisp
