// AppUpload.cpp — Yakalamadan sonra kendiliğinden başlayan yükleme.
//
// AYRI DOSYA: AppActions.cpp 475 satıra çıkmıştı ve ev kuralı 400 (docs §9).
// Ayrım keyfi değil — buradaki iki işlev tek bir şeyi anlatıyor: ağ arka
// planda, arayüze dokunan her şey pencere iş parçacığında, ve aradaki tek
// köprü bir pencere mesajı. Aynı kural düzenleyicideki yükleme için de
// geçerli ve o da kendi dosyasında (EditorUpload.cpp).
#include "App.h"

#include "ClipboardImage.h"
#include "ImageCodec.h"
#include "Localization.h"
#include "Messages.h"
#include "Toast.h"
#include "Upload.h"
#include "UploadLog.h"
#include "UploadText.h"
#include "Util.h"
#include "resource.h"

#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace crisp {
namespace {

// Arka plandaki yüklemeden pencereye dönen sonuç. `WM_CRISP_UPLOAD_TOAST`in
// `lParam`ı bunun adresidir ve alan taraf sahipliği devralır.
//
// CÜMLE BURADA HAZIR: `UploadErrorText` salt okunur bir kaynak tablosuna bakar,
// hangi iş parçacığından çağrıldığı önemli değil, ve hazır bir metin taşımak
// alan tarafını hata kodlarını yeniden yorumlamaktan kurtarıyor.
struct UploadToast {
    bool ok = false;
    std::wstring text;   // başarıda bağlantı, başarısızlıkta hata cümlesi
};

}  // namespace

void App::UploadInBackground(const Image& image) {
    const UploadService service = UploadServiceFromId(m_settings.uploadService);
    if (service == UploadService::None) {
        // Kutu işaretli ama servis seçilmemiş. Sessizce geçmek doğru: ayarlar
        // penceresi zaten servis seçilmeden hiçbir şeyin yüklenmeyeceğini
        // yazıyor ve her yakalamada bir hata kutusu açmak cezalandırmak olurdu.
        return;
    }

    // PNG BURADA KODLANIR, İŞ PARÇACIĞINDAN ÖNCE. `Image` bir GDI nesnesi
    // tutuyor; ömrünü iki iş parçacığına birden bağlamak, yakalama akışı bitip
    // görüntü yok edildiğinde çöken bir yarış olurdu. Baytların sahibi yok.
    auto png = std::make_shared<std::vector<uint8_t>>();
    if (!EncodePng(image, *png)) {
        LogV(L"Otomatik yükleme: PNG kodlanamadı");
        return;
    }

    // SÜRDÜĞÜNÜ SÖYLE. Yükleme servise ve dosya boyutuna göre yirmi saniyeyi
    // bulabiliyor ve o süre boyunca ekranda hiçbir şey yoktu: kullanıcının
    // gördüğü, yakalamanın bildirimi sönüyor ve sonra uzun bir sessizlik.
    // İşin sürdüğü ile unutulduğu aynı görünüyordu. Bu bildirim sönmez ve
    // saniyeleri sayar; yerini sonucu bildiren bildirim alır.
    m_uploadPending = true;
    if (m_settings.showNotification) {
        ShowProgressToast(m_instance, image, Loc::Str(IDS_UPLOAD_WORKING),
                          UploadServiceOf(service).displayName);
    }

    // AYRILMIŞ İŞ PARÇACIĞI: yakalama akışı burada bitiyor ve kullanıcı bir
    // saniyeliğine donmuş bir tepsi uygulaması görmemeli. Yükleme kendi hızında
    // biter ve sonucu bir mesajla geri gönderir.
    const HWND window = m_window;
    const std::wstring key = m_settings.uploadApiKey;

    std::thread([window, service, key, png]() {
        const UploadResult result = UploadPng(service, key, *png, L"crisp.png");

        // Defter BURADA yazılır: bir dosyaya satır eklemek arayüze dokunmuyor
        // ve yükleme bitmişse kayıt da bitmiştir.
        if (result.ok) {
            UploadRecord record;
            record.link = result.link;
            record.service = UploadServiceId(service);
            (void)AppendUploadRecord(record);
        } else {
            LogV(L"Otomatik yükleme başarısız: kod %u, durum %u",
                 static_cast<unsigned>(result.error), result.status);
        }

        auto payload = std::make_unique<UploadToast>();
        payload->ok = result.ok;
        payload->text = result.ok ? result.link : UploadErrorText(result);

        if (::PostMessageW(window, WM_CRISP_UPLOAD_TOAST, 0,
                           reinterpret_cast<LPARAM>(payload.get())) != FALSE) {
            (void)payload.release();   // sahiplik pencereye geçti
        }
    }).detach();
}

void App::FinishBackgroundUpload(LPARAM lParam) {
    const std::unique_ptr<UploadToast> payload(
        reinterpret_cast<UploadToast*>(lParam));
    m_uploadPending = false;
    if (!payload) {
        return;
    }

    // PANO BURADA YAZILIR, İŞ PARÇACIĞINDA DEĞİL. `OpenClipboard` verilen
    // pencerenin ÇAĞIRAN İŞ PARÇACIĞINA ait olmasını ister; yükleme iş
    // parçacığından çağrıldığında sessizce başarısız oluyordu ve kullanıcı
    // "kopyalandı" diyen bir bildirimle boş bir panoya kalıyordu.
    if (payload->ok && !CopyTextToClipboard(payload->text.c_str(), m_window)) {
        LogV(L"Yükleme bağlantısı panoya kopyalanamadı");
    }

    if (!m_settings.showNotification) {
        return;
    }

    // BİLDİRİM YÜKLEME BİTİNCE ÇIKAR, yakalanınca değil. "Bağlantı kopyalandı"
    // diyen bir bildirimin ardından sessizce başarısız olan bir yükleme,
    // kullanıcıya panosunda olmayan bir bağlantıyı yapıştırtırdı.
    Image none;
    ShowCaptureToast(m_instance, none,
                     Loc::Str(payload->ok ? IDS_UPLOAD_COPIED : IDS_UPLOAD_FAILED),
                     payload->text, std::wstring());
}

}  // namespace crisp
