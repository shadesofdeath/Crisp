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

enum class CaptureMode {
    Region,       // sürükleyerek alan
    Window,       // imlecin altındaki pencere
    FullScreen,   // imlecin bulunduğu monitör
    Delayed,      // geri sayım, sonra bölge seçimi
};

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

    // --- AppCapture.cpp ------------------------------------------------------
    void StartCapture(CaptureMode mode);
    void CaptureRegionOrWindow(bool preferWindowPick);
    void CaptureCurrentMonitor();
    void DeliverCapture(const Image& image, POINT origin);
    void OpenSaveFolder();

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

    [[nodiscard]] bool SaveCapture(const Image& image, std::wstring& savedPath);
    void ReportSaveFailure();

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
};

inline constexpr const wchar_t* kHostWindowClass = L"CrispMessageWindow";
inline constexpr const wchar_t* kHostWindowTitle = L"Crisp";
inline constexpr const wchar_t* kSingleInstanceMutex =
    L"Local\\CrispSingleInstance";

}  // namespace crisp
