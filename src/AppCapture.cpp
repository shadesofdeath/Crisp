// AppCapture.cpp — Yakalama akışları ve yakalama sonrası eylemler.
//
// Eylem gönderimi, geri sayım ve ikincil görevler AppActions.cpp'de.
#include "App.h"

#include "ClipboardImage.h"
#include "EditorWindow.h"
#include "Geometry.h"
#include "HistoryWindow.h"
#include "ImageCodec.h"
#include "Localization.h"
#include "MessageWindow.h"
#include "Messages.h"
#include "NameFormat.h"
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
    RECT selection{};
    return RunRegionCaptureRect(instance, settings, preferWindowPick, out, origin,
                                selection);
}

bool RunRegionCaptureRect(HINSTANCE instance, const Settings& settings,
                          bool preferWindowPick, Image& out, POINT& origin,
                          RECT& selection) {
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
    selection = result.selection;
    return true;
}

void App::CaptureRegionOrWindow(bool preferWindowPick) {
    Image capture;
    POINT origin{};
    RECT selection{};
    ::Sleep(kMenuSettleMs);
    if (!RunRegionCaptureRect(m_instance, m_settings, preferWindowPick, capture,
                              origin, selection)) {
        return;
    }

    // SON BÖLGE YALNIZCA SÜRÜKLEYEREK SEÇİLENDİR: bir pencereye tıklayarak
    // yakalayan kullanıcı, "son bölge"nin o pencerenin o anki yerini
    // tekrarlamasını beklemez — pencere taşınmış olabilir.
    if (!preferWindowPick) {
        m_settings.lastRegion = selection;
        if (!m_settings.Save(SettingsStore::ForApp())) {
            LogV(L"Son bölge kaydedilemedi");
        }
    }
    DeliverCapture(capture, origin, nullptr);
}

void App::CaptureCurrentMonitor() {
    const RECT monitor = MonitorRectAtCursor();
    Image capture;
    if (!CaptureRect(monitor, capture, m_settings.includeCursor)) {
        LogV(L"Tam ekran yakalama başarısız");
        return;
    }
    DeliverCapture(capture, POINT{monitor.left, monitor.top}, nullptr);
}

bool App::SaveCapture(const Image& image, std::wstring& savedPath,
                      HWND sourceWindow) {
    ImageFormat format = FormatFromString(m_settings.saveFormat.c_str());

    NameContext context;
    ::GetLocalTime(&context.time);
    context.width = image.Width();
    context.height = image.Height();
    context.counter = ++m_captureCounter;
    // TOHUM ZAMANDAN GELİR ama sabit değil: aynı saniyede iki yakalamada
    // sayaç da tohuma katılır, yoksa %ra ikisinde aynı çıkardı.
    context.random = static_cast<unsigned>(context.time.wMilliseconds) * 977u +
                     context.counter;
    context.windowTitle = WindowTitleText(sourceWindow);

    const std::wstring baseName =
        SanitizeFileName(ExpandNameFormat(m_settings.fileNameFormat, context));
    const std::wstring subFolder =
        SanitizeRelativePath(ExpandNameFormat(m_settings.subFolderFormat, context));

    std::wstring path = BuildCapturePath(m_settings.EffectiveSaveFolder(),
                                         subFolder, baseName, format);
    if (path.empty()) {
        return false;
    }

    if (!SaveImage(image, path, format, m_settings.saveQuality)) {
        LogV(L"Kaydedilemedi: %s", path.c_str());
        return false;
    }

    savedPath = std::move(path);
    return true;
}

void App::DeliverCapture(const Image& image, POINT origin, HWND sourceWindow) {
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
        // PANO VE DOSYA İŞLERİNİ DÜZENLEYİCİ KENDİSİ YAPAR ve sonucu burada
        // rapor eder; burada tekrar kopyalamak, kullanıcı düzenleyicide
        // "farklı kaydet" ile başka bir yere yazdığında ikinci bir dosya
        // üretmek olurdu.
        const EditorResult result = RunEditor(m_instance, m_settings, edited);
        if (!result.accepted) {
            return;   // kullanıcı iptal etti
        }
        if (!result.savedPath.empty()) {
            FlashSaved(result.savedPath);
        }
        const bool pinned = m_settings.after.pinToScreen &&
                            PinImageToScreen(m_instance, edited, origin);
        RememberInHistory(edited);
        RunExtraTasks(edited, result.savedPath);
        Announce(edited, result.copied, pinned, result.savedPath);
        return;
    }

    const bool copied = m_settings.after.copyToClipboard &&
                        CopyImageToClipboard(image, m_window);
    if (m_settings.after.copyToClipboard && !copied) {
        LogV(L"Panoya kopyalama başarısız");
    }

    std::wstring savedPath;
    if (m_settings.after.saveToFile) {
        if (SaveCapture(image, savedPath, sourceWindow)) {
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
    RunExtraTasks(image, savedPath);
    Announce(image, copied, pinned, savedPath);
}

void App::Announce(const Image& image, bool copied, bool pinned,
                   const std::wstring& savedPath) {
    if (!m_settings.showNotification) {
        return;
    }

    // SÜREN YÜKLEME BİLDİRİMİN SAHİBİDİR. `RunExtraTasks` bu çağrıdan ÖNCE
    // çalışıyor ve kendiliğinden yükleme oradan başlıyor; buradaki bildirim
    // onunkinin üstüne binseydi, "yükleniyor" kutusu doğduğu anda kapanır ve
    // kullanıcı yirmi saniyelik sessizliğe geri dönerdi. Yüklemenin sonucu
    // zaten kendi bildirimini getiriyor.
    if (m_uploadPending) {
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
    if (!result.savedPath.empty()) {
        FlashSaved(result.savedPath);
    }
    RememberInHistory(chosen.image);
    // BU YOL ESKİDEN SESSİZDİ: geçmişten bir görüntüyü düzenleyip kopyalayan
    // kullanıcı hiçbir bildirim almıyordu ve yakalama akışıyla arasındaki tek
    // fark buydu.
    Announce(chosen.image, result.copied, false, result.savedPath);
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
