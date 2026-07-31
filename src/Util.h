// Util.h — RAII sarmalayıcıları ve küçük ortak yardımcılar.
//
// EV KURALI: ham new/delete YOK, istisna fırlatma YOK. Kaynaklar aşağıdaki
// unique_* şablonlarıyla tutulur, hatalar HRESULT/bool olarak taşınır.
#pragma once

#include <windows.h>

// WIN32_LEAN_AND_MEAN, <windows.h> içinden OLE başlıklarını çıkarır; com_scope
// için CoInitializeEx/CoUninitialize açıkça istenmelidir.
#include <objbase.h>

#include <string>
#include <utility>

namespace crisp {

// ---------------------------------------------------------------------------
// unique_res — tek bir kaynak tipi için taşınabilir sahiplik
// ---------------------------------------------------------------------------
// Traits: static T invalid() ve static void close(T) sağlar. Tek bir şablonla
// HANDLE, HDC, HBITMAP, HKEY hepsi karşılanır; her biri için ayrı sınıf yazmak
// aynı beş satırı beş kez kopyalamak olurdu.
template <typename Traits>
class unique_res {
public:
    using value_type = typename Traits::value_type;

    unique_res() noexcept = default;
    explicit unique_res(value_type v) noexcept : m_value(v) {}

    unique_res(const unique_res&) = delete;
    unique_res& operator=(const unique_res&) = delete;

    unique_res(unique_res&& other) noexcept
        : m_value(std::exchange(other.m_value, Traits::invalid())) {}

    unique_res& operator=(unique_res&& other) noexcept {
        if (this != &other) {
            reset(std::exchange(other.m_value, Traits::invalid()));
        }
        return *this;
    }

    ~unique_res() { reset(); }

    [[nodiscard]] value_type get() const noexcept { return m_value; }
    [[nodiscard]] bool valid() const noexcept { return m_value != Traits::invalid(); }
    explicit operator bool() const noexcept { return valid(); }

    void reset(value_type v = Traits::invalid()) noexcept {
        if (m_value != Traits::invalid()) {
            Traits::close(m_value);
        }
        m_value = v;
    }

    // Çıkış parametresi bekleyen API'ler için (RegOpenKeyEx gibi).
    [[nodiscard]] value_type* put() noexcept {
        reset();
        return &m_value;
    }

    [[nodiscard]] value_type release() noexcept {
        return std::exchange(m_value, Traits::invalid());
    }

private:
    value_type m_value = Traits::invalid();
};

struct handle_traits {
    using value_type = HANDLE;
    static value_type invalid() noexcept { return nullptr; }
    static void close(value_type v) noexcept { ::CloseHandle(v); }
};

struct hkey_traits {
    using value_type = HKEY;
    static value_type invalid() noexcept { return nullptr; }
    static void close(value_type v) noexcept { ::RegCloseKey(v); }
};

struct hbitmap_traits {
    using value_type = HBITMAP;
    static value_type invalid() noexcept { return nullptr; }
    static void close(value_type v) noexcept { ::DeleteObject(v); }
};

struct hdc_delete_traits {
    using value_type = HDC;
    static value_type invalid() noexcept { return nullptr; }
    static void close(value_type v) noexcept { ::DeleteDC(v); }
};

using unique_handle = unique_res<handle_traits>;
using unique_hkey   = unique_res<hkey_traits>;
using unique_hbitmap = unique_res<hbitmap_traits>;
using unique_hdc    = unique_res<hdc_delete_traits>;

// GetDC/ReleaseDC çifti DeleteDC ile kapatılamaz: pencereyi de bilmek gerekir,
// bu yüzden ayrı ve daha küçük bir sınıf.
class window_dc {
public:
    window_dc() noexcept = default;
    explicit window_dc(HWND wnd) noexcept : m_wnd(wnd), m_dc(::GetDC(wnd)) {}

    window_dc(const window_dc&) = delete;
    window_dc& operator=(const window_dc&) = delete;

    ~window_dc() {
        if (m_dc != nullptr) {
            ::ReleaseDC(m_wnd, m_dc);
        }
    }

    [[nodiscard]] HDC get() const noexcept { return m_dc; }
    [[nodiscard]] bool valid() const noexcept { return m_dc != nullptr; }

private:
    HWND m_wnd = nullptr;
    HDC m_dc = nullptr;
};

// SelectObject'in eski nesneyi geri koyma zorunluluğunu otomatikleştirir.
class dc_selection {
public:
    dc_selection(HDC dc, HGDIOBJ obj) noexcept
        : m_dc(dc), m_previous(::SelectObject(dc, obj)) {}

