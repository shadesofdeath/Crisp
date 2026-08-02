cmake_minimum_required(VERSION 3.25)

# CMP0091: MSVC runtime library, CMAKE_MSVC_RUNTIME_LIBRARY Ã¼zerinden seÃ§ilir.
cmake_policy(SET CMP0091 NEW)

project(crisp
        # CHANGELOG.md ile aynÄ± numara olmalÄ±. Ä°kisi birbirinden kopmuÅŸtu:
        # deÄŸiÅŸiklik gÃ¼nlÃ¼ÄŸÃ¼ 0.4.0'a gelmiÅŸken burasÄ± hÃ¢lÃ¢ 0.1.0 diyordu, ve
        # sÃ¼rÃ¼m numarasÄ±nÄ± arÅŸivin adÄ±na yazan CPack yanlÄ±ÅŸ adlÄ± bir paket
        # Ã¼retiyordu.
        VERSION 0.7.1
        DESCRIPTION "Screen capture tool for Windows"
        LANGUAGES CXX RC)

if(NOT MSVC)
    message(FATAL_ERROR "Crisp yalnÄ±zca MSVC ile derlenir.")
endif()

# --- Global derleme ayarlarÄ± -------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Statik CRT â€” runtime kurulumu gerektirmeyen tek dosya .exe.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
endif()

option(CRISP_PACKAGED "MSIX paketli derleme" OFF)
option(CRISP_TESTS    "Test koÅŸucusunu derle"  ON)

# Ã‡ekirdeÄŸi ve testleri, arayÃ¼z katmanÄ± olmadan derleyebilmek iÃ§in. GeliÅŸtirme
# sÄ±rasÄ±nda Ã§ekirdeÄŸi tek baÅŸÄ±na yeÅŸile boyamayÄ±, CI'da ise pencere aÃ§mayan bir
# derleme yapmayÄ± mÃ¼mkÃ¼n kÄ±lar.
option(CRISP_APP      "Uygulama hedefini derle" ON)

# ---------------------------------------------------------------------------
# crisp_core â€” kullanÄ±cÄ± arayÃ¼zÃ¼ OLMAYAN her ÅŸey
# ---------------------------------------------------------------------------
# AYRIM NEDENÄ°: bu hedefteki hiÃ§bir fonksiyon pencere oluÅŸturmaz ve kullanÄ±cÄ±
# etkileÅŸimi beklemez, bu yÃ¼zden hepsi bir konsol test koÅŸucusundan doÄŸrudan
# Ã§aÄŸrÄ±labilir. Yakalama, kodlama, pano ve geometri buradadÄ±r; yalnÄ±zca
# etkileÅŸimli katman (kaplama, tepsi, uygulama dÃ¶ngÃ¼sÃ¼) dÄ±ÅŸarÄ±da kalÄ±r.
set(CRISP_CORE_SOURCES
    src/Util.cpp
    src/Geometry.cpp
    src/GeometryGrab.cpp
    src/Capture.cpp
    src/WindowPick.cpp
    src/ImageCodec.cpp
    src/ImageDecode.cpp
    src/ClipboardImage.cpp
    src/Settings.cpp
    src/SettingsStore.cpp
    src/OcrLayout.cpp
    src/ImageEffects.cpp
    src/ImageTransform.cpp
    src/ImageCanvas.cpp
    src/ImageAdjust.cpp
    src/NameFormat.cpp
    src/ShellIntegration.cpp
    src/ColorSpace.cpp
    src/Annotation.cpp
    src/History.cpp
    src/Sound.cpp
    src/Upload.cpp
    src/UploadServices.cpp
    src/UploadHttp.cpp
    src/UploadLog.cpp
)

set(CRISP_CORE_HEADERS
    src/Util.h
    src/Geometry.h
    src/Capture.h
    src/WindowPick.h
    src/ImageCodec.h
    src/ImageCodecInternal.h
    src/ClipboardImage.h
    src/Settings.h
    src/OcrLayout.h
    src/ImageEffects.h
    src/ImageTransform.h
    src/ImageAdjust.h
    src/NameFormat.h
    src/ShellIntegration.h
    src/ColorSpace.h
    src/Annotation.h
    src/History.h
    src/Sound.h
    src/Upload.h
    src/UploadInternal.h
    src/UploadLog.h
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
    /W4             # uyarÄ±sÄ±z derlenmeli
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
    dwmapi          # DwmGetWindowAttribute â€” pencerenin gerÃ§ek Ã§erÃ§eve sÄ±nÄ±rÄ±
    windowscodecs   # WIC â€” PNG kodlama/Ã§Ã¶zme
    ole32           # CoCreateInstance
    shlwapi
    uxtheme         # SetWindowTheme â€” koyu kaydÄ±rma Ã§ubuÄŸu
    winhttp         # gÃ¶rsel yÃ¼kleme; TEK aÄŸ baÄŸÄ±mlÄ±lÄ±ÄŸÄ±
)

