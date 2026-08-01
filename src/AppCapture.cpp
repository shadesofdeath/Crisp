// AppCapture.cpp — Yakalama akışları ve yakalama sonrası eylemler.
#include "App.h"

#include "ClipboardImage.h"
#include "EditorWindow.h"
#include "Geometry.h"
#include "HistoryWindow.h"
#include "ImageCodec.h"
#include "Localization.h"
#include "MessageWindow.h"
#include "Messages.h"
#include "Ocr.h"
#include "Overlay.h"
#include "PinWindow.h"
#include "Sound.h"
#include "Toast.h"
#include "Util.h"
#include "WindowPick.h"
#include "resource.h"

#include <mmsystem.h>
#include <shellapi.h>

#include <cstdint>
#include <string>
#include <vector>

namespace crisp {
namespace {

// Kaplama açılmadan önce kısa bir bekleme. Kullanıcı tepsi menüsünden
// tetiklediyse menü daha kapanmamış olabilir ve dondurulmuş ekranda menünün
// kendisi görünürdü.
constexpr DWORD kMenuSettleMs = 120;

void PlayShutter() {
    // TAMPON ÇALMA BİTENE KADAR YAŞAMALI: SND_ASYNC ile PlaySound hemen döner
    // ve belleği kendisi kopyalamaz. Yerel bir vektör kullanmak, sesin yarısı
    // çalınmışken serbest bırakılan bellekten okumak olurdu.
    static const std::vector<uint8_t> shutter = BuildShutterWav();
    // SND_NODEFAULT: ses aygıtı yoksa Windows kendi uyarı sesini çalmasın —
    // yakalama alındığında "ding" duymak, sessizlikten kötü.
    ::PlaySoundW(reinterpret_cast<LPCWSTR>(shutter.data()), nullptr,
                 SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
}

void FlashSaved(const std::wstring& path) {
    // Kayıt yerini sessizce bildirmenin en ucuz yolu: hata ayıklama günlüğü.
    // Bir bildirim balonu, her yakalamada kullanıcıyı rahatsız ederdi.
    LogV(L"Kaydedildi: %s", path.c_str());
}

}  // namespace

// Kaplamayı çalıştırıp seçili alanı kırpar. Kullanıcı iptal ederse ya da
// bir şey ters giderse false döner ve `origin` dokunulmaz.
//
// ADSIZ AD ALANINDA DEĞİL: hem yakalama hem metin akışları kullanıyor ve
// ikisi ayrı dosyalarda (AppCapture.cpp / AppText.cpp).
bool RunRegionCapture(HINSTANCE instance, const Settings& settings,
                      bool preferWindowPick, Image& out, POINT& origin) {
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

    // SES BURADA, teslim yollarının BAŞINDA: her yakalama kipi buradan geçer
    // ve düzenleyici açılmadan önce çalar — çünkü ses yakalamanın alındığını
    // bildirir, düzenlemenin bittiğini değil.
    if (m_settings.playShutterSound) {
        PlayShutter();
    }

    // DÜZENLEYİCİ VARSA ÖNCE O ÇALIŞIR: kullanıcı işaretlemesini yaptıktan
    // sonra hangi hedeflere gideceğine kendisi karar verir. Önce panoya
    // kopyalayıp sonra düzenleyici açmak, panoda işaretlenmemiş bir görüntü
    // bırakırdı.
    if (m_settings.after.openEditor) {
        Image edited;
        if (!CropImage(image, 0, 0, image.Width(), image.Height(), edited)) {
            return;
        }
        const EditorResult result = RunEditor(m_instance, m_settings, edited);
        if (!result.accepted) {
            return;   // kullanıcı iptal etti
        }
        const bool copied =
            result.copyToClipboard && CopyImageToClipboard(edited, m_window);
        std::wstring savedPath;
        if (result.saveToFile) {
            if (SaveCapture(edited, savedPath)) {
                FlashSaved(savedPath);
            } else {
                ReportSaveFailure();
            }
        }
        const bool pinned = m_settings.after.pinToScreen &&
                            PinImageToScreen(m_instance, edited, origin);
        RememberInHistory(edited);
        Announce(edited, copied, pinned, savedPath);
        return;
    }

    const bool copied = m_settings.after.copyToClipboard &&
                        CopyImageToClipboard(image, m_window);
    if (m_settings.after.copyToClipboard && !copied) {
        LogV(L"Panoya kopyalama başarısız");
    }

    std::wstring savedPath;
    if (m_settings.after.saveToFile) {
        if (SaveCapture(image, savedPath)) {
            FlashSaved(savedPath);
        } else {
            ReportSaveFailure();
        }
    }

    // İğne, yakalamanın ALINDIĞI yere açılır: kullanıcı sonucu gözüyle takip
    // ettiği yerde bulur, ekranın ortasında değil.
    const bool pinned = m_settings.after.pinToScreen &&
                        PinImageToScreen(m_instance, image, origin);

    RememberInHistory(image);
    Announce(image, copied, pinned, savedPath);
}

void App::Announce(const Image& image, bool copied, bool pinned,
                   const std::wstring& savedPath) {
    if (!m_settings.showNotification) {
        return;
    }

    // BAŞLIK NE OLDUĞUNU SÖYLER, ne olmasını istediğimizi değil: kopyalama
    // başarısız olduysa "kopyalandı" yazmak kullanıcıyı olmayan bir panoya
    // güvendirirdi.
    const bool saved = !savedPath.empty();
    UINT titleId = IDS_TOAST_CAPTURED;
    if (copied && saved) {
        titleId = IDS_TOAST_COPIED_SAVED;
    } else if (saved) {
        titleId = IDS_TOAST_SAVED;
    } else if (copied) {
        titleId = IDS_TOAST_COPIED;
    } else if (pinned) {
        titleId = IDS_TOAST_PINNED;
    }

    std::wstring detail;
    if (saved) {
        const size_t slash = savedPath.find_last_of(L'\\');
        detail = slash == std::wstring::npos ? savedPath
                                             : savedPath.substr(slash + 1);
    } else {
        wchar_t size[64];
        ::swprintf_s(size, L"%d × %d", image.Width(), image.Height());
        detail = size;
    }

    ShowCaptureToast(m_instance, image, Loc::Str(titleId), detail, savedPath);
}

void App::ReportSaveFailure() {
    // GÜNLÜĞE YAZMAK YETMEZ: kullanıcı "dosyaya kaydet" seçmişse ve disk dolu
    // ya da klasör yazılamıyorsa, yakalamayı kaybettiğini ÖĞRENMELİ. Sessiz
    // kalmak, saatler sonra klasörü açıp boş bulmak demek.
    ShowMessage(m_instance, m_window, Loc::Str(IDS_SAVE_FAILED),
                MessageIcon::Error);
}

void App::RememberInHistory(const Image& image) {
    // GEÇMİŞ, TESLİM EDİLENİ SAKLAR: düzenleyici açıksa işaretlenmiş hâli,
    // değilse ham yakalamayı. Kullanıcı iptal ettiyse hiçbir şey saklanmaz —
    // vazgeçilen bir yakalamayı diskte tutmak, "sildiğimi sandığım şey neden
    // hâlâ duruyor" sorusunu doğururdu.
    if (m_settings.historyLimit == 0) {
        return;   // kullanıcı geçmişi kapatmış
    }
    m_history.SetLimit(m_settings.historyLimit);
    std::wstring written;
    if (!m_history.Record(image, written)) {
        LogV(L"Geçmişe yazılamadı");
    }
}

void App::ShowHistory() {
    if (m_busy) {
        return;
    }
    m_busy = true;
    HistoryResult chosen = ShowHistoryWindow(m_instance, m_history);
    m_busy = false;

    if (chosen.choice != HistoryChoice::Edit || !chosen.image.Valid()) {
        return;
    }

    // Geçmişten gelen bir görüntü düzenlendiğinde YENİ bir kayıt doğar; eskisi
    // yerinde kalır. Üzerine yazmak, kullanıcının elindeki tek kopyayı
    // farkında olmadan değiştirmesi olurdu.
    const EditorResult result = RunEditor(m_instance, m_settings, chosen.image);
    if (!result.accepted) {
        return;
    }
    if (result.copyToClipboard && !CopyImageToClipboard(chosen.image, m_window)) {
        LogV(L"Geçmişten düzenlenen görüntü panoya kopyalanamadı");
    }
    if (result.saveToFile) {
        std::wstring savedPath;
        if (SaveCapture(chosen.image, savedPath)) {
            FlashSaved(savedPath);
        } else {
            ReportSaveFailure();
        }
    }
    RememberInHistory(chosen.image);
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
