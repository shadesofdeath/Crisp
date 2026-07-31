// Hotkeys.h — Global kısayol kaydı.
//
// RegisterHotKey başarısız olabilir ve bu NORMAL bir durumdur: başka bir
// uygulama aynı kombinasyonu almış olabilir. Kayıt hatası uygulamayı
// durdurmaz; hangi kısayolların çalıştığı çağırana bildirilir ki kullanıcıya
// söylenebilsin.
#pragma once

#include "Settings.h"

#include <windows.h>

namespace crisp {

class Hotkeys {
public:
    Hotkeys() noexcept = default;

    Hotkeys(const Hotkeys&) = delete;
    Hotkeys& operator=(const Hotkeys&) = delete;

    ~Hotkeys() { UnregisterAll(); }

    // Önce mevcut kayıtları kaldırır, sonra ayarlardakileri kaydeder.
    // Dönüş: kaydedilemeyen kısayol sayısı (0 = hepsi başarılı).
    int Apply(HWND owner, const Settings& settings);

    void UnregisterAll() noexcept;

    // Belirli bir kısayol şu an kayıtlı mı?
    [[nodiscard]] bool IsRegistered(int id) const noexcept;

private:
    HWND m_owner = nullptr;
    // Bit maskesi: HotkeyId değerleri 1..4 olduğu için 1 << id yeterli.
    unsigned m_registered = 0;
};

}  // namespace crisp
