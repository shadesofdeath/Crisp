// SettingsStore.cpp — Ayarların saklandığı yer: kayıt defteri ya da .ini.
//
// AYRI DOSYA: doğrulama ve göç mantığıyla aynı dosyada Settings.cpp ev
// kuralının 400 satır sınırını aşıyordu (docs §9). Ayrım işlevsel: burası
// "ad → değer" alışverişinin NASIL yapıldığı, Settings.cpp ise HANGİ değerin
// ne anlama geldiği.
#include "Settings.h"

#include "Util.h"

#include <string>

namespace crisp {
namespace {

constexpr const wchar_t* kRegistryPath = L"Software\\ShadesOfDeath\\Crisp";
constexpr const wchar_t* kIniSection   = L"Crisp";
constexpr const wchar_t* kIniFileName  = L"Crisp.ini";

// Okunamayan bir değeri "yok" saymak yerine hata döndürmek çağıranı her yerde
// iki dallı yapardı; tek bir sentinel yeter.
constexpr unsigned kUnsignedMissing = 0xFFFFFFFFu;

}  // namespace

// ---------------------------------------------------------------------------
// SettingsStore
// ---------------------------------------------------------------------------

std::wstring SettingsStore::PortableIniPath() {
    std::wstring directory = ModuleDirectory();
    if (directory.empty()) {
        return std::wstring();
    }
    directory += L'\\';
    directory += kIniFileName;
    return directory;
}

bool SettingsStore::PortableModeActive() {
    const std::wstring path = PortableIniPath();
    if (path.empty()) {
        return false;
    }
    const DWORD attributes = ::GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES &&
           (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

SettingsStore SettingsStore::ForApp() {
    if (PortableModeActive()) {
        return SettingsStore{Backend::IniFile, PortableIniPath()};
    }
    return SettingsStore{Backend::Registry, std::wstring()};
}

SettingsStore SettingsStore::ForFile(std::wstring path) {
    return SettingsStore{Backend::IniFile, std::move(path)};
}

bool SettingsStore::ReadString(const wchar_t* name, std::wstring& out) const {
    if (m_backend == Backend::Registry) {
        DWORD bytes = 0;
        DWORD type = REG_NONE;
        LSTATUS status = ::RegGetValueW(HKEY_CURRENT_USER, kRegistryPath, name,
                                        RRF_RT_REG_SZ, &type, nullptr, &bytes);
        if (status != ERROR_SUCCESS || bytes < sizeof(wchar_t)) {
            return false;
        }

        std::wstring buffer(bytes / sizeof(wchar_t), L'\0');
        status = ::RegGetValueW(HKEY_CURRENT_USER, kRegistryPath, name,
                                RRF_RT_REG_SZ, nullptr, buffer.data(), &bytes);
        if (status != ERROR_SUCCESS) {
            return false;
        }
        // RegGetValueW sonlandırıcıyı da sayar; onu dizenin parçası bırakmak
        // sonradan yol birleştirmede görünmez bir NUL'a yol açar.
        const size_t nul = buffer.find(L'\0');
        if (nul != std::wstring::npos) {
            buffer.resize(nul);
        }
        out = std::move(buffer);
        return true;
    }

    // .ini: değer yoksa sentinel geri gelir, böylece "boş dize" ile "yok"
    // birbirinden ayrılır.
    constexpr const wchar_t* kMissing = L"\x01";
    wchar_t buffer[1024];
    const DWORD length = ::GetPrivateProfileStringW(
        kIniSection, name, kMissing, buffer,
        static_cast<DWORD>(std::size(buffer)), m_path.c_str());
    if (length == 0 || ::wcscmp(buffer, kMissing) == 0) {
        return false;
    }
    out.assign(buffer, length);
    return true;
}

bool SettingsStore::ReadUnsigned(const wchar_t* name, unsigned& out) const {
    if (m_backend == Backend::Registry) {
        DWORD value = 0;
        DWORD bytes = sizeof(value);
        const LSTATUS status =
            ::RegGetValueW(HKEY_CURRENT_USER, kRegistryPath, name, RRF_RT_REG_DWORD,
                           nullptr, &value, &bytes);
        if (status != ERROR_SUCCESS) {
            return false;
        }
        out = value;
        return true;
    }

    const UINT value = ::GetPrivateProfileIntW(kIniSection, name,
                                               static_cast<INT>(kUnsignedMissing),
                                               m_path.c_str());
    if (value == kUnsignedMissing) {
        return false;
    }
    out = value;
    return true;
}

bool SettingsStore::ReadBool(const wchar_t* name, bool& out) const {
    unsigned value = 0;
    if (!ReadUnsigned(name, value)) {
        return false;
    }
    out = value != 0;
    return true;
}

bool SettingsStore::Prepare() const {
    if (m_backend == Backend::Registry) {
        unique_hkey key;
        return ::RegCreateKeyExW(HKEY_CURRENT_USER, kRegistryPath, 0, nullptr, 0,
                                 KEY_SET_VALUE, nullptr, key.put(),
                                 nullptr) == ERROR_SUCCESS;
    }

    if (m_path.empty()) {
        return false;
    }
    const size_t slash = m_path.find_last_of(L'\\');
    if (slash != std::wstring::npos) {
        if (!EnsureDirectory(m_path.substr(0, slash))) {
            return false;
        }
    }
    // WritePrivateProfileStringW dosyayı ilk yazmada kendi oluşturur; ayrıca
    // dokunmaya gerek yok.
    return true;
}

bool SettingsStore::WriteString(const wchar_t* name, const std::wstring& value) const {
    if (m_backend == Backend::Registry) {
        const DWORD bytes = static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t));
        return ::RegSetKeyValueW(HKEY_CURRENT_USER, kRegistryPath, name, REG_SZ,
                                 value.c_str(), bytes) == ERROR_SUCCESS;
    }
    return ::WritePrivateProfileStringW(kIniSection, name, value.c_str(),
                                        m_path.c_str()) != FALSE;
}

bool SettingsStore::WriteUnsigned(const wchar_t* name, unsigned value) const {
    if (m_backend == Backend::Registry) {
        const DWORD dword = value;
        return ::RegSetKeyValueW(HKEY_CURRENT_USER, kRegistryPath, name, REG_DWORD,
                                 &dword, sizeof(dword)) == ERROR_SUCCESS;
    }
    return WriteString(name, std::to_wstring(value));
}

bool SettingsStore::WriteBool(const wchar_t* name, bool value) const {
    return WriteUnsigned(name, value ? 1u : 0u);
}

void SettingsStore::Flush() const {
    if (m_backend == Backend::IniFile) {
        // nullptr'lı çağrı önbelleği diske indirir.
        ::WritePrivateProfileStringW(nullptr, nullptr, nullptr, m_path.c_str());
    }
}

}  // namespace crisp
