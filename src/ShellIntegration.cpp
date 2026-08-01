// ShellIntegration.cpp — bkz. ShellIntegration.h.
#include "ShellIntegration.h"

#include "Util.h"

#include <shlobj.h>

namespace crisp {
namespace {

constexpr const wchar_t* kImageAssociations =
    L"Software\\Classes\\SystemFileAssociations\\image\\shell";

[[nodiscard]] std::wstring VerbKey(const wchar_t* verb) {
    std::wstring path = kImageAssociations;
    path += L'\\';
    path += verb;
    return path;
}

[[nodiscard]] std::wstring CommandKey(const wchar_t* verb) {
    return VerbKey(verb) + L"\\command";
}

// "C:\...\Crisp.exe" "%1" — TIRNAKLAR ŞART: boşluk içeren bir yolda ya da
// boşluk içeren bir dosya adında tırnaksız komut, argümanı ikiye böler ve
// Explorer yanlış dosyayı açtırır.
[[nodiscard]] std::wstring CommandLine() {
    const std::wstring exe = ModulePath();
    if (exe.empty()) {
        return std::wstring();
    }
    return L'"' + exe + L"\" \"%1\"";
}

[[nodiscard]] bool ReadValue(const std::wstring& key, const wchar_t* name,
                             std::wstring& out) {
    DWORD bytes = 0;
    LSTATUS status = ::RegGetValueW(HKEY_CURRENT_USER, key.c_str(), name,
                                    RRF_RT_REG_SZ, nullptr, nullptr, &bytes);
    if (status != ERROR_SUCCESS || bytes < sizeof(wchar_t)) {
        return false;
    }
    std::wstring buffer(bytes / sizeof(wchar_t), L'\0');
    status = ::RegGetValueW(HKEY_CURRENT_USER, key.c_str(), name, RRF_RT_REG_SZ,
                            nullptr, buffer.data(), &bytes);
    if (status != ERROR_SUCCESS) {
        return false;
    }
    const size_t nul = buffer.find(L'\0');
    if (nul != std::wstring::npos) {
        buffer.resize(nul);
    }
    out = std::move(buffer);
    return true;
}

[[nodiscard]] bool WriteValue(const std::wstring& key, const wchar_t* name,
                              const std::wstring& value) {
    const DWORD bytes =
        static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t));
    return ::RegSetKeyValueW(HKEY_CURRENT_USER, key.c_str(), name, REG_SZ,
                             value.c_str(), bytes) == ERROR_SUCCESS;
}

// Explorer, ilişkilendirme önbelleğini süreç ömrü boyunca tutar; bildirim
// olmadan yeni fiil bir sonraki oturuma kadar görünmezdi.
void NotifyShell() {
    ::SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
}

}  // namespace

bool IsShellMenuRegistered(const wchar_t* verb) {
    std::wstring command;
    return ReadValue(CommandKey(verb), nullptr, command) && !command.empty();
}

bool ShellMenuPathIsCurrent(const wchar_t* verb) {
    std::wstring command;
    if (!ReadValue(CommandKey(verb), nullptr, command)) {
        return false;
    }
    const std::wstring expected = CommandLine();
    return !expected.empty() && command == expected;
}

bool RegisterShellMenu(const std::wstring& label, const wchar_t* verb) {
    const std::wstring command = CommandLine();
    if (command.empty()) {
        LogV(L"Kabuk fiili kaydedilemedi: exe yolu okunamadı");
        return false;
    }

    const std::wstring key = VerbKey(verb);
    bool ok = WriteValue(key, nullptr, label);

    // Simge menüde metnin yanında görünür. ",0" exe'nin ilk simgesidir.
    ok = WriteValue(key, L"Icon", ModulePath() + L",0") && ok;
    ok = WriteValue(CommandKey(verb), nullptr, command) && ok;

    if (!ok) {
        LogV(L"Kabuk fiili kaydedilemedi (hata %lu)", ::GetLastError());
        // YARIM KAYIT BIRAKILMAZ: komutu olmayan bir fiil menüde görünür ama
        // tıklandığında hiçbir şey yapmaz.
        (void)UnregisterShellMenu(verb);
        return false;
    }

    NotifyShell();
    return true;
}

bool UnregisterShellMenu(const wchar_t* verb) {
    const LSTATUS status =
        ::RegDeleteTreeW(HKEY_CURRENT_USER, VerbKey(verb).c_str());
    // ZATEN YOKSA BAŞARIDIR: "kayıtlı olmasın" isteği zaten karşılanmış.
    const bool ok = status == ERROR_SUCCESS || status == ERROR_FILE_NOT_FOUND;
    if (ok) {
        NotifyShell();
    } else {
        LogV(L"Kabuk fiili kaldırılamadı (hata %ld)", status);
    }
    return ok;
}

}  // namespace crisp
