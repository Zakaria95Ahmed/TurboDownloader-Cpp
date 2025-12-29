cmake_minimum_required(VERSION 3.21)

# ═══════════════════════════════════════════════════════════════════════════════
# OpenIDM - High-Performance Cross-Platform Download Manager
# ═══════════════════════════════════════════════════════════════════════════════

project(OpenIDM
    VERSION 1.0.0
    DESCRIPTION "High-performance, cross-platform download manager"
    HOMEPAGE_URL "https://github.com/openidm/openidm"
    LANGUAGES CXX
)

# ───────────────────────────────────────────────────────────────────────────────
# C++ Standard Requirements
# ───────────────────────────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ───────────────────────────────────────────────────────────────────────────────
# Build Configuration
# ───────────────────────────────────────────────────────────────────────────────
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Include custom CMake modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")

# ───────────────────────────────────────────────────────────────────────────────
# Platform Detection
# ───────────────────────────────────────────────────────────────────────────────
if(WIN32)
    set(OPENIDM_PLATFORM "Windows")
    add_compile_definitions(OPENIDM_PLATFORM_WINDOWS)
elseif(APPLE)
    set(OPENIDM_PLATFORM "macOS")
    add_compile_definitions(OPENIDM_PLATFORM_MACOS)
elseif(ANDROID)
    set(OPENIDM_PLATFORM "Android")
    add_compile_definitions(OPENIDM_PLATFORM_ANDROID)
elseif(UNIX)
    set(OPENIDM_PLATFORM "Linux")
    add_compile_definitions(OPENIDM_PLATFORM_LINUX)
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

message(STATUS "Building OpenIDM for: ${OPENIDM_PLATFORM}")

# ───────────────────────────────────────────────────────────────────────────────
# Build Options
# ───────────────────────────────────────────────────────────────────────────────
option(OPENIDM_BUILD_TESTS "Build unit tests" ON)
option(OPENIDM_BUILD_DOCS "Build documentation" OFF)
option(OPENIDM_USE_SYSTEM_CURL "Use system libcurl instead of bundled" ON)
option(OPENIDM_ENABLE_SANITIZERS "Enable address/undefined sanitizers" OFF)
option(OPENIDM_ENABLE_LTO "Enable Link Time Optimization" OFF)

# ───────────────────────────────────────────────────────────────────────────────
# Compiler Configuration
# ───────────────────────────────────────────────────────────────────────────────
if(MSVC)
    # MSVC-specific flags
    add_compile_options(
        /W4                 # High warning level
        /WX-                # Don't treat warnings as errors (for now)
        /permissive-        # Standards conformance
        /Zc:__cplusplus     # Correct __cplusplus macro
        /MP                 # Multi-processor compilation
        /utf-8              # UTF-8 source and execution charset
    )
    
    # Debug-specific
    add_compile_options($<$<CONFIG:Debug>:/Zi /Od /RTC1>)
    
    # Release-specific
    add_compile_options($<$<CONFIG:Release>:/O2 /GL /DNDEBUG>)
    add_link_options($<$<CONFIG:Release>:/LTCG>)
    
else()
    # GCC/Clang flags
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Wno-unused-parameter
        -fPIC
    )
    
    # Debug-specific
    add_compile_options($<$<CONFIG:Debug>:-g -O0>)
    
    # Release-specific
    add_compile_options($<$<CONFIG:Release>:-O3 -DNDEBUG>)
    
    # Sanitizers (Debug only)
    if(OPENIDM_ENABLE_SANITIZERS AND CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address,undefined)
    endif()
    
    # LTO
    if(OPENIDM_ENABLE_LTO)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
    endif()
endif()

# ───────────────────────────────────────────────────────────────────────────────
# Find Dependencies
# ───────────────────────────────────────────────────────────────────────────────

# Qt 6
find_package(Qt6 6.5 REQUIRED COMPONENTS
    Core
    Gui
    Quick
    QuickControls2
    Sql
    Network
    Concurrent
)

if(NOT ANDROID)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets)
endif()

message(STATUS "Found Qt ${Qt6_VERSION}")

# libcurl
if(OPENIDM_USE_SYSTEM_CURL)
    find_package(CURL REQUIRED)
    message(STATUS "Found system libcurl ${CURL_VERSION_STRING}")