# ---------------------------------------------------------------------------
# Crisp.exe
# ---------------------------------------------------------------------------
if(CRISP_APP)

set(CRISP_APP_SOURCES
    src/main.cpp
    src/App.cpp
    src/AppCapture.cpp
    src/AppActions.cpp
    src/AppSettings.cpp
    src/AppText.cpp
    src/Overlay.cpp
    src/OverlayInput.cpp
    src/OverlaySession.cpp
    src/OverlayPaint.cpp
    src/OverlayPanels.cpp
    src/OverlayAdjust.cpp
    src/AlphaLayer.cpp
    src/Localization.cpp
    src/Theme.cpp
    src/AboutWindow.cpp
    src/AboutPaint.cpp
    src/MessageWindow.cpp
    src/MessagePaint.cpp
    src/EditorWindow.cpp
    src/EditorInput.cpp
    src/EditorSession.cpp
    src/EditorRender.cpp
    src/EditorLayout.cpp
    src/EditorChrome.cpp
    src/EditorGlyphs.cpp
    src/EditorStatus.cpp
    src/EditorFile.cpp
    src/EditorUpload.cpp
    src/UploadText.cpp
    src/EditorText.cpp
    src/EditorEffects.cpp
    src/EditorSelect.cpp
    src/EditorActions.cpp
    src/EditorOcr.cpp
    src/EditorTooltip.cpp
    src/ColorPicker.cpp
    src/ColorPickerPaint.cpp
    src/ColorPickerLayout.cpp
    src/ThicknessPicker.cpp
    src/HistoryWindow.cpp
    src/HistoryPaint.cpp
    src/HistoryInput.cpp
    src/HistoryActions.cpp
    src/SettingsWindow.cpp
    src/HotkeyEdit.cpp
    src/SettingsInput.cpp
    src/SettingsValues.cpp
    src/Toast.cpp
    src/PinWindow.cpp
    src/Ocr.cpp
    src/OcrScan.cpp
    src/TrayIcon.cpp
    src/Hotkeys.cpp
)

