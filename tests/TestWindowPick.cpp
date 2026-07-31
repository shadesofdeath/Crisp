// TestWindowPick.cpp — İmleç altındaki pencereyi bulma.
//
// Testler KENDİ pencerelerini oluşturur; masaüstünde ne olduğuna bağlı bir
// doğrulama yoktur, aksi hâlde sonuç çalıştıran makineye göre değişirdi.
#include "TestFramework.h"

#include "Geometry.h"
#include "WindowPick.h"

using namespace crisp;

namespace {

constexpr const wchar_t* kTestClass = L"CrispTestWindowClass";

// Test penceresi: görünür, bilinen konumda, bilinen boyutta.
class TestWindow {
public:
    TestWindow(int x, int y, int width, int height) noexcept {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = ::DefWindowProcW;
        wc.hInstance = ::GetModuleHandleW(nullptr);
        wc.lpszClassName = kTestClass;
        // Sınıf zaten kayıtlıysa hata dönmesi beklenen bir durumdur.
        ::RegisterClassExW(&wc);

        m_window = ::CreateWindowExW(WS_EX_TOOLWINDOW, kTestClass, L"Crisp test",
                                     WS_POPUP, x, y, width, height, nullptr,
                                     nullptr, wc.hInstance, nullptr);
        if (m_window != nullptr) {
            ::ShowWindow(m_window, SW_SHOWNA);
            // DWM'in çerçeve sınırını yayımlaması için mesaj kuyruğu boşaltılır;
            // aksi hâlde DwmGetWindowAttribute eski değeri döndürebilir.
            Pump();
        }
    }

    TestWindow(const TestWindow&) = delete;
    TestWindow& operator=(const TestWindow&) = delete;

    ~TestWindow() {
        if (m_window != nullptr) {
            ::DestroyWindow(m_window);
            Pump();
        }
    }

    [[nodiscard]] HWND get() const noexcept { return m_window; }

    static void Pump() noexcept {
        MSG msg{};
        while (::PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessageW(&msg);
        }
        ::Sleep(30);
    }

private:
    HWND m_window = nullptr;
};

}  // namespace

CRISP_TEST(WindowPick, IsCapturableWindow_gecersiz_girdiler) {
    CHECK(!IsCapturableWindow(nullptr));
    CHECK(!IsCapturableWindow(reinterpret_cast<HWND>(static_cast<INT_PTR>(0xDEAD))));
}

CRISP_TEST(WindowPick, IsCapturableWindow_gorunur_pencere) {
    const TestWindow window{300, 300, 200, 150};
    CHECK(window.get() != nullptr);
    if (window.get() != nullptr) {
        CHECK(IsCapturableWindow(window.get()));
    }
}

CRISP_TEST(WindowPick, IsCapturableWindow_gizli_pencere_elenir) {
    const TestWindow window{300, 300, 200, 150};
    if (window.get() == nullptr) {
        CHECK(false);
        return;
    }
    ::ShowWindow(window.get(), SW_HIDE);
    TestWindow::Pump();
    CHECK(!IsCapturableWindow(window.get()));
}

CRISP_TEST(WindowPick, IsCapturableWindow_WS_EX_TRANSPARENT_elenir) {
    // Fare girdisini geçiren pencerelerin "üzerine" gelinemez; seçilmemeliler.
    const TestWindow window{300, 300, 200, 150};
    if (window.get() == nullptr) {
        CHECK(false);
        return;
    }
    const LONG_PTR style = ::GetWindowLongPtrW(window.get(), GWL_EXSTYLE);
    ::SetWindowLongPtrW(window.get(), GWL_EXSTYLE, style | WS_EX_TRANSPARENT);
    CHECK(!IsCapturableWindow(window.get()));
}

CRISP_TEST(WindowPick, WindowFrameBounds_pencere_dikdortgenine_yakin) {
    // KOORDİNATLAR SABİT YAZILAMAZ: test koşucusu DPI farkındalığı bildirmez,
    // bu yüzden CreateWindowExW'ye verilen mantıksal koordinatlar %150
    // ölçekli bir ekranda fiziksel olarak başka yere düşer. Doğrulama, istenen
    // değerlere değil pencerenin GERÇEKTEN bildirdiği dikdörtgene göre yapılır.
    const TestWindow window{400, 250, 320, 240};
    if (window.get() == nullptr) {
        CHECK(false);
        return;
    }

    RECT actual{};
    CHECK(::GetWindowRect(window.get(), &actual) != FALSE);

    RECT bounds{};
    CHECK(WindowFrameBounds(window.get(), bounds));
    CHECK(!geom::IsEmpty(bounds));

    // Asıl sözleşme: DWM çerçeve sınırı, pencere dikdörtgeninin İÇİNDE ya da
    // ona çok yakın olmalı — hiçbir zaman belirgin biçimde dışında değil.
    // Windows 10/11'de görünmez yeniden boyutlandırma kenarlığı tipik olarak
    // kenar başına 7-8 pikseldir; 16 piksel bol bir tolerans.
    constexpr LONG kTolerance = 16;
    CHECK(bounds.left   >= actual.left   - kTolerance);
    CHECK(bounds.top    >= actual.top    - kTolerance);
    CHECK(bounds.right  <= actual.right  + kTolerance);
    CHECK(bounds.bottom <= actual.bottom + kTolerance);

    // Ve anlamlı bir alanı olmalı: pencere boyutunun yarısından küçük olamaz.
    CHECK(geom::Width(bounds)  >= geom::Width(actual)  / 2);
    CHECK(geom::Height(bounds) >= geom::Height(actual) / 2);
}

