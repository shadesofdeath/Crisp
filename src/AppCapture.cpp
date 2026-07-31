// AppCapture.cpp — Yakalama akışları ve yakalama sonrası eylemler.
#include "App.h"

#include "ClipboardImage.h"
#include "Geometry.h"
#include "ImageCodec.h"
#include "Localization.h"
#include "Messages.h"
#include "Ocr.h"
#include "Overlay.h"
#include "PinWindow.h"
#include "Util.h"
#include "WindowPick.h"
#include "resource.h"

#include <shellapi.h>

#include <string>

namespace crisp {
namespace {

// Kaplama açılmadan önce kısa bir bekleme. Kullanıcı tepsi menüsünden
// tetiklediyse menü daha kapanmamış olabilir ve dondurulmuş ekranda menünün
// kendisi görünürdü.
constexpr DWORD kMenuSettleMs = 120;

void FlashSaved(const std::wstring& path) {
    // Kayıt yerini sessizce bildirmenin en ucuz yolu: hata ayıklama günlüğü.
    // Bir bildirim balonu, her yakalamada kullanıcıyı rahatsız ederdi.
    LogV(L"Kaydedildi: %s", path.c_str());
}

// Kaplamayı çalıştırıp seçili alanı kırpar. Kullanıcı iptal ederse ya da
// bir şey ters giderse false döner ve `origin` dokunulmaz.
[[nodiscard]] bool RunRegionCapture(HINSTANCE instance, const Settings& settings,
                                    bool preferWindowPick, Image& out,
                                    POINT& origin) {
    Image frozen;
    const OverlayResult result = RunSelectionOverlay(
        instance, settings, OverlayMode::Region, preferWindowPick, frozen);

    if (!result.accepted || !frozen.Valid()) {
        return false;
    }

    // Kaplama ekran koordinatı döndürür; dondurulmuş görüntü sanal ekranın
    // sol-üst köşesinden başlar, bu yüzden köken çıkarılmalı.
    const RECT screen = VirtualScreenRect();
    const int x = static_cast<int>(result.selection.left - screen.left);
    const int y = static_cast<int>(result.selection.top - screen.top);
    const int width = static_cast<int>(geom::Width(result.selection));
    const int height = static_cast<int>(geom::Height(result.selection));

    if (!CropImage(frozen, x, y, width, height, out)) {
        LogV(L"Seçim kırpılamadı (%d,%d %dx%d)", x, y, width, height);
        return false;
    }

    origin = POINT{result.selection.left, result.selection.top};
    return true;
}

}  // namespace

void App::StartCapture(CaptureMode mode) {
    if (m_busy) {
        return;   // geri sayım sürüyor ya da kaplama zaten açık
    }

    switch (mode) {
        case CaptureMode::FullScreen:
            m_busy = true;
            ::Sleep(kMenuSettleMs);
            CaptureCurrentMonitor();
            m_busy = false;
            return;

        case CaptureMode::Delayed: {
            m_busy = true;
            const UINT delay = m_settings.delaySeconds * 1000u;
            if (::SetTimer(m_window, TIMER_DELAY, delay, nullptr) == 0) {
                LogV(L"Gecikme zamanlayıcısı kurulamadı");
                m_busy = false;
            }
            return;
        }

        case CaptureMode::Window:
        case CaptureMode::Region:
        default:
            m_busy = true;
            ::Sleep(kMenuSettleMs);
            CaptureRegionOrWindow(mode == CaptureMode::Window);
            m_busy = false;
            return;
    }
}

void App::CaptureRegionOrWindow(bool preferWindowPick) {
    Image capture;
    POINT origin{};
    if (!RunRegionCapture(m_instance, m_settings, preferWindowPick, capture,
                          origin)) {
        return;
    }
    DeliverCapture(capture, origin);
}

void App::CaptureCurrentMonitor() {
    const RECT monitor = MonitorRectAtCursor();
    Image capture;
    if (!CaptureRect(monitor, capture)) {
        LogV(L"Tam ekran yakalama başarısız");
        return;
    }
    DeliverCapture(capture, POINT{monitor.left, monitor.top});
}

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
        ::MessageBoxW(nullptr, Loc::Str(IDS_OCR_UNAVAILABLE).c_str(),
                      Loc::Str(IDS_APP_TITLE).c_str(), MB_OK | MB_ICONWARNING);
        return;
    }

    if (layout.empty()) {
        m_busy = false;
        ::MessageBoxW(nullptr, Loc::Str(IDS_OCR_NO_TEXT_SCREEN).c_str(),
                      Loc::Str(IDS_APP_TITLE).c_str(), MB_OK | MB_ICONINFORMATION);
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
        ::MessageBoxW(nullptr, Loc::Str(IDS_OCR_UNAVAILABLE).c_str(),
                      Loc::Str(IDS_APP_TITLE).c_str(), MB_OK | MB_ICONWARNING);
        return;
    }

    if (text.empty()) {
        // Boş sonuç bir hata değil: kullanıcı metin içermeyen bir alan seçmiş
        // olabilir. Panoyu boş metinle EZMEK ise veri kaybı olurdu.
        ::MessageBoxW(nullptr, Loc::Str(IDS_OCR_NO_TEXT_REGION).c_str(),
                      Loc::Str(IDS_APP_TITLE).c_str(), MB_OK | MB_ICONINFORMATION);
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

bool App::SaveCapture(const Image& image, std::wstring& savedPath) {
    std::wstring folder = m_settings.EffectiveSaveFolder();
    if (folder.empty()) {
        return false;
    }

    ImageFormat format = FormatFromString(m_settings.saveFormat.c_str());

    // BİÇİM YOKSA PNG'YE DÜŞ. Windows PNG ve JPEG kodlayıcılarını her zaman
    // taşır ama WebP kodlayıcısı isteğe bağlı bir bileşendir ve çoğu kurulumda
    // yoktur. Seçili biçimde ısrar etmek, kullanıcının yakalamasını
    // kaybetmesi demek olurdu — kaydedilmiş bir PNG, kaydedilmemiş bir
    // WebP'den her zaman iyidir.
    if (format != ImageFormat::Png && !IsFormatAvailable(format)) {
        LogV(L"%s kodlayıcısı yok; PNG'ye düşülüyor", m_settings.saveFormat.c_str());
        format = ImageFormat::Png;
    }

    std::wstring path = folder;
    path += L"\\Crisp ";
    path += TimestampForFileName();
    path += L'.';
    path += ExtensionForFormat(format);

    // Aynı saniyede iki yakalama olabilir; benzersizleştirme olmadan ikincisi
    // birincinin üzerine yazardı.
    path = MakeUniquePath(path);

    if (!SaveImage(image, path, format, m_settings.saveQuality)) {
        LogV(L"Kaydedilemedi: %s", path.c_str());
        return false;
    }

    savedPath = std::move(path);
    return true;
}

void App::DeliverCapture(const Image& image, POINT origin) {
    if (!image.Valid()) {
        return;
    }

    if (m_settings.after.copyToClipboard) {
        if (!CopyImageToClipboard(image, m_window)) {
            LogV(L"Panoya kopyalama başarısız");
        }
    }

    if (m_settings.after.saveToFile) {
        std::wstring savedPath;
        if (SaveCapture(image, savedPath)) {
            FlashSaved(savedPath);
        }
    }

    if (m_settings.after.pinToScreen) {
        // İğne, yakalamanın ALINDIĞI yere açılır: kullanıcı sonucu gözüyle
        // takip ettiği yerde bulur, ekranın ortasında değil.
        if (!PinImageToScreen(m_instance, image, origin)) {
            LogV(L"İğneleme başarısız");
        }
    }

    // Düzenleyici sonraki kilometre taşında; ayar açık olsa bile şimdilik
    // sessizce atlanır. Settings::Clamp en az bir hedef garanti ettiği için
    // yakalama yine de bir yere gitmiş olur.
    if (m_settings.after.openEditor) {
        LogV(L"Düzenleyici bu sürümde yok; yakalama diğer eylemlerle işlendi");
    }
}

void App::OpenSaveFolder() {
    const std::wstring folder = m_settings.EffectiveSaveFolder();
    if (folder.empty()) {
        return;
    }
    if (!EnsureDirectory(folder)) {
        LogV(L"Kayıt klasörü oluşturulamadı: %s", folder.c_str());
        return;
    }
    ::ShellExecuteW(nullptr, L"open", folder.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

}  // namespace crisp