set(CRISP_APP_HEADERS
    src/App.h
    src/Overlay.h
    src/OverlayInternal.h
    src/OverlayPaint.h
    src/AlphaLayer.h
    src/Localization.h
    src/Theme.h
    src/AboutWindow.h
    src/AboutInternal.h
    src/MessageWindow.h
    src/MessageInternal.h
    src/EditorWindow.h
    src/EditorInternal.h
    src/EditorRender.h
    src/ColorPicker.h
    src/ColorPickerInternal.h
    src/ThicknessPicker.h
    src/HistoryWindow.h
    src/HistoryInternal.h
    src/SettingsWindow.h
    src/HotkeyEdit.h
    src/SettingsInternal.h
    src/Toast.h
    src/PinWindow.h
    src/Ocr.h
    src/OcrInternal.h
    src/TrayIcon.h
    src/Hotkeys.h
    src/Messages.h
    src/UploadText.h
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

# runtimeobject : WinRT etkinleÅŸtirme (Windows.Media.Ocr), ham ABI ile â€”
#                 C++/WinRT baÄŸÄ±mlÄ±lÄ±ÄŸÄ± YOK, yalnÄ±zca RoGetActivationFactory.
# comdlg32      : iÄŸne penceresindeki "FarklÄ± kaydet" iletiÅŸim kutusu.
# imm32        : metin aracinda IME aday penceresinin imlecin yanina tasinmasi.
target_link_libraries(crisp PRIVATE crisp_core comctl32 runtimeobject comdlg32 msimg32 winmm imm32)

target_link_options(crisp PRIVATE
    # Manifest RC Ã¼zerinden gÃ¶mÃ¼lÃ¼r (res/app.rc â†’ RT_MANIFEST); linker'Ä±n kendi
    # manifestini Ã¼retmesi engellenir, yoksa Ã§akÄ±ÅŸÄ±r.
    /MANIFEST:NO
    /SUBSYSTEM:WINDOWS
    $<$<CONFIG:Release,MinSizeRel,RelWithDebInfo>:/LTCG>
    $<$<CONFIG:Release,MinSizeRel,RelWithDebInfo>:/OPT:REF>
    $<$<CONFIG:Release,MinSizeRel,RelWithDebInfo>:/OPT:ICF>
    $<$<CONFIG:Release,MinSizeRel>:/INCREMENTAL:NO>
)

# SÃ¼rÃ¼m numarasÄ± kaynak dosyaya elle yazÄ±lmaz; RC derleyicisine tanÄ±m olarak
# geÃ§er. Bkz. res/app.rc.
target_compile_definitions(crisp PRIVATE
    CRISP_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    CRISP_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    CRISP_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

set_target_properties(crisp PROPERTIES
    OUTPUT_NAME "Crisp"
    VS_DPI_AWARE "PerMonitor"
)

# app.rc simgeleri gÃ¶mer ama CMake'in RC baÄŸÄ±mlÄ±lÄ±k taramasÄ± onlarÄ± gÃ¶rmez:
# simge deÄŸiÅŸince ninja "no work to do" der ve ESKÄ° simge exe'de kalÄ±r. Hata
# sessizdir â€” derleme baÅŸarÄ±lÄ± gÃ¶rÃ¼nÃ¼r, yalnÄ±zca Ã¼rÃ¼n yanlÄ±ÅŸtÄ±r.
set_property(SOURCE res/app.rc APPEND PROPERTY OBJECT_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/res/icons/app.ico"
    "${CMAKE_CURRENT_SOURCE_DIR}/res/icons/tray_dark.ico"
    "${CMAKE_CURRENT_SOURCE_DIR}/res/icons/tray_light.ico"
    "${CMAKE_CURRENT_SOURCE_DIR}/res/strings.rc"
)

endif()  # CRISP_APP

# ---------------------------------------------------------------------------
# crisp_tests.exe â€” konsol test koÅŸucusu
# ---------------------------------------------------------------------------
if(CRISP_TESTS)
    add_executable(crisp_tests
        tests/TestMain.cpp
        tests/TestFramework.h
        tests/TestGeometry.cpp
        tests/TestGeometryGrab.cpp
        tests/TestCapture.cpp
        tests/TestCodec.cpp
        tests/TestClipboard.cpp
        tests/TestSettings.cpp
        tests/TestUpload.cpp
        tests/TestWindowPick.cpp
        tests/TestOcrLayout.cpp
        tests/TestAnnotation.cpp
        tests/TestTransform.cpp
        tests/TestCanvas.cpp
        tests/TestAdjust.cpp
        tests/TestNameFormat.cpp
        tests/TestShellIntegration.cpp
        tests/TestColorSpace.cpp
        tests/TestHistory.cpp
        tests/TestSound.cpp
    )
    # winmm: TestSound sesi gerÃ§ekten Ã§alarak biÃ§imi doÄŸrular.
    target_link_libraries(crisp_tests PRIVATE crisp_core winmm)
    target_include_directories(crisp_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    set_target_properties(crisp_tests PROPERTIES OUTPUT_NAME "crisp_tests")

    enable_testing()
    add_test(NAME crisp_core_tests COMMAND crisp_tests)
endif()

# --- Simge doÄŸrulamasÄ± -------------------------------------------------------
# Simgeler ÃœRETÄ°LMEZ, doÄŸrulanÄ±r. Elle tasarlanmÄ±ÅŸ varlÄ±klardÄ±r ve depoda
# tutulurlar; bir Ã¼retici burada Ã§alÄ±ÅŸsaydÄ± her yapÄ±landÄ±rmada onlarÄ±n Ã¼zerine
# yazardÄ±. DoÄŸrulayÄ±cÄ± eksik kare ya da yanlÄ±ÅŸ boyutu yapÄ±landÄ±rma sÄ±rasÄ±nda
# yakalar â€” alternatifi, bulanÄ±k bir tepsi simgesini Ã§alÄ±ÅŸÄ±rken fark etmektir.
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
        message(FATAL_ERROR "Simge doÄŸrulamasÄ± baÅŸarÄ±sÄ±z:\n${CRISP_ICON_OUTPUT}")
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
