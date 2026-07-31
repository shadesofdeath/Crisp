// AppCapture.cpp — Yakalama akışları ve yakalama sonrası eylemler.
#include "App.h"

#include "ClipboardImage.h"
#include "Geometry.h"
#include "ImageCodec.h"
#include "Messages.h"
#include "Overlay.h"
#include "Util.h"
#include "WindowPick.h"

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
    Image frozen;
    const OverlayResult result =
        RunSelectionOverlay(m_instance, m_settings, preferWindowPick, frozen);

    if (!result.accepted || !frozen.Valid()) {
        return;
    }

    // Kaplama ekran koordinatı döndürür; dondurulmuş görüntü sanal ekranın
    // sol-üst köşesinden başlar, bu yüzden köken çıkarılmalı.
    const RECT screen = VirtualScreenRect();
    const int x = static_cast<int>(result.selection.left - screen.left);
    const int y = static_cast<int>(result.selection.top - screen.top);
    const int width = static_cast<int>(geom::Width(result.selection));
    const int height = static_cast<int>(geom::Height(result.selection));

    Image capture;
    if (!CropImage(frozen, x, y, width, height, capture)) {
        LogV(L"Seçim kırpılamadı (%d,%d %dx%d)", x, y, width, height);
        return;
    }

    DeliverCapture(capture);
}

void App::CaptureCurrentMonitor() {
    Image capture;
    if (!CaptureRect(MonitorRectAtCursor(), capture)) {
        LogV(L"Tam ekran yakalama başarısız");
        return;
    }
    DeliverCapture(capture);
}

bool App::SaveCapture(const Image& image, std::wstring& savedPath) {
    std::wstring folder = m_settings.EffectiveSaveFolder();
    if (folder.empty()) {
        return false;
    }

    std::wstring path = folder;
    path += L"\\Crisp ";
    path += TimestampForFileName();
    path += L".png";

    // Aynı saniyede iki yakalama olabilir; benzersizleştirme olmadan ikincisi
    // birincinin üzerine yazardı.
    path = MakeUniquePath(path);

    if (!SavePng(image, path)) {
        LogV(L"PNG kaydedilemedi: %s", path.c_str());
        return false;
    }

    savedPath = std::move(path);
    return true;
}

void App::DeliverCapture(const Image& image) {
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

    // Ekrana iğneleme ve düzenleyici sonraki kilometre taşlarında; ayar açık
    // olsa bile şimdilik sessizce atlanır. Kullanıcının yakalaması yine de
    // panoya ya da dosyaya gitmiş olur (Settings::Clamp en az birini garanti
    // eder), yani hiçbir yakalama kaybolmaz.
    if (m_settings.after.pinToScreen || m_settings.after.openEditor) {
        LogV(L"İğneleme/düzenleyici bu sürümde yok; yakalama diğer eylemlerle işlendi");
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
