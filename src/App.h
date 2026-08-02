// App.h — Uygulama kabuğu: görünmez mesaj penceresi, tepsi, kısayollar.
//
// Uygulamanın görünür bir ana penceresi YOKTUR. Tepside yaşar ve yalnızca
// yakalama sırasında ekranda bir şey gösterir. Mesaj penceresi, tepsi geri
// bildirimlerini ve global kısayolları almak için gerekli olan tek şeydir.
#pragma once

#include "Capture.h"
#include "History.h"
#include "Hotkeys.h"
#include "Settings.h"
#include "TrayIcon.h"

#include <windows.h>

namespace crisp {

// Kaplamayı çalıştırıp seçili alanı kırpar (AppCapture.cpp). Yakalama ve
// metin akışları ayrı dosyalarda olduğu için paylaşılır.
[[nodiscard]] bool RunRegionCapture(HINSTANCE instance, const Settings& settings,
                                    bool preferWindowPick, Image& out,
                                    POINT& origin);

// Aynısı, ama seçilen dikdörtgeni de verir — "son bölge" onu saklar.
[[nodiscard]] bool RunRegionCaptureRect(HINSTANCE instance,
                                        const Settings& settings,
                                        bool preferWindowPick, Image& out,
                                        POINT& origin, RECT& selection);

// Komut satırı argümanını bir eyleme çevirir; tanınmayan argüman None döner.
// Dosya yolu argümanları burada değil, çağıranda ele alınır.
[[nodiscard]] HotkeyAction ActionFromArgument(const wchar_t* argument) noexcept;

// Eylemi tepsi menüsü komutuna çevirir. Komut satırı ve ikinci örnek isteği
// mesajla taşınır ve mesajda taşınabilen tek şey komut kimliğidir.
[[nodiscard]] int CommandForAction(HotkeyAction action) noexcept;

class App {
public:
    App() noexcept = default;

    App(const App&) = delete;
    App& operator=(const App&) = delete;

    [[nodiscard]] bool Initialize(HINSTANCE instance);
    [[nodiscard]] int Run();

    // Zaten çalışan bir örneğin penceresi; tek örnek denetimi için.
    [[nodiscard]] static HWND FindExistingInstanceWindow();

private:
    static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam,
                                       LPARAM lParam);
    LRESULT HandleMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

    void OnCommand(int command);
    void OnHotkey(int id);

    // Ayarlar penceresini açar ve onaylanırsa değişiklikleri uygular.
    void ShowSettings();

    // Explorer'ın sağ tık menüsündeki fiili ayara göre kaydeder ya da
    // kaldırır.
    void ApplyShellMenuSetting();

public:
    // Komut satırından ve ikinci örnekten çağrılır.
    void RunAction(HotkeyAction action);
    void OpenImageFile(const std::wstring& path);

    // YALNIZCA BİR DOSYA AÇMAK İÇİN başlatıldıysa, düzenleyici kapanınca
    // uygulama da kapanır.
    //
    // SEBEBİ ŞAŞIRTMA: Explorer'da bir resme sağ tıklayıp "Crisp ile düzenle"
    // diyen kullanıcı bir ekran alıntısı aracı başlatmak istemiyordu. Tepside
    // bir simge ve dört global kısayol bırakmak, istenmeyen bir yan etki
    // olurdu. Zaten çalışan bir örneğe yönlendirilen istekler bunu KURMAZ:
    // orada tepsi simgesi kullanıcının kendi kararıydı.
    void SetExitAfterFile(bool exit) noexcept { m_exitAfterFile = exit; }

private:
    // --- AppCapture.cpp ------------------------------------------------------
    // Gecikme ayarı AÇIKSA geri sayımı başlatır, değilse hemen yakalar.
    //
    // GECİKME ARTIK BİR KİP DEĞİL: eskiden yalnızca "gecikmeli yakalama"
    // komutu bekliyordu ve bir menüyü açıp yakalamak isteyen kullanıcı için
    // pencere ya da monitör kipinde gecikme yoktu.
    void StartCapture(HotkeyAction action, bool withDelay);
    void PerformCapture(HotkeyAction action);
    void CaptureRegionOrWindow(bool preferWindowPick);
    void CaptureCurrentMonitor();
    void CaptureActiveWindow();
    void CaptureAllMonitors();
    void CaptureLastRegion();
    void OpenClipboardImage();
    void DeliverCapture(const Image& image, POINT origin, HWND sourceWindow);
    void OpenSaveFolder();

    // Yakalamadan sonra çalışan ikincil görevler: yolu kopyala, dosyayı
    // kopyala, klasörde göster, metni tanı.
    void RunExtraTasks(const Image& image, const std::wstring& savedPath);

    // Görüntüyü seçili servise ARKA PLANDA gönderir ve dönen bağlantıyı panoya
    // koyar. Hemen döner; yükleme kendi iş parçacığında sürer.
    void UploadInBackground(const Image& image);

    // Geçmiş penceresini açar; kullanıcı bir kayıt seçerse düzenleyiciye taşır.
    void ShowHistory();
    void RememberInHistory(const Image& image);

    // Yakalamadan sonra köşede kısa bir bildirim gösterir.
    void Announce(const Image& image, bool copied, bool pinned,
                  const std::wstring& savedPath);

    // Bölge seçtirip içindeki metni panoya kopyalar.
    void CaptureTextToClipboard();
    // Ekranı tarayıp kelimeleri kutular; kullanıcı metin seçer gibi seçer.
    void SelectTextOnScreen();
    // Ekrandan bir pikselin rengini seçtirip panoya kopyalar.
    void PickColorToClipboard();

    [[nodiscard]] bool SaveCapture(const Image& image, std::wstring& savedPath,
                                   HWND sourceWindow);
    void ReportSaveFailure();

    // Geri sayımın kalan saniyesini ekranda gösterir.
    void ShowCountdown();

    // Kaydedilemeyen kısayolları kullanıcıya bildirir; hepsi kaydedildiyse
    // hiçbir şey yapmaz.
    void ReportHotkeyFailures();

    HINSTANCE m_instance = nullptr;
    HWND m_window = nullptr;
    Settings m_settings;
    TrayIcon m_tray;
    Hotkeys m_hotkeys;
    HistoryStore m_history;

    // Geri sayım sırasında yeniden tetiklenmeyi engeller; kaplama açıkken de
    // ikinci bir kaplama açılmamalı.
    bool m_busy = false;

    // Geri sayım bittiğinde çalıştırılacak eylem ve kalan saniye.
    HotkeyAction m_pendingAction = HotkeyAction::None;
    unsigned m_countdown = 0;

    // Dosya adı şablonundaki %i sayacı. Oturum boyunca artar; diske
    // yazılmaz, çünkü "bugünün kaçıncı yakalaması" sorusunun cevabı
    // oturumlar arası taşınacak kadar önemli değil.
    unsigned m_captureCounter = 0;

    bool m_exitAfterFile = false;
};

inline constexpr const wchar_t* kHostWindowClass = L"CrispMessageWindow";
inline constexpr const wchar_t* kHostWindowTitle = L"Crisp";
inline constexpr const wchar_t* kSingleInstanceMutex =
    L"Local\\CrispSingleInstance";

}  // namespace crisp
