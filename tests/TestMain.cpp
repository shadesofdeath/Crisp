// TestMain.cpp — Test kayıt defteri ve koşucu.
#include "TestFramework.h"

#include <windows.h>

// WIN32_LEAN_AND_MEAN nedeniyle OLE başlıkları <windows.h> ile gelmez.
#include <objbase.h>

#include <cstdio>
#include <string>
#include <vector>

namespace test {
namespace {

struct Entry {
    const wchar_t* suite;
    const wchar_t* name;
    TestFn fn;
};

// STATİK BAŞLATMA SIRASI: Registrar nesneleri farklı çeviri birimlerinde ve
// aralarındaki sıra tanımsız. Kayıt defteri global bir std::vector olsaydı
// henüz kurulmamış bir vector'e push_back edilebilirdi. Fonksiyon içi static
// ise ilk erişimde kurulur, bu yüzden sıra sorunu ortadan kalkar.
std::vector<Entry>& Registry() {
    static std::vector<Entry> registry;
    return registry;
}

int g_failuresInCurrentTest = 0;
int g_totalFailures = 0;

std::wstring g_tempDirectory;

void SetConsoleToUtf8() {
    // Türkçe test adları ve hata metinleri konsolda bozulmasın.
    ::SetConsoleOutputCP(CP_UTF8);
}

std::wstring CreateTempDirectory() {
    wchar_t base[MAX_PATH];
    const DWORD length = ::GetTempPathW(static_cast<DWORD>(std::size(base)), base);
    if (length == 0 || length >= std::size(base)) {
        return std::wstring();
    }

    std::wstring path{base, length};
    path += L"crisp_tests_";
    path += std::to_wstring(::GetCurrentProcessId());

    if (!::CreateDirectoryW(path.c_str(), nullptr) &&
        ::GetLastError() != ERROR_ALREADY_EXISTS) {
        return std::wstring();
    }
    return path;
}

void RemoveTempDirectory(const std::wstring& path) {
    if (path.empty()) {
        return;
    }

    // Yalnızca doğrudan içindeki dosyalar; testler alt klasör oluşturursa
    // orası kasıtlıdır ve ayrıca temizlenir.
    std::wstring pattern = path + L"\\*";
    WIN32_FIND_DATAW data{};
    const HANDLE find = ::FindFirstFileW(pattern.c_str(), &data);
    if (find != INVALID_HANDLE_VALUE) {
        do {
            if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
                continue;
            }
            std::wstring file = path + L'\\' + data.cFileName;
            ::DeleteFileW(file.c_str());
        } while (::FindNextFileW(find, &data));
        ::FindClose(find);
    }
    ::RemoveDirectoryW(path.c_str());
}

}  // namespace

Registrar::Registrar(const wchar_t* suite, const wchar_t* name, TestFn fn) noexcept {
    Registry().push_back(Entry{suite, name, fn});
}

void ReportFailure(const char* file, int line, const wchar_t* message) {
    ++g_failuresInCurrentTest;
    ++g_totalFailures;

    // Yol tamamı yerine yalnızca dosya adı: satırlar okunaklı kalsın.
    const char* shortFile = file;
    for (const char* p = file; *p != '\0'; ++p) {
        if (*p == '\\' || *p == '/') {
            shortFile = p + 1;
        }
    }
    ::wprintf(L"      %hs:%d  %s\n", shortFile, line, message);
}

const wchar_t* TempDirectory() { return g_tempDirectory.c_str(); }

int RunAll() {
    SetConsoleToUtf8();
    g_tempDirectory = CreateTempDirectory();

    const auto& registry = Registry();
    ::wprintf(L"Crisp — %zu test\n", registry.size());
    ::wprintf(L"Geçici klasör: %s\n\n",
              g_tempDirectory.empty() ? L"(oluşturulamadı)" : g_tempDirectory.c_str());

    const wchar_t* currentSuite = nullptr;
    int passed = 0;
    int failed = 0;

    for (const Entry& entry : registry) {
        if (currentSuite == nullptr || ::wcscmp(currentSuite, entry.suite) != 0) {
            currentSuite = entry.suite;
            ::wprintf(L"%s\n", currentSuite);
        }

        g_failuresInCurrentTest = 0;
        entry.fn();

        if (g_failuresInCurrentTest == 0) {
            ::wprintf(L"  [ gecti ] %s\n", entry.name);
            ++passed;
        } else {
            ::wprintf(L"  [ HATA  ] %s  (%d dogrulama basarisiz)\n", entry.name,
                      g_failuresInCurrentTest);
            ++failed;
        }
    }

    RemoveTempDirectory(g_tempDirectory);

    ::wprintf(L"\n%d gecti, %d basarisiz, toplam %d dogrulama hatasi\n", passed,
              failed, g_totalFailures);
    return g_totalFailures;
}

}  // namespace test

int wmain() {
    // DPI FARKINDALIĞI ÜRÜNLE AYNI OLMALI.
    //
    // Crisp.exe manifestinde PerMonitorV2 bildirir. Test koşucusu bunu
    // bildirmezse Windows onu sanallaştırır: GetWindowRect MANTIKSAL, DWM ise
    // FİZİKSEL piksel döndürür ve %150 ölçekli bir ekranda ikisi 1,5 kat
    // ayrışır. Yakalama testleri de mantıksal koordinat verip fiziksel piksel
    // beklerdi. Bu çağrı olmadan testler ürünün çalıştığı koşulu değil,
    // sanallaştırılmış bir taklidini sınar.
    //
    // Manifest yerine çağrı kullanılır: test koşucusu bir .rc'ye ve dolayısıyla
    // kaynak derlemesine bağlanmasın diye.
    ::SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // Testlerin çoğu COM kullanır (WIC, kabuk klasörleri). Her testte ayrı
    // ayrı başlatmak yerine süreç ömrü boyunca bir kez.
    const HRESULT hr = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool ownsCom = SUCCEEDED(hr);

    const int failures = test::RunAll();

    if (ownsCom) {
        ::CoUninitialize();
    }
    return failures == 0 ? 0 : 1;
}
