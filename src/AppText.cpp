// AppText.cpp — Metin tanıma ve renk seçme akışları.
//
// AYRI DOSYA: yakalama akışlarıyla aynı dosyada AppCapture.cpp ev kuralının
// 400 satır sınırını aşıyordu (docs §9). Ayrım işlevsel — buradaki üç akış da
// GÖRÜNTÜ değil METİN ya da RENK üretir; hiçbiri düzenleyiciden, geçmişten
// veya kaydetmeden geçmez.
#include "App.h"

#include "ClipboardImage.h"
#include "Localization.h"
#include "MessageWindow.h"
#include "Ocr.h"
#include "Overlay.h"
#include "Util.h"
#include "resource.h"

#include <string>

namespace crisp {
namespace {

// Kaplama açılmadan önce kısa bir bekleme; gerekçesi AppCapture.cpp'de.
constexpr DWORD kMenuSettleMs = 120;

}  // namespace

void App::SelectTextOnScreen() {
    if (m_busy) {
        return;
    }
    m_busy = true;
    ::Sleep(kMenuSettleMs);

    // Ekran ÖNCE dondurulur ve OCR kaplama açılmadan çalıştırılır. Tanıma tam
    // ekranda saniye sürebiliyor; pencereyi açıp sonra taramak, kullanıcının
    // donmuş bir arayüzle karşılaşması demek olurdu.
    Image frozen;
    if (!CaptureRect(VirtualScreenRect(), frozen)) {
        m_busy = false;
        return;
    }

    OcrLayout layout;
    const HCURSOR previousCursor = ::SetCursor(::LoadCursorW(nullptr, IDC_WAIT));
    const bool recognized = RecognizeLayout(frozen, layout);
    ::SetCursor(previousCursor);

    if (!recognized) {
        m_busy = false;
        ShowMessage(m_instance, m_window, Loc::Str(IDS_OCR_UNAVAILABLE),
                    MessageIcon::Warning);
        return;
    }

    if (layout.empty()) {
        m_busy = false;
        ShowMessage(m_instance, m_window, Loc::Str(IDS_OCR_NO_TEXT_SCREEN),
                    MessageIcon::Information);
        return;
    }

    const OverlayResult result =
        RunSelectionOverlay(m_instance, m_settings, OverlayMode::TextSelect,
                            false, frozen, &layout);
    m_busy = false;

    if (!result.accepted || result.pickedText.empty()) {
        return;
    }
    if (!CopyTextToClipboard(result.pickedText.c_str(), m_window)) {
        LogV(L"Seçilen metin panoya kopyalanamadı");
    }
}

void App::CaptureTextToClipboard() {
    if (m_busy) {
        return;
    }
    m_busy = true;
    ::Sleep(kMenuSettleMs);

    Image capture;
    POINT origin{};
    const bool captured =
        RunRegionCapture(m_instance, m_settings, false, capture, origin);
    m_busy = false;

    if (!captured) {
        return;
    }

    std::wstring text;
    if (!RecognizeText(capture, text)) {
        ShowMessage(m_instance, m_window, Loc::Str(IDS_OCR_UNAVAILABLE),
                    MessageIcon::Warning);
        return;
    }

    if (text.empty()) {
        // Boş sonuç bir hata değil: kullanıcı metin içermeyen bir alan seçmiş
        // olabilir. Panoyu boş metinle EZMEK ise veri kaybı olurdu.
        ShowMessage(m_instance, m_window, Loc::Str(IDS_OCR_NO_TEXT_REGION),
                    MessageIcon::Information);
        return;
    }

    if (!CopyTextToClipboard(text.c_str(), m_window)) {
        LogV(L"OCR metni panoya kopyalanamadı");
    }
}

void App::PickColorToClipboard() {
    if (m_busy) {
        return;
    }
    m_busy = true;
    ::Sleep(kMenuSettleMs);

    Image frozen;
    const OverlayResult result = RunSelectionOverlay(
        m_instance, m_settings, OverlayMode::ColorPick, false, frozen);
    m_busy = false;

    if (!result.accepted) {
        return;
    }

    const uint32_t color = result.pickedColor;
    wchar_t hex[16];
    ::swprintf_s(hex, L"#%02X%02X%02X", (color >> 16) & 0xFFu,
                 (color >> 8) & 0xFFu, color & 0xFFu);

    if (!CopyTextToClipboard(hex, m_window)) {
        LogV(L"Renk panoya kopyalanamadı");
    }
}


}  // namespace crisp
