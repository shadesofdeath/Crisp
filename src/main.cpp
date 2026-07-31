// main.cpp — Giriş noktası, tek örnek kilidi.
#include "App.h"
#include "Localization.h"
#include "Util.h"
#include "resource.h"

#include <windows.h>

int WINAPI wWinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE, _In_ PWSTR,
                    _In_ int) {
    // Kilit süreç ömrü boyunca yığında tutulur; yıkıcı CloseHandle çağırır.
    const crisp::unique_handle instanceLock{
        ::CreateMutexW(nullptr, TRUE, crisp::kSingleInstanceMutex)};
    if (::GetLastError() == ERROR_ALREADY_EXISTS) {
        // İkinci örnek: çalışan örneğe bölge yakalaması yaptırıp çekilir.
        // Kullanıcı muhtemelen kısayolun çalışmadığını sanıp exe'ye tekrar
        // tıkladı; hiçbir şey yapmadan kapanmak onu sessizce yanıltırdı.
        const HWND existing = crisp::App::FindExistingInstanceWindow();
        if (existing != nullptr) {
            ::PostMessageW(existing, WM_COMMAND, IDM_CAPTURE_REGION, 0);
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

    return app.Run();
}
