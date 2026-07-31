cmake_minimum_required(VERSION 3.25)

# CMP0091: MSVC runtime library, CMAKE_MSVC_RUNTIME_LIBRARY üzerinden seçilir.
cmake_policy(SET CMP0091 NEW)

project(crisp
        VERSION 0.1.0
        DESCRIPTION "Screen capture tool for Windows"
        LANGUAGES CXX RC)

if(NOT MSVC)
    message(FATAL_ERROR "Crisp yalnızca MSVC ile derlenir.")
endif()

# --- Global derleme ayarları -------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Statik CRT — runtime kurulumu gerektirmeyen tek dosya .exe.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
endif()

option(CRISP_PACKAGED "MSIX paketli derleme" OFF)
option(CRISP_TESTS    "Test koşucusunu derle"  ON)

# Çekirdeği ve testleri, arayüz katmanı olmadan derleyebilmek için. Geliştirme
# sırasında çekirdeği tek başına yeşile boyamayı, CI'da ise pencere açmayan bir
# derleme yapmayı mümkün kılar.
option(CRISP_APP      "Uygulama hedefini derle" ON)

# ---------------------------------------------------------------------------
# crisp_core — kullanıcı arayüzü OLMAYAN her şey
# ---------------------------------------------------------------------------
# AYRIM NEDENİ: bu hedefteki hiçbir fonksiyon pencere oluşturmaz ve kullanıcı
# etkileşimi beklemez, bu yüzden hepsi bir konsol test koşucusundan doğrudan
# çağrılabilir. Yakalama, kodlama, pano ve geometri buradadır; yalnızca
# etkileşimli katman (kaplama, tepsi, uygulama döngüsü) dışarıda kalır.
set(CRISP_CORE_SOURCES
    src/Util.cpp
    src/Geometry.cpp
    src/Capture.cpp
    src/WindowPick.cpp
    src/ImageCodec.cpp
    src/ClipboardImage.cpp
    src/Settings.cpp
    src/OcrLayout.cpp
    src/ImageEffects.cpp
    src/ImageTransform.cpp
    src/Annotation.cpp
    src/History.cpp
    src/Sound.cpp
)

set(CRISP_CORE_HEADERS
    src/Util.h
    src/Geometry.h
    src/Capture.h
    src/WindowPick.h
    src/ImageCodec.h
    src/ClipboardImage.h
    src/Settings.h
    src/OcrLayout.h
    src/ImageEffects.h
    src/ImageTransform.h
    src/Annotation.h
    src/History.h
    src/Sound.h
)

add_library(crisp_core STATIC ${CRISP_CORE_SOURCES} ${CRISP_CORE_HEADERS})

target_include_directories(crisp_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/res"
)

target_compile_definitions(crisp_core PUBLIC
    UNICODE
    _UNICODE
    STRICT
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    $<$<BOOL:${CRISP_PACKAGED}>:CRISP_PACKAGED>
)

target_compile_options(crisp_core PUBLIC
    /W4             # uyarısız derlenmeli
    /permissive-
    /utf-8
    /Zc:__cplusplus
    /Zc:inline
    /EHsc
    /GS
    /Gy
    /Gw
    $<$<CONFIG:Release,MinSizeRel,RelWithDebInfo>:/GL>
    $<$<CONFIG:Debug>:/JMC>
)

target_link_libraries(crisp_core PUBLIC
    user32
    gdi32
    shell32
    advapi32
    shcore          # per-monitor DPI
    dwmapi          # DwmGetWindowAttribute — pencerenin gerçek çerçeve sınırı
    windowscodecs   # WIC — PNG kodlama/çözme
    ole32           # CoCreateInstance
    shlwapi
    uxtheme         # SetWindowTheme — koyu kaydırma çubuğu
)

# ---------------------------------------------------------------------------
# Crisp.exe
# ---------------------------------------------------------------------------
if(CRISP_APP)

set(CRISP_APP_SOURCES
    src/main.cpp
    src/App.cpp
    src/AppCapture.cpp
    src/Overlay.cpp
    src/OverlayPaint.cpp
    src/AlphaLayer.cpp
    src/Localization.cpp
    src/Theme.cpp
    src/AboutWindow.cpp
    src/EditorWindow.cpp
    src/EditorInput.cpp
    src/EditorRender.cpp
    src/EditorLayout.cpp
    src/EditorChrome.cpp
    src/EditorActions.cpp
    src/EditorOcr.cpp
    src/HistoryWindow.cpp
    src/HistoryInput.cpp
    src/SettingsWindow.cpp
    src/HotkeyEdit.cpp
    src/SettingsInput.cpp
    src/Toast.cpp
    src/PinWindow.cpp
    src/Ocr.cpp
    src/TrayIcon.cpp
    src/Hotkeys.cpp
)

set(CRISP_APP_HEADERS
    src/App.h
    src/Overlay.h
    src/OverlayPaint.h
    src/AlphaLayer.h
    src/Localization.h
    src/Theme.h
    src/AboutWindow.h
    src/EditorWindow.h
    src/EditorInternal.h
    src/EditorRender.h
    src/HistoryWindow.h
    src/HistoryInternal.h
    src/SettingsWindow.h
    src/HotkeyEdit.h
    src/SettingsInternal.h
    src/Toast.h
    src/PinWindow.h
    src/Ocr.h
    src/TrayIcon.h
    src/Hotkeys.h
    src/Messages.h
    res/resource.h
)

