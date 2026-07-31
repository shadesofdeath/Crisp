// TestFramework.h — Bağımlılıksız, kendini kaydeden minik test koşucusu.
//
// NEDEN HAZIR BİR ÇATI DEĞİL: projenin kuralı harici bağımlılık olmamasıdır ve
// GoogleTest'i yalnızca testler için getirmek, tek dosya .exe hedefini
// korumakla çelişmese de depoyu bir paket yöneticisine bağlardı. İhtiyaç
// duyulan yüzey küçük: kaydet, koş, karşılaştır, raporla.
#pragma once

#include <windows.h>

#include <cstdio>

namespace test {

using TestFn = void (*)();

// Statik nesne olarak oluşturulur; main çalışmadan önce kendini kaydeder.
struct Registrar {
    Registrar(const wchar_t* suite, const wchar_t* name, TestFn fn) noexcept;
};

// Bir doğrulama başarısız olduğunda çağrılır. Test durdurulmaz — aynı testteki
// diğer doğrulamalar da koşar, böylece tek çalıştırmada daha çok bilgi çıkar.
void ReportFailure(const char* file, int line, const wchar_t* message);

// Başarısız doğrulama sayısını döndürür (0 = hepsi geçti).
[[nodiscard]] int RunAll();

// Testlerin geçici dosya yazabileceği, koşu sonunda silinen klasör.
[[nodiscard]] const wchar_t* TempDirectory();

}  // namespace test

// ---------------------------------------------------------------------------
// Makrolar
// ---------------------------------------------------------------------------

#define CRISP_TEST(suite, name)                                               \
    static void suite##_##name##_body();                                      \
    namespace {                                                               \
    const ::test::Registrar suite##_##name##_registrar{L## #suite, L## #name,  \
                                                       &suite##_##name##_body};\
    }                                                                          \
    static void suite##_##name##_body()

#define CHECK(condition)                                                      \
    do {                                                                      \
        if (!(condition)) {                                                   \
            ::test::ReportFailure(__FILE__, __LINE__, L"CHECK: " L## #condition); \
        }                                                                     \
    } while (0)

// Sayısal karşılaştırma; başarısızlıkta iki değeri de yazar, çünkü "false"
// tek başına hata ayıklamaya yetmez.
#define CHECK_EQ(actual, expected)                                            \
    do {                                                                      \
        const long long check_a_ = static_cast<long long>(actual);            \
        const long long check_b_ = static_cast<long long>(expected);          \
        if (check_a_ != check_b_) {                                           \
            wchar_t check_msg_[256];                                          \
            ::swprintf_s(check_msg_,                                          \
                         L"CHECK_EQ(%hs, %hs): %lld != %lld", #actual,        \
                         #expected, check_a_, check_b_);                      \
            ::test::ReportFailure(__FILE__, __LINE__, check_msg_);            \
        }                                                                     \
    } while (0)

// PARAMETRE ADLARI ÖNEMLİ: önişlemci `.` işaretini üye erişimi olarak
// anlamaz, yalnızca belirteç değiştirir. Parametre `x` adını taşısaydı gövdedeki
// `check_p_.x` de değiştirilir ve `check_p_.516` gibi bir şey ortaya çıkardı.
// Bu yüzden hiçbir parametre, kullanılan üye adlarıyla çakışmaz.
#define CHECK_RECT(rect, expectL, expectT, expectR, expectB)                  \
    do {                                                                      \
        const RECT check_r_ = (rect);                                         \
        if (check_r_.left != (expectL) || check_r_.top != (expectT) ||        \
            check_r_.right != (expectR) || check_r_.bottom != (expectB)) {    \
            wchar_t check_msg_[256];                                          \
            ::swprintf_s(check_msg_,                                          \
                         L"CHECK_RECT(%hs): (%ld,%ld,%ld,%ld) != (%ld,%ld,%ld,%ld)", \
                         #rect, check_r_.left, check_r_.top, check_r_.right,  \
                         check_r_.bottom, static_cast<LONG>(expectL),         \
                         static_cast<LONG>(expectT), static_cast<LONG>(expectR), \
                         static_cast<LONG>(expectB));                         \
            ::test::ReportFailure(__FILE__, __LINE__, check_msg_);            \
        }                                                                     \
    } while (0)

#define CHECK_POINT(point, expectX, expectY)                                  \
    do {                                                                      \
        const POINT check_p_ = (point);                                       \
        if (check_p_.x != (expectX) || check_p_.y != (expectY)) {             \
            wchar_t check_msg_[256];                                          \
            ::swprintf_s(check_msg_, L"CHECK_POINT(%hs): (%ld,%ld) != (%ld,%ld)", \
                         #point, check_p_.x, check_p_.y,                      \
                         static_cast<LONG>(expectX),                          \
                         static_cast<LONG>(expectY));                         \
            ::test::ReportFailure(__FILE__, __LINE__, check_msg_);            \
        }                                                                     \
    } while (0)

#define CHECK_STR(actual, expected)                                           \
    do {                                                                      \
        const std::wstring& check_a_ = (actual);                              \
        const std::wstring check_b_{expected};                                \
        if (check_a_ != check_b_) {                                           \
            wchar_t check_msg_[512];                                          \
            ::swprintf_s(check_msg_, L"CHECK_STR(%hs): \"%s\" != \"%s\"",     \
                         #actual, check_a_.c_str(), check_b_.c_str());        \
            ::test::ReportFailure(__FILE__, __LINE__, check_msg_);            \
        }                                                                     \
    } while (0)
