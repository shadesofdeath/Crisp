// Hotkeys.cpp — bkz. Hotkeys.h.
#include "Hotkeys.h"

#include "Messages.h"
#include "Util.h"

namespace crisp {
namespace {

// MOD_NOREPEAT: tuş basılı tutulduğunda tek bir mesaj gelir. Onsuz kullanıcı
// kısayolu bir saniye basılı tutunca onlarca yakalama başlardı.
[[nodiscard]] bool Register(HWND owner, int id, const Hotkey& hotkey) {
    if (!hotkey.assigned()) {
        return false;
    }
    if (::RegisterHotKey(owner, id, hotkey.modifiers | MOD_NOREPEAT, hotkey.key)) {
        return true;
    }
    LogV(L"RegisterHotKey(%d) başarısız (hata %lu) — kombinasyon başkasında olabilir",
         id, ::GetLastError());
    return false;
}

}  // namespace

int Hotkeys::Apply(HWND owner, const Settings& settings) {
    UnregisterAll();
    m_owner = owner;

    struct Binding {
        int id;
        const Hotkey* hotkey;
    };
    const Binding bindings[] = {
        {HOTKEY_REGION, &settings.hotkeyRegion},
        {HOTKEY_FULLSCREEN, &settings.hotkeyFullScreen},
        {HOTKEY_WINDOW, &settings.hotkeyWindow},
        {HOTKEY_DELAYED, &settings.hotkeyDelayed},
    };

    int failures = 0;
    for (const Binding& binding : bindings) {
        if (!binding.hotkey->assigned()) {
            continue;   // atanmamış kısayol bir hata değildir
        }
        if (Register(owner, binding.id, *binding.hotkey)) {
            m_registered |= (1u << binding.id);
        } else {
            ++failures;
        }
    }
    return failures;
}

void Hotkeys::UnregisterAll() noexcept {
    if (m_owner == nullptr) {
        m_registered = 0;
        return;
    }
    for (int id = HOTKEY_REGION; id <= HOTKEY_DELAYED; ++id) {
        if ((m_registered & (1u << id)) != 0) {
            ::UnregisterHotKey(m_owner, id);
        }
    }
    m_registered = 0;
}

bool Hotkeys::IsRegistered(int id) const noexcept {
    return (m_registered & (1u << id)) != 0;
}

}  // namespace crisp
