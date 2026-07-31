// App.h — Uygulama kabuğu: görünmez mesaj penceresi, tepsi, kısayollar.
//
// Uygulamanın görünür bir ana penceresi YOKTUR. Tepside yaşar ve yalnızca
// yakalama sırasında ekranda bir şey gösterir. Mesaj penceresi, tepsi geri
// bildirimlerini ve global kısayolları almak için gerekli olan tek şeydir.
#pragma once

#include "Capture.h"
#include "Hotkeys.h"
#include "Settings.h"
#include "TrayIcon.h"

#include <windows.h>

namespace crisp {

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

    // --- AppCapture.cpp ------------------------------------------------------
    void StartCapture(CaptureMode mode);
    void CaptureRegionOrWindow(bool preferWindowPick);
    void CaptureCurrentMonitor();
    void DeliverCapture(const Image& image);
    void OpenSaveFolder();

    [[nodiscard]] bool SaveCapture(const Image& image, std::wstring& savedPath);

    HINSTANCE m_instance = nullptr;
    HWND m_window = nullptr;
    Settings m_settings;
    TrayIcon m_tray;
    Hotkeys m_hotkeys;

    // Geri sayım sırasında yeniden tetiklenmeyi engeller; kaplama açıkken de
    // ikinci bir kaplama açılmamalı.
    bool m_busy = false;
};

inline constexpr const wchar_t* kHostWindowClass = L"CrispMessageWindow";
inline constexpr const wchar_t* kHostWindowTitle = L"Crisp";
inline constexpr const wchar_t* kSingleInstanceMutex =
    L"Local\\CrispSingleInstance";

}  // namespace crisp
