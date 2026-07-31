// Settings.cpp — bkz. Settings.h.
#include "Settings.h"

#include "Util.h"

#include <shlobj.h>

namespace crisp {
namespace {

constexpr const wchar_t* kRegistryPath = L"Software\\ShadesOfDeath\\Crisp";
constexpr const wchar_t* kIniSection   = L"Crisp";
constexpr const wchar_t* kIniFileName  = L"Crisp.ini";

// Okunamayan bir değeri "yok" saymak yerine hata döndürmek çağıranı her yerde
// iki dallı yapardı; tek bir sentinel yeter.
constexpr unsigned kUnsignedMissing = 0xFFFFFFFFu;

[[nodiscard]] unsigned ClampUnsigned(unsigned value, unsigned lo, unsigned hi) noexcept {
    return value < lo ? lo : (value > hi ? hi : value);
}

[[nodiscard]] std::wstring PicturesFolder() {
    PWSTR raw = nullptr;
    if (FAILED(::SHGetKnownFolderPath(FOLDERID_Pictures, 0, nullptr, &raw))) {
        return std::wstring();
    }
    std::wstring path{raw};
    ::CoTaskMemFree(raw);
    return path;
}

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

// ---------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------

void Settings::Clamp() {
    delaySeconds = ClampUnsigned(delaySeconds, 1u, 30u);

    // Hiçbir eylem seçili değilse araç yakalar ve sonucu sessizce atar.
    // Kullanıcı bunu isteyerek yapmış olamaz; en beklenen davranışa dönülür.
    if (!after.copyToClipboard && !after.saveToFile && !after.pinToScreen &&
        !after.openEditor) {
        after.copyToClipboard = true;
    }

    // Değiştirici tuşu olmayan kısayol, tek tuşa basınca yakalama başlatırdı.
    // MOD_WIN tek başına da kabul edilmez: Windows onu kendi ayırıyor.
    auto sanitize = [](Hotkey& h) {
        if (h.key != 0 && (h.modifiers & (MOD_CONTROL | MOD_ALT | MOD_SHIFT)) == 0) {
            h.key = 0;
        }
    };
    sanitize(hotkeyRegion);
    sanitize(hotkeyFullScreen);
    sanitize(hotkeyWindow);
    sanitize(hotkeyDelayed);
}

void Settings::Load(const SettingsStore& store) {
    store.ReadString(L"SaveFolder", saveFolder);

    store.ReadBool(L"CopyToClipboard", after.copyToClipboard);
    store.ReadBool(L"SaveToFile", after.saveToFile);
    store.ReadBool(L"PinToScreen", after.pinToScreen);
    store.ReadBool(L"OpenEditor", after.openEditor);

    store.ReadUnsigned(L"DelaySeconds", delaySeconds);
    store.ReadBool(L"ShowMagnifier", showMagnifier);
    store.ReadBool(L"ShowWindowHighlight", showWindowHighlight);
    store.ReadBool(L"PlayShutterSound", playShutterSound);

    auto readHotkey = [&store](const wchar_t* name, Hotkey& target) {
        unsigned packed = 0;
        if (store.ReadUnsigned(name, packed)) {
            target = Hotkey::unpack(packed);
        }
    };
    readHotkey(L"HotkeyRegion", hotkeyRegion);
    readHotkey(L"HotkeyFullScreen", hotkeyFullScreen);
    readHotkey(L"HotkeyWindow", hotkeyWindow);
    readHotkey(L"HotkeyDelayed", hotkeyDelayed);

    Clamp();
}

bool Settings::Save(const SettingsStore& store) const {
    if (!store.Prepare()) {
        return false;
    }

    bool ok = true;
    ok = store.WriteString(L"SaveFolder", saveFolder) && ok;

    ok = store.WriteBool(L"CopyToClipboard", after.copyToClipboard) && ok;
    ok = store.WriteBool(L"SaveToFile", after.saveToFile) && ok;
    ok = store.WriteBool(L"PinToScreen", after.pinToScreen) && ok;
    ok = store.WriteBool(L"OpenEditor", after.openEditor) && ok;

    ok = store.WriteUnsigned(L"DelaySeconds", delaySeconds) && ok;
    ok = store.WriteBool(L"ShowMagnifier", showMagnifier) && ok;
    ok = store.WriteBool(L"ShowWindowHighlight", showWindowHighlight) && ok;
    ok = store.WriteBool(L"PlayShutterSound", playShutterSound) && ok;

    ok = store.WriteUnsigned(L"HotkeyRegion", hotkeyRegion.packed()) && ok;
    ok = store.WriteUnsigned(L"HotkeyFullScreen", hotkeyFullScreen.packed()) && ok;
    ok = store.WriteUnsigned(L"HotkeyWindow", hotkeyWindow.packed()) && ok;
    ok = store.WriteUnsigned(L"HotkeyDelayed", hotkeyDelayed.packed()) && ok;

    store.Flush();
    return ok;
}

std::wstring Settings::EffectiveSaveFolder() const {
    if (!saveFolder.empty()) {
        return saveFolder;
    }
    std::wstring pictures = PicturesFolder();
    if (pictures.empty()) {
        return ModuleDirectory();
    }
    pictures += L"\\Crisp";
    return pictures;
}

}  // namespace crisp
