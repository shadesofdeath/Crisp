// main.cpp — Giriş noktası, tek örnek kilidi.
#include "App.h"
#include "Localization.h"
#include "Util.h"
#include "resource.h"

#include <shellapi.h>
#include <windows.h>

#include <string>

namespace {

// Komut satırını ayrıştırır: ilk tanınan bayrak bir eylem, ilk bayraksız
// argüman bir dosya yoludur.
//
// NEDEN GEREKLİ: `Crisp.exe -region` bir betikten ya da klavye kısayolundan
// yakalama başlatmayı, `Crisp.exe foto.png` Explorer'ın "birlikte aç"
// listesinden düzenleyicide açmayı mümkün kılar. Şimdiye kadar komut satırı
// hiç okunmuyordu.
struct Arguments {
    crisp::HotkeyAction action = crisp::HotkeyAction::None;
    std::wstring file;
};

[[nodiscard]] Arguments ParseCommandLine() {
    Arguments parsed;
    int count = 0;
    LPWSTR* argv = ::CommandLineToArgvW(::GetCommandLineW(), &count);
    if (argv == nullptr) {
        return parsed;
    }
    for (int i = 1; i < count; ++i) {
        if (argv[i][0] == L'-' || argv[i][0] == L'/') {
            const crisp::HotkeyAction action = crisp::ActionFromArgument(argv[i]);
            if (action != crisp::HotkeyAction::None) {
                parsed.action = action;
            }
            continue;
        }
        if (parsed.file.empty()) {
            parsed.file = argv[i];
        }
    }
    ::LocalFree(argv);
    return parsed;
}

}  // namespace

int WINAPI wWinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE, _In_ PWSTR,
                    _In_ int) {
    const Arguments arguments = ParseCommandLine();

    // Kilit süreç ömrü boyunca yığında tutulur; yıkıcı CloseHandle çağırır.
    const crisp::unique_handle instanceLock{
        ::CreateMutexW(nullptr, TRUE, crisp::kSingleInstanceMutex)};
    if (::GetLastError() == ERROR_ALREADY_EXISTS) {
        // İkinci örnek: çalışan örneğe isteneni yaptırıp çekilir. Kullanıcı
        // muhtemelen kısayolun çalışmadığını sanıp exe'ye tekrar tıkladı ya da
        // bir dosyayı Crisp ile açtı; hiçbir şey yapmadan kapanmak onu
        // sessizce yanıltırdı.
        const HWND existing = crisp::App::FindExistingInstanceWindow();
        if (existing != nullptr) {
            if (!arguments.file.empty()) {
                // WM_COPYDATA SENKRONDUR ve veriyi karşı sürece kopyalar;
                // PostMessage ile gönderilen bir işaretçi, bu süreç kapandığı
                // an geçersiz olurdu.
                COPYDATASTRUCT data{};
                data.cbData = static_cast<DWORD>(
                    (arguments.file.size() + 1) * sizeof(wchar_t));
                data.lpData = const_cast<wchar_t*>(arguments.file.c_str());
                ::SendMessageW(existing, WM_COPYDATA, 0,
                               reinterpret_cast<LPARAM>(&data));
            } else {
                ::PostMessageW(existing, WM_COMMAND,
                               crisp::CommandForAction(arguments.action), 0);
            }
        }
        return 0;
    }

    // DİL, İLK HATA İLETİSİNDEN ÖNCE kurulur. Loc::Str, Initialize
    // çağrılmadan BOŞ dize döndürür; aşağıdaki iki hata kutusu bu yüzden
    // metinsiz açılıyordu — yani başlatma başarısızlığı boş bir pencere
    // olarak görünüyordu. Ayarlar henüz okunmadığı için "auto" kullanılır;
    // App::Initialize dili kullanıcının seçimiyle yeniden kurar.
    crisp::Loc::Initialize(instance, L"auto");

    // COM, WIC (PNG kodlama) ve kabuk klasörü sorguları için gerekli.
    const crisp::com_scope com;
    if (!com.ok()) {
        ::MessageBoxW(nullptr, crisp::Loc::Str(IDS_COM_FAILED).c_str(),
                      L"Crisp", MB_OK | MB_ICONERROR);
        return 1;
    }

    crisp::App app;
    if (!app.Initialize(instance)) {
        ::MessageBoxW(nullptr, crisp::Loc::Str(IDS_START_FAILED).c_str(),
                      L"Crisp", MB_OK | MB_ICONERROR);
        return 1;
    }

    // İSTEK MESAJ DÖNGÜSÜ BAŞLAMADAN ÇALIŞTIRILMAZ: yakalama ve düzenleyici
    // kendi döngülerini işletiyor ve buradan doğrudan çağırmak, tepsi simgesi
    // daha görünmeden bir kaplama açardı. Kendimize gönderilen mesaj, döngü
    // ayağa kalktıktan sonra işlenir.
    if (!arguments.file.empty()) {
        app.SetExitAfterFile(true);
        COPYDATASTRUCT data{};
        data.cbData =
            static_cast<DWORD>((arguments.file.size() + 1) * sizeof(wchar_t));
        data.lpData = const_cast<wchar_t*>(arguments.file.c_str());
        ::SendMessageW(crisp::App::FindExistingInstanceWindow(), WM_COPYDATA, 0,
                       reinterpret_cast<LPARAM>(&data));
    } else if (arguments.action != crisp::HotkeyAction::None) {
        ::PostMessageW(crisp::App::FindExistingInstanceWindow(), WM_COMMAND,
                       crisp::CommandForAction(arguments.action), 0);
    }

    return app.Run();
}
