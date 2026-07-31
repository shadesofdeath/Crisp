// TrayIcon.h — Bildirim alanı simgesi ve bağlam menüsü.
#pragma once

#include <windows.h>

namespace crisp {

class TrayIcon {
public:
    TrayIcon() noexcept = default;

    TrayIcon(const TrayIcon&) = delete;
    TrayIcon& operator=(const TrayIcon&) = delete;

    ~TrayIcon() { Remove(); }

    [[nodiscard]] bool Add(HWND owner, HINSTANCE instance);
    void Remove() noexcept;

    // Görev çubuğu temasına göre açık/koyu simge çeşidini seçer. Tema
    // değişmediyse hiçbir şey yapmaz, bu yüzden bir zamanlayıcıdan sık sık
    // çağrılabilir.
    void RefreshTheme();

    // Explorer yeniden başladığında simge kaybolur; kabuk bunu
    // "TaskbarCreated" mesajıyla duyurur ve simge yeniden eklenmelidir.
    [[nodiscard]] static UINT TaskbarCreatedMessage() noexcept;
    void Restore();

    // Bağlam menüsünü imlecin konumunda açar ve seçilen komut kimliğini
    // döndürür (iptal edilirse 0).
    [[nodiscard]] int ShowMenu(HWND owner);

private:
    void UpdateIcon();

    HWND m_owner = nullptr;
    HINSTANCE m_instance = nullptr;
    bool m_added = false;
    bool m_darkTaskbar = true;
    bool m_themeKnown = false;
};

}  // namespace crisp