else()
    # Fetch and build libcurl
    include(FetchContent)
    FetchContent_Declare(
        curl
        GIT_REPOSITORY https://github.com/curl/curl.git
        GIT_TAG curl-8_5_0
    )
    set(BUILD_CURL_EXE OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(CURL_USE_OPENSSL ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(curl)
endif()

# SQLite (bundled for consistency)
# We'll use Qt's SQL driver but ensure SQLite is available

# ───────────────────────────────────────────────────────────────────────────────
# Core Library (Engine)
# ───────────────────────────────────────────────────────────────────────────────
add_library(openidm_engine STATIC
    # Engine core
    src/engine/DownloadManager.cpp
    src/engine/DownloadTask.cpp
    src/engine/Segment.cpp
    src/engine/SegmentWorker.cpp
    src/engine/SegmentScheduler.cpp
    src/engine/NetworkProbe.cpp
    src/engine/SpeedCalculator.cpp
    
    # Persistence
    src/persistence/PersistenceManager.cpp
    src/persistence/DatabaseSchema.cpp
    
    # Streaming integration
    src/streaming/YtDlpIntegration.cpp
    src/streaming/StreamParser.cpp
)

target_include_directories(openidm_engine
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(openidm_engine
    PUBLIC
        Qt6::Core
        Qt6::Sql
        Qt6::Network
        Qt6::Concurrent
    PRIVATE
        CURL::libcurl
)

# Platform-specific sources
if(WIN32)
    target_sources(openidm_engine PRIVATE
        src/platform/windows/WindowsPlatform.cpp
    )
    target_link_libraries(openidm_engine PRIVATE
        shell32 ole32 uuid
    )
elseif(APPLE)
    target_sources(openidm_engine PRIVATE
        src/platform/macos/MacOSPlatform.mm
    )
    target_link_libraries(openidm_engine PRIVATE
        "-framework Cocoa"
        "-framework UserNotifications"
    )
elseif(ANDROID)
    target_sources(openidm_engine PRIVATE
        src/platform/android/AndroidPlatform.cpp
    )
elseif(UNIX)
    target_sources(openidm_engine PRIVATE
        src/platform/linux/LinuxPlatform.cpp
    )
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(DBUS IMPORTED_TARGET dbus-1)
    if(DBUS_FOUND)
        target_link_libraries(openidm_engine PRIVATE PkgConfig::DBUS)
    endif()
endif()

# ───────────────────────────────────────────────────────────────────────────────
# ViewModel Library
# ───────────────────────────────────────────────────────────────────────────────
add_library(openidm_viewmodel STATIC
    src/viewmodel/DownloadListModel.cpp
    src/viewmodel/DownloadViewModel.cpp
    src/viewmodel/SettingsViewModel.cpp
)

target_include_directories(openidm_viewmodel
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(openidm_viewmodel
    PUBLIC
        Qt6::Core
        Qt6::Quick
        openidm_engine
)

# ───────────────────────────────────────────────────────────────────────────────
# QML Module
# ───────────────────────────────────────────────────────────────────────────────
qt_add_qml_module(openidm_qml
    URI OpenIDM
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/views/DownloadListView.qml
        qml/views/AddDownloadDialog.qml
        qml/views/SettingsPage.qml
        qml/components/DownloadItemDelegate.qml
        qml/components/ProgressBar.qml
        qml/components/SpeedIndicator.qml
        qml/components/ActionButton.qml
        qml/components/GlassPanel.qml
        qml/theme/Theme.qml
    RESOURCES
        resources/icons/app_icon.svg
        resources/icons/download.svg
        resources/icons/pause.svg
        resources/icons/resume.svg
        resources/icons/cancel.svg
        resources/icons/settings.svg
        resources/icons/add.svg
        resources/fonts/Inter-Regular.ttf
        resources/fonts/Inter-Bold.ttf
    SOURCES
        src/ui/QmlRegistration.cpp
)

target_link_libraries(openidm_qml
    PRIVATE
        Qt6::Quick
        Qt6::QuickControls2
        openidm_viewmodel
)

# ───────────────────────────────────────────────────────────────────────────────
# Main Executable
# ───────────────────────────────────────────────────────────────────────────────
if(ANDROID)
    qt_add_executable(OpenIDM
        src/main.cpp
    )
    
    set_target_properties(OpenIDM PROPERTIES
        QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_SOURCE_DIR}/android"
        QT_ANDROID_TARGET_SDK_VERSION 34
        QT_ANDROID_MIN_SDK_VERSION 24
    )
else()
    qt_add_executable(OpenIDM
        src/main.cpp
    )
    
    if(WIN32)
        # Windows application properties
        set_target_properties(OpenIDM PROPERTIES
            WIN32_EXECUTABLE TRUE
        )
        
        # Add application icon (Windows)
        target_sources(OpenIDM PRIVATE
            resources/windows/app.rc
        )
    elseif(APPLE)
        set_target_properties(OpenIDM PROPERTIES
            MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_BUNDLE_NAME "OpenIDM"
            MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
            MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
            MACOSX_BUNDLE_GUI_IDENTIFIER "com.openidm.app"
        )
    endif()
endif()

target_link_libraries(OpenIDM
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Quick
        Qt6::QuickControls2
        openidm_engine
        openidm_viewmodel
        openidm_qml
)

if(NOT ANDROID)
    target_link_libraries(OpenIDM PRIVATE Qt6::Widgets)
endif()

# ───────────────────────────────────────────────────────────────────────────────
# Tests
# ───────────────────────────────────────────────────────────────────────────────
if(OPENIDM_BUILD_TESTS AND NOT ANDROID)
    enable_testing()
    find_package(Qt6 REQUIRED COMPONENTS Test)
    
    add_executable(openidm_tests
        tests/unit/test_main.cpp
        tests/unit/test_segment.cpp
        tests/unit/test_scheduler.cpp
        tests/unit/test_persistence.cpp
        tests/unit/test_download_task.cpp
    )
    
    target_link_libraries(openidm_tests
        PRIVATE
            Qt6::Test
            openidm_engine
    )
    
    target_include_directories(openidm_tests
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
    )
    
    add_test(NAME OpenIDMTests COMMAND openidm_tests)
endif()

# ───────────────────────────────────────────────────────────────────────────────
# Installation
# ───────────────────────────────────────────────────────────────────────────────
include(GNUInstallDirs)

install(TARGETS OpenIDM
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install desktop file (Linux)
if(UNIX AND NOT APPLE AND NOT ANDROID)
    install(FILES resources/linux/openidm.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
    )
    install(FILES resources/icons/app_icon.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
        RENAME openidm.svg
    )
endif()

# Qt deployment (Windows/macOS)
if(WIN32 OR APPLE)
    qt_generate_deploy_qml_app_script(
        TARGET OpenIDM
        OUTPUT_SCRIPT deploy_script
        NO_UNSUPPORTED_PLATFORM_ERROR
    )
    install(SCRIPT ${deploy_script})
endif()

# ───────────────────────────────────────────────────────────────────────────────
# CPack Configuration
# ───────────────────────────────────────────────────────────────────────────────
set(CPACK_PACKAGE_NAME "OpenIDM")
set(CPACK_PACKAGE_VENDOR "OpenIDM Project")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "High-performance download manager")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_INSTALL_DIRECTORY "OpenIDM")

if(WIN32)
    set(CPACK_GENERATOR "NSIS;ZIP")
    set(CPACK_NSIS_DISPLAY_NAME "OpenIDM")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_DMG_VOLUME_NAME "OpenIDM")
elseif(UNIX)
    set(CPACK_GENERATOR "DEB;RPM;TGZ")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "OpenIDM Project")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libqt6-dev, libcurl4")
endif()

include(CPack)

# ───────────────────────────────────────────────────────────────────────────────
# Summary
# ───────────────────────────────────────────────────────────────────────────────
message(STATUS "")
message(STATUS "═══════════════════════════════════════════════════════════")
message(STATUS " OpenIDM Configuration Summary")
message(STATUS "═══════════════════════════════════════════════════════════")
message(STATUS " Version:           ${PROJECT_VERSION}")
message(STATUS " Platform:          ${OPENIDM_PLATFORM}")
message(STATUS " Build Type:        ${CMAKE_BUILD_TYPE}")
message(STATUS " C++ Compiler:      ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS " Qt Version:        ${Qt6_VERSION}")
message(STATUS " libcurl:           ${CURL_VERSION_STRING}")
message(STATUS " Build Tests:       ${OPENIDM_BUILD_TESTS}")
message(STATUS " Sanitizers:        ${OPENIDM_ENABLE_SANITIZERS}")
message(STATUS " LTO:               ${OPENIDM_ENABLE_LTO}")
message(STATUS "═══════════════════════════════════════════════════════════")
message(STATUS "")