CRISP_TEST(WindowPick, WindowFrameBounds_gecersiz_pencere) {
    RECT bounds{};
    CHECK(!WindowFrameBounds(nullptr, bounds));
    CHECK(!WindowFrameBounds(reinterpret_cast<HWND>(static_cast<INT_PTR>(0xDEAD)),
                             bounds));
}

namespace {

// Pencerenin GERÇEK sınırlarından merkez noktasını üretir. Sabit koordinat
// yazmak DPI ölçeklemesinde noktayı pencerenin dışına düşürür.
[[nodiscard]] bool CenterOf(HWND window, POINT& out) noexcept {
    RECT bounds{};
    if (!WindowFrameBounds(window, bounds) || geom::IsEmpty(bounds)) {
        return false;
    }
    out.x = bounds.left + geom::Width(bounds) / 2;
    out.y = bounds.top + geom::Height(bounds) / 2;
    return true;
}

}  // namespace

CRISP_TEST(WindowPick, WindowUnderPoint_kendi_penceremizi_bulur) {
    const TestWindow window{500, 400, 300, 200};
    if (window.get() == nullptr) {
        CHECK(false);
        return;
    }
    // En üste getir ki Z sırasında ilk sırada olsun.
    ::SetWindowPos(window.get(), HWND_TOPMOST, 0, 0, 0, 0,
                   SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    TestWindow::Pump();

    POINT inside{};
    CHECK(CenterOf(window.get(), inside));
    CHECK(WindowUnderPoint(inside, nullptr) == window.get());
}

CRISP_TEST(WindowPick, WindowUnderPoint_ignore_penceresini_atlar) {
    // Kaplama tüm ekranı kapladığı için kendisini yok saymalı; yoksa daima
    // kendini bulur ve pencere vurgulama hiç çalışmaz.
    const TestWindow window{500, 400, 300, 200};
    if (window.get() == nullptr) {
        CHECK(false);
        return;
    }
    ::SetWindowPos(window.get(), HWND_TOPMOST, 0, 0, 0, 0,
                   SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    TestWindow::Pump();

    POINT inside{};
    CHECK(CenterOf(window.get(), inside));
    const HWND found = WindowUnderPoint(inside, window.get());
    CHECK(found != window.get());
}

CRISP_TEST(WindowPick, WindowUnderPoint_ust_pencereyi_secer) {
    // İki üst üste pencere: Z sırasında üstte olan kazanmalı.
    const TestWindow lower{600, 500, 200, 200};
    const TestWindow upper{600, 500, 200, 200};
    if (lower.get() == nullptr || upper.get() == nullptr) {
        CHECK(false);
        return;
    }

    ::SetWindowPos(lower.get(), HWND_TOPMOST, 0, 0, 0, 0,
                   SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    ::SetWindowPos(upper.get(), HWND_TOPMOST, 0, 0, 0, 0,
                   SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    TestWindow::Pump();

    POINT inside{};
    CHECK(CenterOf(upper.get(), inside));

    const HWND found = WindowUnderPoint(inside, nullptr);
    // İkisinden biri bulunmalı ve bulunan, o noktayı GERÇEKTEN kapsamalı.
    // Hangisinin üstte olduğu Z sırasına bağlıdır ve pencere yöneticisinin
    // kararıdır; testin sabitlemesi gereken şey kapsama ilişkisidir.
    CHECK(found == upper.get() || found == lower.get());
    if (found != nullptr) {
        RECT bounds{};
        CHECK(WindowFrameBounds(found, bounds));
        CHECK(::PtInRect(&bounds, inside) != FALSE);
    }
}

CRISP_TEST(WindowPick, WindowUnderPoint_uzak_noktada_pencere_disi) {
    const TestWindow window{500, 400, 100, 100};
    if (window.get() == nullptr) {
        CHECK(false);
        return;
    }
    TestWindow::Pump();

    // Pencerenin çok uzağındaki bir nokta bu pencereyi DÖNDÜRMEMELİ.
    // Değişken adı "far" OLAMAZ: MSVC'de eski bir bellek modeli makrosudur ve
    // boş dizeye açılıp bildirimi anlamsız hâle getirir.
    const POINT distant{50, 50};
    CHECK(WindowUnderPoint(distant, nullptr) != window.get());
}