    dc_selection(const dc_selection&) = delete;
    dc_selection& operator=(const dc_selection&) = delete;

    ~dc_selection() {
        if (m_previous != nullptr) {
            ::SelectObject(m_dc, m_previous);
        }
    }

private:
    HDC m_dc;
    HGDIOBJ m_previous;
};

// ---------------------------------------------------------------------------
// ComPtr — minimal, yalnızca ihtiyacımız olan kadarı
// ---------------------------------------------------------------------------
template <typename T>
class ComPtr {
public:
    ComPtr() noexcept = default;

    ComPtr(const ComPtr& other) noexcept : m_ptr(other.m_ptr) {
        if (m_ptr != nullptr) {
            m_ptr->AddRef();
        }
    }

    ComPtr(ComPtr&& other) noexcept : m_ptr(std::exchange(other.m_ptr, nullptr)) {}

    ComPtr& operator=(const ComPtr& other) noexcept {
        if (this != &other) {
            if (other.m_ptr != nullptr) {
                other.m_ptr->AddRef();
            }
            Release();
            m_ptr = other.m_ptr;
        }
        return *this;
    }

    ComPtr& operator=(ComPtr&& other) noexcept {
        if (this != &other) {
            Release();
            m_ptr = std::exchange(other.m_ptr, nullptr);
        }
        return *this;
    }

    ~ComPtr() { Release(); }

    [[nodiscard]] T* Get() const noexcept { return m_ptr; }
    T* operator->() const noexcept { return m_ptr; }
    explicit operator bool() const noexcept { return m_ptr != nullptr; }

    // Çıkış parametresi; önce mevcut referans bırakılır.
    [[nodiscard]] T** GetAddressOf() noexcept {
        Release();
        return &m_ptr;
    }

    [[nodiscard]] void** GetVoidAddressOf() noexcept {
        Release();
        return reinterpret_cast<void**>(&m_ptr);
    }

    void Release() noexcept {
        if (m_ptr != nullptr) {
            m_ptr->Release();
            m_ptr = nullptr;
        }
    }

private:
    T* m_ptr = nullptr;
};

// COM'u bu iş parçacığı için başlatır ve yıkımda kapatır. Zaten başlatılmışsa
// (RPC_E_CHANGED_MODE) kapatma YAPILMAZ: başkasının başlattığı apartmanı
// kapatmak sahibinin altındaki zemini çekerdi.
class com_scope {
public:
    com_scope() noexcept
        : m_hr(::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)) {}

    com_scope(const com_scope&) = delete;
    com_scope& operator=(const com_scope&) = delete;

    ~com_scope() {
        if (SUCCEEDED(m_hr)) {
            ::CoUninitialize();
        }
    }

    [[nodiscard]] bool ok() const noexcept {
        return SUCCEEDED(m_hr) || m_hr == RPC_E_CHANGED_MODE;
    }

private:
    HRESULT m_hr;
};

// ---------------------------------------------------------------------------
// Tanılama
// ---------------------------------------------------------------------------
// Yalnızca hata ayıklayıcı çıktısına yazar; sürüm derlemesinde de kalır çünkü
// maliyeti sıfıra yakın ve sahadaki bir sorunu DebugView ile izlemeyi sağlar.
void LogV(const wchar_t* format, ...);

#define CRISP_LOG_IF_FAILED(expr)                                            \
    do {                                                                     \
        const HRESULT crisp_hr_ = (expr);                                    \
        if (FAILED(crisp_hr_)) {                                             \
            ::crisp::LogV(L"HR FAIL 0x%08X @ %hs:%d", crisp_hr_, __FILE__,   \
                          __LINE__);                                         \
        }                                                                    \
    } while (0)

// ---------------------------------------------------------------------------
// Dize ve yol yardımcıları
// ---------------------------------------------------------------------------
[[nodiscard]] std::wstring ModulePath();
[[nodiscard]] std::wstring ModuleDirectory();

// "2026-07-31 14-22-05" — dosya adında güvenli, sıralanabilir zaman damgası.
// Windows dosya adlarında ':' yasak olduğu için saat ayracı '-' kullanılır.
[[nodiscard]] std::wstring TimestampForFileName();

// Var olmayan ara dizinleri de oluşturur. Zaten varsa true döner.
[[nodiscard]] bool EnsureDirectory(const std::wstring& path);

// "C:\a\b.png" zaten varsa "C:\a\b (2).png" döner; 999'a kadar dener.
[[nodiscard]] std::wstring MakeUniquePath(const std::wstring& desired);

}  // namespace crisp
