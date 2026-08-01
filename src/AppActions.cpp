// AppActions.cpp — Eylem gönderimi, geri sayım, komut satırı ve yakalama
// sonrası ikincil görevler.
//
// AYRI DOSYA: yakalama akışlarıyla aynı dosyada AppCapture.cpp ev kuralının
// 400 satır sınırını aşıyordu (docs §9). Ayrım işlevsel: AppCapture.cpp
// PİKSELLERİ nasıl aldığımızı, burası HANGİ isteğin hangi yolu açtığını
// anlatır.
#include "App.h"

#include "ClipboardImage.h"
#include "EditorWindow.h"
#include "Geometry.h"
#include "ImageCodec.h"
#include "Localization.h"
#include "MessageWindow.h"
#include "Messages.h"
#include "Ocr.h"
#include "PinWindow.h"
#include "Toast.h"
#include "Util.h"
#include "WindowPick.h"
#include "resource.h"

#include <shellapi.h>

#include <string>

namespace crisp {
namespace {

// Tepsi menüsünün kapanmasını beklemek için; gerekçesi AppCapture.cpp'de.
constexpr DWORD kMenuSettleMs = 120;

// Öne alınan pencerenin kendini yeniden çizmesi için. Daha kısası, animasyonlu
// bir pencerede yarı saydam bir kare yakalıyordu.
constexpr DWORD kActivateSettleMs = 220;

}  // namespace

HotkeyAction ActionFromArgument(const wchar_t* argument) noexcept {
    if (argument == nullptr || (*argument != L'-' && *argument != L'/')) {
        return HotkeyAction::None;
    }
    ++argument;

    struct Entry {
        const wchar_t* name;
        HotkeyAction action;
    };
    // ADLAR KISA VE İNGİLİZCE: komut satırı bir arayüz değil, bir sözleşmedir
    // ve dil ayarına göre değişmesi, betiklerin dil değiştiğinde bozulması
    // demek olurdu.
    constexpr Entry kEntries[] = {
        {L"region", HotkeyAction::Region},
        {L"window", HotkeyAction::Window},
        {L"active", HotkeyAction::ActiveWindow},
        {L"monitor", HotkeyAction::Monitor},
        {L"fullscreen", HotkeyAction::Monitor},
        {L"all", HotkeyAction::AllMonitors},
        {L"last", HotkeyAction::LastRegion},
        {L"delayed", HotkeyAction::Delayed},
        {L"text", HotkeyAction::SelectText},
        {L"ocr", HotkeyAction::RegionText},
        {L"color", HotkeyAction::PickColor},
        {L"history", HotkeyAction::History},
    };
    for (const Entry& entry : kEntries) {
        if (::_wcsicmp(argument, entry.name) == 0) {
            return entry.action;
        }
    }
    return HotkeyAction::None;
}

int CommandForAction(HotkeyAction action) noexcept {
    switch (action) {
        case HotkeyAction::Window:       return IDM_CAPTURE_WINDOW;
        case HotkeyAction::ActiveWindow: return IDM_CAPTURE_ACTIVE;
        case HotkeyAction::Monitor:      return IDM_CAPTURE_FULLSCREEN;
        case HotkeyAction::AllMonitors:  return IDM_CAPTURE_ALL;
        case HotkeyAction::LastRegion:   return IDM_CAPTURE_LAST;
        case HotkeyAction::Delayed:      return IDM_CAPTURE_DELAYED;
        case HotkeyAction::SelectText:   return IDM_SELECT_TEXT;
        case HotkeyAction::RegionText:   return IDM_CAPTURE_OCR;
        case HotkeyAction::PickColor:    return IDM_PICK_COLOR;
        case HotkeyAction::History:      return IDM_HISTORY;
        // BİLİNMEYEN VE BOŞ EYLEM BÖLGE SEÇER: kullanıcı exe'ye tıkladıysa
        // (argümansız ikinci örnek) beklediği şey budur.
        default:                         return IDM_CAPTURE_REGION;
    }
}

void App::RunAction(HotkeyAction action) {
    switch (action) {
        case HotkeyAction::Region:
        case HotkeyAction::Window:
        case HotkeyAction::ActiveWindow:
        case HotkeyAction::Monitor:
        case HotkeyAction::AllMonitors:
        case HotkeyAction::LastRegion:
            StartCapture(action, false);
            return;
        case HotkeyAction::Delayed:
            // "Gecikmeli" TEK BAŞINA BİR EYLEM DEĞİL, bölge yakalamanın
            // gecikmeli hâlidir; kullanıcının beklediği de budur.
            StartCapture(HotkeyAction::Region, true);
            return;
        case HotkeyAction::SelectText:
            SelectTextOnScreen();
            return;
        case HotkeyAction::RegionText:
            CaptureTextToClipboard();
            return;
        case HotkeyAction::PickColor:
            PickColorToClipboard();
            return;
        case HotkeyAction::History:
            ShowHistory();
            return;
        default:
            return;
    }
}

void App::StartCapture(HotkeyAction action, bool withDelay) {
    if (m_busy) {
        return;   // geri sayım sürüyor ya da kaplama zaten açık
    }

    if (withDelay && m_settings.delaySeconds > 0) {
        m_busy = true;
        m_pendingAction = action;
        m_countdown = m_settings.delaySeconds;
        ShowCountdown();
        if (::SetTimer(m_window, TIMER_COUNTDOWN, 1000, nullptr) == 0) {
            LogV(L"Geri sayım zamanlayıcısı kurulamadı");
            m_busy = false;
            m_pendingAction = HotkeyAction::None;
            CloseCaptureToast();
        }
        return;
    }

    m_busy = true;
    PerformCapture(action);
    m_busy = false;
}

void App::ShowCountdown() {
    if (!m_settings.showNotification) {
        return;
    }
    // GERİ SAYIM BİLDİRİM PENCERESİNİ KULLANIR: ekranın köşesinde zaten
    // görünen, temalı ve kendi kendine kaybolan bir pencere var. İkinci bir
    // katmanlı pencere yazmak aynı işi ikinci kez yapmak olurdu.
    wchar_t text[32];
    ::swprintf_s(text, L"%u", m_countdown);
    const Image empty;
    ShowCaptureToast(m_instance, empty, Loc::Str(IDS_TOAST_COUNTDOWN), text,
                     std::wstring());
}

void App::PerformCapture(HotkeyAction action) {
    switch (action) {
        case HotkeyAction::Region:
            CaptureRegionOrWindow(false);
            return;
        case HotkeyAction::Window:
            CaptureRegionOrWindow(true);
            return;
        case HotkeyAction::ActiveWindow:
            CaptureActiveWindow();
            return;
        case HotkeyAction::Monitor:
            CaptureCurrentMonitor();
            return;
        case HotkeyAction::AllMonitors:
            CaptureAllMonitors();
            return;
        case HotkeyAction::LastRegion:
            CaptureLastRegion();
            return;
        default:
            return;
    }
}

void App::CaptureActiveWindow() {
    // Tepsi menüsü daha kapanıyor olabilir; kapanan menü ekranda kalırsa
    // yakalamanın içine girer.
    ::Sleep(kMenuSettleMs);

    const HWND window = ForegroundCapturableWindow();
    if (window == nullptr) {
        LogV(L"Aktif pencere bulunamadı");
        return;
    }

    // HEDEF ÖNE ALINIR. İki sebep:
    //   1. BitBlt EKRANDA GÖRÜNENİ alır. Tepsiden çağrıldığında hedef artık ön
    //      planda değildir ve üstünü örten bir pencere varsa onun pikselleri
    //      yakalanırdı.
    //   2. "Aktif pencere" görüntüsünde başlık çubuğunun ETKİN görünmesi
    //      beklenir; pasif başlıklı bir pencere yanlış anı gösterir.
    if (::GetForegroundWindow() != window) {
        (void)::SetForegroundWindow(window);
        ::Sleep(kActivateSettleMs);
    }

    Image capture;
    if (!CaptureWindow(window, capture, m_settings.includeCursor)) {
        LogV(L"Aktif pencere yakalanamadı");
        return;
    }
    RECT bounds{};
    (void)::GetWindowRect(window, &bounds);
    DeliverCapture(capture, POINT{bounds.left, bounds.top}, window);
}

void App::CaptureAllMonitors() {
    const RECT screen = VirtualScreenRect();
    Image capture;
    if (!CaptureRect(screen, capture, m_settings.includeCursor)) {
        LogV(L"Tüm monitörler yakalanamadı");
        return;
    }
    DeliverCapture(capture, POINT{screen.left, screen.top}, nullptr);
}

void App::CaptureLastRegion() {
    if (!m_settings.HasLastRegion()) {
        return;   // hiç bölge seçilmemiş; sessizce hiçbir şey yapma
    }
    // BÖLGE EKRANA SIĞDIRILIR: monitör düzeni değişmiş olabilir ve ekran
    // dışında kalan bir dikdörtgeni yakalamak çöp piksel kopyalardı.
    const RECT clamped = geom::ClampTo(m_settings.lastRegion, VirtualScreenRect());
    if (geom::IsEmpty(clamped)) {
        return;
    }
    Image capture;
    if (!CaptureRect(clamped, capture, m_settings.includeCursor)) {
        return;
    }
    DeliverCapture(capture, POINT{clamped.left, clamped.top}, nullptr);
}

void App::OpenClipboardImage() {
    Image image;
    if (!ReadImageFromClipboard(image, m_window)) {
        return;
    }
    // PANODAN GELEN GÖRÜNTÜ HER ZAMAN DÜZENLEYİCİYE GİDER: ayarlardaki
    // "yakalamadan sonra" kuralları bir YAKALAMA için yazılmıştır ve panodaki
    // görüntüyü tekrar panoya koymak anlamsız olurdu.
    const EditorResult result = RunEditor(m_instance, m_settings, image);
    if (!result.accepted) {
        return;
    }
    RememberInHistory(image);
    Announce(image, result.copied, false, result.savedPath);
}

void App::OpenImageFile(const std::wstring& path) {
    // ÇIKIŞ HER YOLDAN GEÇER: dosya açılamasa da, kullanıcı düzenleyiciyi iptal
    // etse de, yalnızca bu dosya için başlatılmış bir örnek geride kalmamalı.
    struct ExitGuard {
        bool armed;
        HWND window;
        ~ExitGuard() {
            if (armed && window != nullptr) {
                ::DestroyWindow(window);
            }
        }
    } guard{m_exitAfterFile, m_window};

    Image image;
    if (!LoadImageFile(path, image)) {
        ShowMessage(m_instance, m_window, Loc::Str(IDS_OPEN_FAILED),
                    MessageIcon::Error);
        return;
    }
    const EditorResult result = RunEditor(m_instance, m_settings, image);
    if (!result.accepted) {
        return;
    }
    RememberInHistory(image);
    Announce(image, result.copied, false, result.savedPath);
}

void App::RunExtraTasks(const Image& image, const std::wstring& savedPath) {
    const AfterCapture& after = m_settings.after;

    // SIRA ÖNEMLİ: pano görevleri birbirinin üstüne yazar ve son yazan kazanır.
    // Yol → dosya → metin sırası, en dar kapsamlıdan en genişe gider; kullanıcı
    // ikisini birden açtıysa muhtemelen sonuncusunu istiyordur.
    if (after.copyPathToClipboard && !savedPath.empty()) {
        if (!CopyTextToClipboard(savedPath.c_str(), m_window)) {
            LogV(L"Dosya yolu panoya kopyalanamadı");
        }
    }
    if (after.copyFileToClipboard && !savedPath.empty()) {
        if (!CopyFileToClipboard(savedPath, m_window)) {
            LogV(L"Dosya panoya kopyalanamadı");
        }
    }
    if (after.copyTextViaOcr) {
        std::wstring text;
        if (RecognizeText(image, text) && !text.empty()) {
            if (!CopyTextToClipboard(text.c_str(), m_window)) {
                LogV(L"Tanınan metin panoya kopyalanamadı");
            }
        }
    }
    if (after.revealInFolder && !savedPath.empty()) {
        std::wstring arguments = L"/select,\"";
        arguments += savedPath;
        arguments += L'"';
        ::ShellExecuteW(nullptr, L"open", L"explorer.exe", arguments.c_str(),
                        nullptr, SW_SHOWNORMAL);
    }
}

}  // namespace crisp
