// AppSettings.cpp — Ayar penceresinin uygulamaya dönen sonuçları.
//
// AYRI DOSYA: pencere ömrü ve mesaj yönlendirmesiyle aynı dosyada App.cpp ev
// kuralının 400 satır sınırını aşıyordu (docs §9). Ayrım işlevsel: App.cpp
// pencerenin YAŞADIĞI yer, burası kullanıcı "Tamam"a bastıktan sonra NELERİN
// yeniden kurulduğu — dil, tema, kabuk menüsü ve kısayollar.
#include "App.h"

#include "HotkeyEdit.h"
#include "Localization.h"
#include "MessageWindow.h"
#include "Messages.h"
#include "SettingsInternal.h"
#include "SettingsWindow.h"
#include "ShellIntegration.h"
#include "Theme.h"
#include "Util.h"
#include "resource.h"

#include <string>

namespace crisp {

void App::ApplyShellMenuSetting() {
    const bool ok = m_settings.shellContextMenu
                        ? RegisterShellMenu(Loc::Str(IDS_SHELL_VERB))
                        : UnregisterShellMenu();
    if (ok) {
        return;
    }
    // BAŞARISIZLIK SESSİZ KALMAZ: kullanıcı kutuyu işaretledi ve menüde hiçbir
    // şey görmedi; sebebini bilmeden ayarı açıp kapatmayı denerdi.
    ShowMessage(m_instance, m_window, Loc::Str(IDS_SHELL_FAILED),
                MessageIcon::Warning);
    m_settings.shellContextMenu = IsShellMenuRegistered();
}

void App::ShowSettings() {
    if (m_busy) {
        return;
    }
    m_busy = true;
    // KISAYOLLAR PENCERE AÇIKKEN ASKIYA ALINIR: kayıtlı bir kombinasyon
    // ayarlardaki kısayol kutusuna hiç ulaşmaz, Windows onu doğrudan bize
    // WM_HOTKEY olarak gönderir. Kullanıcı Ctrl+Shift+S'yi başka bir eyleme
    // taşımak isteyince kutu kımıldamıyor, eskisini silmişse de boş kalıyordu.
    // (Kutu ayrıca alçak seviye kanca kurar; bu, kanca kurulamazsa da doğru
    // davranmamızı sağlar.)
    m_hotkeys.UnregisterAll();
    Settings edited = m_settings;
    const bool accepted = ShowSettingsWindow(m_instance, edited);
    m_busy = false;
    if (!accepted) {
        (void)m_hotkeys.Apply(m_window, m_settings);   // eskiler geri gelir
        return;
    }

    // NE DEĞİŞTİĞİ ÖNEMLİ: dili ve temayı her onayda yeniden kurmak, hiçbir
    // şeye dokunulmadan kapatılan bir ayar penceresinden sonra bile bütün
    // pencerelerin yeniden çizilmesi demek olurdu.
    const bool languageChanged = edited.language != m_settings.language;
    const bool themeChanged = edited.theme != m_settings.theme;
    const bool shellChanged = edited.shellContextMenu != m_settings.shellContextMenu;
    m_settings = edited;

    if (!m_settings.Save(SettingsStore::ForApp())) {
        LogV(L"Ayarlar kaydedilemedi");
    }
    if (languageChanged) {
        Loc::SetLanguage(m_settings.language);
    }

    // KABUK FİİLİ AYAR DEPOSUNA DEĞİL, KAYIT DEFTERİNE yazılır; Settings'teki
    // alan yalnızca kullanıcının isteğidir. Dil değiştiyse fiil de yeniden
    // kaydedilir: menüdeki metin kayıt anındaki dilde donmuştu.
    if (shellChanged || (languageChanged && m_settings.shellContextMenu)) {
        ApplyShellMenuSetting();
    }
    if (themeChanged) {
        theme::SetMode(theme::ModeFromString(m_settings.theme.c_str()));
        m_tray.RefreshTheme();
    }
    m_history.SetLimit(m_settings.historyLimit);

    // KISAYOLLAR HER ZAMAN YENİDEN UYGULANIR: kullanıcı bir kısayolu
    // değiştirmediyse bile eski kayıt duruyor olabilir ve yeni bir kısayol
    // eskisiyle çakışırsa sessizce kaydedilemezdi.
    const int failed = m_hotkeys.Apply(m_window, m_settings);
    if (failed > 0) {
        LogV(L"%d kısayol kaydedilemedi", failed);
        ReportHotkeyFailures();
    }
}

void App::ReportHotkeyFailures() {
    // HANGİSİNİN ÇALIŞMADIĞI SÖYLENİR, kaç tane olduğu değil: "2 kısayol
    // kaydedilemedi" kullanıcıya hangi tuşu değiştireceğini söylemez.
    // RegisterHotKey'in başarısızlığı bir hata değil, bir ÇAKIŞMADIR — başka
    // bir uygulama o kombinasyonu almıştır ve çözüm kullanıcıdadır.
    std::wstring list;
    for (int slot = 0; slot < kHotkeySlots; ++slot) {
        const HotkeyBinding& binding = m_settings.hotkeys[slot];
        // EYLEMSİZ YUVA HİÇ KAYDEDİLMEZ ve bir çakışma değildir: eylemini
        // seçmemiş bir tuşu "başka uygulama almış" diye bildirmek yalan olurdu.
        if (!binding.key.assigned() || binding.action == HotkeyAction::None ||
            m_hotkeys.IsRegistered(HOTKEY_SLOT_FIRST + slot)) {
            continue;
        }
        list += L"\n    ";
        list += Loc::Str(settings_ui::HotkeyActionLabel(binding.action));
        list += L"  —  ";
        list += HotkeyText(binding.key);
    }

    if (list.empty()) {
        // Yalnızca Print Screen kaydedilememiş olabilir; onun için ayrı bir
        // uyarı doğru değil, çünkü Print Screen'i başka bir ekran alıntısı
        // aracının alması çok yaygın ve kullanıcının seçtiği bir tuş değil.
        return;
    }

    ShowMessage(m_instance, m_window, Loc::Str(IDS_HOTKEY_FAILED) + list,
                MessageIcon::Warning);
}

}  // namespace crisp
