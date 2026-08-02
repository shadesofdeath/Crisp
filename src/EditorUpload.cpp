// EditorUpload.cpp — Görüntüyü bir servise gönderir ve dönen bağlantıyı verir.
//
// AYRI DOSYA ÇÜNKÜ AYRI BİR ZAMAN ÇİZELGESİ. Düzenleyicideki diğer her eylem
// anında biter: kopyala, kaydet, geri al. Yükleme saniyeler sürer, başarısız
// olabilir, ve o sırada pencere yaşamaya devam etmeli. Bu dosyanın tamamı o
// farkı yönetmekle ilgili.
//
// İŞ PARÇACIĞI KURALI: ağ isteği arka planda, arayüze dokunan her şey pencere
// iş parçacığında. Aradaki tek köprü `PostMessageW` — sonuç yığında ayrılıp
// mesajla geçirilir ve alan taraf onu siler.
#include "EditorInternal.h"

#include "ClipboardImage.h"
#include "ImageCodec.h"
#include "Localization.h"
#include "Upload.h"
#include "Messages.h"
#include "Util.h"
#include "resource.h"

#include <memory>
#include <string>
#include <thread>
#include <utility>

namespace crisp {
namespace editor {
namespace {

// Arka plandan pencereye dönen sonuç. `lParam` bir `Payload*` taşır ve alan
// taraf sahipliği devralır.
struct Payload {
    bool ok = false;
    std::wstring text;   // başarıda bağlantı, başarısızlıkta hata cümlesi
};

}  // namespace

void BeginUpload(HWND window, State& state) {
    const UploadService service = UploadServiceFromId(state.settings.uploadService);
    if (service == UploadService::None) {
        // Düğme zaten kapalı olmalıydı; buraya klavye kısayoluyla gelinebilir.
        ShowFlash(window, state, Loc::Str(IDS_SET_UPLOAD_HINT));
        return;
    }

    // AYNI ANDA TEK YÜKLEME. İkinci bir tıklama ikinci bir isteği başlatırdı ve
    // hangi bağlantının panoya düştüğü sıraya bağlı olurdu.
    if (state.uploading) {
        return;
    }

    Image flattened;
    if (!CurrentFlattened(state, flattened)) {
        return;
    }

    // PNG KODLAMA BURADA, İŞ PARÇACIĞINDAN ÖNCE. `Image` bir GDI nesnesi tutuyor
    // ve onu başka bir iş parçacığına vermek, ömrünü iki tarafa birden
    // bağlardı; baytlar ise sahipsiz bir tampon.
    auto png = std::make_shared<std::vector<uint8_t>>();
    if (!EncodePng(flattened, *png)) {
        LogV(L"Yükleme: PNG kodlanamadı");
        return;
    }

    state.uploading = true;
    ShowFlash(window, state, Loc::Str(IDS_UPLOAD_WORKING));

    const std::wstring key = state.settings.uploadApiKey;

    // AYRILMIŞ İŞ PARÇACIĞI, ÇÜNKÜ KİMSE ONU BEKLEMİYOR. Pencere yükleme
    // biterken kapanmış olabilir; o durumda PostMessageW başarısız olur, yük
    // burada silinir ve iş parçacığı sessizce biter.
    std::thread([window, service, key, png]() {
        const UploadResult result = UploadPng(service, key, *png, L"crisp.png");

        auto payload = std::make_unique<Payload>();
        payload->ok = result.ok;
        payload->text = result.ok ? result.link : result.error;

        if (::PostMessageW(window, WM_CRISP_UPLOAD_DONE, 0,
                           reinterpret_cast<LPARAM>(payload.get())) != FALSE) {
            (void)payload.release();   // sahiplik pencereye geçti
        }
    }).detach();
}

void FinishUpload(HWND window, State& state, LPARAM lParam) {
    const std::unique_ptr<Payload> payload(reinterpret_cast<Payload*>(lParam));
    state.uploading = false;
    if (!payload) {
        return;
    }

    if (!payload->ok) {
        ShowFlash(window, state, Loc::Str(IDS_UPLOAD_FAILED) + L" — " + payload->text);
        LogV(L"Yükleme başarısız: %s", payload->text.c_str());
        return;
    }

    // BAĞLANTI PANOYA GİDER, TARAYICIYA DEĞİL. Yükledikten sonra sayfayı açmak,
    // kullanıcının istemediği bir pencere açmak demek; adresi yapıştırmak ise
    // zaten yapacağı ilk şey.
    if (!CopyTextToClipboard(payload->text.c_str(), window)) {
        ShowFlash(window, state, payload->text);
        return;
    }
    ShowFlash(window, state, Loc::Str(IDS_UPLOAD_COPIED) + L" — " + payload->text);
}

}  // namespace editor
}  // namespace crisp
