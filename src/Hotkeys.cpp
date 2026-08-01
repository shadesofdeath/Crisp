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

    // PRINT SCREEN AYRI ELE ALINIR: Settings::Clamp değiştiricisi olmayan
    // kısayolları iptal eder, çünkü tek bir harf tuşuna basınca yakalama
    // başlaması istenmez. Print Screen ise tam olarak bunun için var olan bir
    // tuştur ve o kuralın istisnasıdır.
    // BİR YUVA PRINT SCREEN'İ ALDIYSA hazır kutucuk devreye girmez: aynı tuş
    // iki kez kaydedilemez ve ikinci deneme, kullanıcının o yuvaya seçtiği
    // eylemi sessizce çalışmaz bırakırdı. Kullanıcının açıkça atadığı kısayol,
    // kutucuğun varsayılan davranışını yener.
    bool slotOwnsPrintScreen = false;
    for (const HotkeyBinding& binding : settings.hotkeys) {
        if (binding.action != HotkeyAction::None && binding.key.assigned() &&
            binding.key.key == VK_SNAPSHOT && binding.key.modifiers == 0) {
            slotOwnsPrintScreen = true;
        }
    }

    int failures = 0;
    if (settings.printScreenCapture && !slotOwnsPrintScreen) {
        if (::RegisterHotKey(owner, HOTKEY_PRINTSCREEN, MOD_NOREPEAT,
                             VK_SNAPSHOT)) {
            m_registered |= (1u << HOTKEY_PRINTSCREEN);
        } else {
            // Windows'un "PrtScn ile Ekran Alıntısı'nı aç" ayarı bu tuşu
            // kendine ayırmış olabilir; bu bir hata değil, çakışmadır.
            LogV(L"Print Screen kaydedilemedi (hata %lu) — Windows ayarı almış olabilir",
                 ::GetLastError());
            ++failures;
        }
    }
    for (int slot = 0; slot < kHotkeySlots; ++slot) {
        const HotkeyBinding& binding = settings.hotkeys[slot];
        if (!binding.key.assigned() || binding.action == HotkeyAction::None) {
            continue;   // atanmamış kısayol bir hata değildir
        }
        const int id = HOTKEY_SLOT_FIRST + slot;
        if (Register(owner, id, binding.key)) {
            m_registered |= (1u << id);
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
    for (int id = HOTKEY_SLOT_FIRST; id <= HOTKEY_PRINTSCREEN; ++id) {
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
