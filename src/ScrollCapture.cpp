// ScrollCapture.cpp — bkz. ScrollCapture.h.
#include "ScrollCapture.h"

#include "Geometry.h"
#include "Stitch.h"
#include "Util.h"

#include <algorithm>

namespace crisp {
namespace {

// Beklerken mesaj kuyruğunu boşaltır.
//
// DÜZ `Sleep` YETMİYOR. Bu döngü arayüz iş parçacığında çalışıyor ve saniyeler
// sürüyor; o süre boyunca hiçbir mesaj işlenmezse "Kaydırılıyor…" bildirimi
// zamanlayıcısını hiç almaz, opaklığı sıfırda kalır ve GÖRÜNMEZ. Kullanıcı da
// on saniye boyunca hiçbir şey olmuyormuş gibi bekler — tam da yükleme için
// düzeltilen sorun.
//
// Yeni bir yakalama başlatılmasına karşı koruma çağıranda: `m_busy` bu döngü
// boyunca kurulu kalıyor.
void SleepPumping(unsigned milliseconds) {
    const DWORD deadline = ::GetTickCount() + milliseconds;
    for (;;) {
        MSG message{};
        while (::PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            ::TranslateMessage(&message);
            ::DispatchMessageW(&message);
        }
        const DWORD now = ::GetTickCount();
        if (now >= deadline) {
            return;
        }
        const DWORD remaining = deadline - now;
        ::MsgWaitForMultipleObjects(0, nullptr, FALSE, remaining, QS_ALLINPUT);
    }
}

void SendWheel(int notches) noexcept {
    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_WHEEL;
    // Negatif = aşağı. Kaydırmalı yakalama aşağı iner; yukarı çıkan bir sayfa
    // yakalamak isteyen kullanıcı zaten başa gidip aşağı iner.
    input.mi.mouseData = static_cast<DWORD>(-WHEEL_DELTA * notches);
    ::SendInput(1, &input, sizeof(input));
}

}  // namespace

bool CollectScrollFrames(const RECT& region, const ScrollCaptureOptions& options,
                         std::vector<Image>& frames) {
    frames.clear();
    if (geom::IsEmpty(region)) {
        return false;
    }

    const int maxFrames = (std::max)(1, options.maxFrames);
    const int notches = (std::max)(1, options.notchesPerStep);

    POINT restore{};
    const bool haveCursor = ::GetCursorPos(&restore) != FALSE;

    // İMLEÇ BÖLGENİN ORTASINA. Tekerlek olayı imlecin altındaki pencereye
    // gidiyor; kenara koymak, komşu bir panelin kaydırılmasına yol açardı.
    const POINT centre{region.left + geom::Width(region) / 2,
                       region.top + geom::Height(region) / 2};
    ::SetCursorPos(static_cast<int>(centre.x), static_cast<int>(centre.y));

    // İmleç taşındıktan sonra vurgu değişimlerinin oturması için kısa bir
    // bekleme: ilk kare, imlecin altındaki bağlantı henüz renk değiştirmemişken
    // yakalanırsa ikinciyle eşleşmesi zorlaşır.
    SleepPumping(120);

    bool ok = true;
    for (int i = 0; i < maxFrames; ++i) {
        Image frame;
        // İMLEÇ ÇİZİLMEZ. Her karede aynı yerde duran bir imleç, birleştirilmiş
        // görüntüde onlarca kez tekrarlanan bir ok bırakırdı.
        if (!CaptureRect(region, frame, false)) {
            ok = false;
            break;
        }

        // YENİ İÇERİK YOKSA DUR. Sayfanın sonuna gelmiş olabiliriz ya da
        // pencere hiç kaydırılmamış olabilir; ikisinde de devam etmek aynı
        // kareyi tekrar tekrar yakalamak demek.
        if (!frames.empty()) {
            const int shift =
                FindVerticalShift(frames.back(), frame, kScrollOverlapRows);
            if (shift <= 0) {
                LogV(L"Kaydırma durdu: %zu. karede eşleşme yok", frames.size());
                break;
            }
        }

        frames.push_back(std::move(frame));
        if (i + 1 == maxFrames) {
            break;   // son kareden sonra kaydırmanın anlamı yok
        }

        SendWheel(notches);
        SleepPumping(options.settleMs);
    }

    if (haveCursor) {
        ::SetCursorPos(static_cast<int>(restore.x), static_cast<int>(restore.y));
    }

    if (frames.empty()) {
        return false;
    }
    if (!ok) {
        LogV(L"Kaydırmalı yakalama: %zu karede kesildi", frames.size());
    }
    return true;
}

}  // namespace crisp