add_executable(crisp WIN32
    ${CRISP_APP_SOURCES}
    ${CRISP_APP_HEADERS}
    res/app.rc
)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES
    ${CRISP_CORE_SOURCES} ${CRISP_CORE_HEADERS}
    ${CRISP_APP_SOURCES}  ${CRISP_APP_HEADERS})

# runtimeobject : WinRT etkinleştirme (Windows.Media.Ocr), ham ABI ile —
#                 C++/WinRT bağımlılığı YOK, yalnızca RoGetActivationFactory.
# comdlg32      : iğne penceresindeki "Farklı kaydet" iletişim kutusu.
target_link_libraries(crisp PRIVATE crisp_core comctl32 runtimeobject comdlg32 msimg32 winmm)

target_link_options(crisp PRIVATE
    # Manifest RC üzerinden gömülür (res/app.rc → RT_MANIFEST); linker'ın kendi
    # manifestini üretmesi engellenir, yoksa çakışır.
    /MANIFEST:NO
    /SUBSYSTEM:WINDOWS
    $<$<CONFIG:Release,MinSizeRel,RelWithDebInfo>:/LTCG>
    $<$<CONFIG:Release,MinSizeRel,RelWithDebInfo>:/OPT:REF>
    $<$<CONFIG:Release,MinSizeRel,RelWithDebInfo>:/OPT:ICF>
    $<$<CONFIG:Release,MinSizeRel>:/INCREMENTAL:NO>
)

set_target_properties(crisp PROPERTIES
    OUTPUT_NAME "Crisp"
    VS_DPI_AWARE "PerMonitor"
)

# app.rc simgeleri gömer ama CMake'in RC bağımlılık taraması onları görmez:
# simge değişince ninja "no work to do" der ve ESKİ simge exe'de kalır. Hata
# sessizdir — derleme başarılı görünür, yalnızca ürün yanlıştır.
set_property(SOURCE res/app.rc APPEND PROPERTY OBJECT_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/res/icons/app.ico"
    "${CMAKE_CURRENT_SOURCE_DIR}/res/icons/tray_dark.ico"
    "${CMAKE_CURRENT_SOURCE_DIR}/res/icons/tray_light.ico"
    "${CMAKE_CURRENT_SOURCE_DIR}/res/strings.rc"
)

endif()  # CRISP_APP

# ---------------------------------------------------------------------------
# crisp_tests.exe — konsol test koşucusu
# ---------------------------------------------------------------------------
if(CRISP_TESTS)
    add_executable(crisp_tests
        tests/TestMain.cpp
        tests/TestFramework.h
        tests/TestGeometry.cpp
        tests/TestCapture.cpp
        tests/TestCodec.cpp
        tests/TestClipboard.cpp
        tests/TestSettings.cpp
        tests/TestWindowPick.cpp
        tests/TestOcrLayout.cpp
        tests/TestAnnotation.cpp
        tests/TestTransform.cpp
        tests/TestHistory.cpp
        tests/TestSound.cpp
    )
    # winmm: TestSound sesi gerçekten çalarak biçimi doğrular.
    target_link_libraries(crisp_tests PRIVATE crisp_core winmm)
    target_include_directories(crisp_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    set_target_properties(crisp_tests PROPERTIES OUTPUT_NAME "crisp_tests")

    enable_testing()
    add_test(NAME crisp_core_tests COMMAND crisp_tests)
endif()

# --- Simge doğrulaması -------------------------------------------------------
# Simgeler ÜRETİLMEZ, doğrulanır. Elle tasarlanmış varlıklardır ve depoda
# tutulurlar; bir üretici burada çalışsaydı her yapılandırmada onların üzerine
# yazardı. Doğrulayıcı eksik kare ya da yanlış boyutu yapılandırma sırasında
# yakalar — alternatifi, bulanık bir tepsi simgesini çalışırken fark etmektir.
if(CRISP_APP)
    execute_process(
        COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass
                -File "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_icons.ps1"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        RESULT_VARIABLE CRISP_ICON_RESULT
        OUTPUT_VARIABLE CRISP_ICON_OUTPUT
        ERROR_VARIABLE CRISP_ICON_OUTPUT
    )
    if(NOT CRISP_ICON_RESULT EQUAL 0)
        message(FATAL_ERROR "Simge doğrulaması başarısız:\n${CRISP_ICON_OUTPUT}")
    endif()
endif()

# --- Kurulum / portable ZIP --------------------------------------------------
if(CRISP_APP)
    install(TARGETS crisp RUNTIME DESTINATION .)
    install(FILES README.md DESTINATION .)
endif()

set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_NAME "Crisp")
set(CPACK_PACKAGE_FILE_NAME "Crisp-${PROJECT_VERSION}-portable-x64")
set(CPACK_PACKAGE_VENDOR "ShadesOfDeath")
include(CPack)

message(STATUS "Crisp ${PROJECT_VERSION}")
message(STATUS "  Toolset : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  CRT     : ${CMAKE_MSVC_RUNTIME_LIBRARY}")
message(STATUS "  Tests   : ${CRISP_TESTS}")
