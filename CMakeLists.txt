cmake_minimum_required(VERSION 3.21)

project(CopperSteamIdler
    VERSION 0.2.0
    DESCRIPTION "Copper Steam Idler - a Qt6 port of Idle Master Extended"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# qt_standard_project_setup() enables AUTOMOC/AUTOUIC but not AUTORCC, so the
# resource file listed in APP_SOURCES would otherwise be ignored and every
# icon (including the tray icon) would come out blank/null.
set(CMAKE_AUTORCC ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets Network Svg Sql)

qt_standard_project_setup()

set(APP_NAME "CopperSteamIdler")
set(HELPER_NAME "copper-steam-idle")

# ---------------------------------------------------------------------------
# Main GUI application
# ---------------------------------------------------------------------------
set(APP_SOURCES
    src/main.cpp

    src/core/Settings.h
    src/core/Settings.cpp
    src/core/Logger.h
    src/core/Logger.cpp
    src/core/Statistics.h
    src/core/Statistics.cpp
    src/core/SteamProfile.h
    src/core/SteamProfile.cpp
    src/core/CookieJar.h
    src/core/CookieJar.cpp
    src/core/BrowserCookies.h
    src/core/BrowserCookies.cpp
    src/core/HttpClient.h
    src/core/HttpClient.cpp
    src/core/WebSocketClient.h
    src/core/WebSocketClient.cpp
    src/core/SteamHtml.h
    src/core/SteamHtml.cpp
    src/core/ProcessManager.h
    src/core/ProcessManager.cpp
    src/core/PowerManager.h
    src/core/PowerManager.cpp
    src/core/Badge.h
    src/core/Badge.cpp

    src/ui/Theme.h
    src/ui/Theme.cpp
    src/ui/MainWindow.h
    src/ui/MainWindow.cpp
    src/ui/SettingsDialog.h
    src/ui/SettingsDialog.cpp
    src/ui/AdvancedDialog.h
    src/ui/AdvancedDialog.cpp
    src/ui/StatisticsDialog.h
    src/ui/StatisticsDialog.cpp
    src/ui/AboutDialog.h
    src/ui/AboutDialog.cpp
    src/ui/BlacklistDialog.h
    src/ui/BlacklistDialog.cpp
    src/ui/WhitelistDialog.h
    src/ui/WhitelistDialog.cpp
    src/ui/GamesDialog.h
    src/ui/GamesDialog.cpp
    src/ui/CdpLoginDialog.h
    src/ui/CdpLoginDialog.cpp
    src/ui/BrowserLoginDialog.h
    src/ui/BrowserLoginDialog.cpp

    resources/resources.qrc
)

qt_add_executable(${APP_NAME} WIN32 ${APP_SOURCES})

target_link_libraries(${APP_NAME} PRIVATE
    Qt6::Widgets
    Qt6::Network
    Qt6::Svg
    Qt6::Sql
)

if(WIN32)
    # SetSuspendState lives in powrprof.
    target_link_libraries(${APP_NAME} PRIVATE powrprof)
endif()

# ---------------------------------------------------------------------------
# Optional: embedded Steam Quick Login via Qt WebEngine. When the module is
# installed the Sign In action drives an embedded browser automatically;
# otherwise the manual steamLoginSecure dialog is used.
# ---------------------------------------------------------------------------
find_package(Qt6 QUIET COMPONENTS WebEngineWidgets)
if(TARGET Qt6::WebEngineWidgets)
    target_sources(${APP_NAME} PRIVATE
        src/ui/QuickLoginDialog.h
        src/ui/QuickLoginDialog.cpp)
    target_link_libraries(${APP_NAME} PRIVATE Qt6::WebEngineWidgets)
    target_compile_definitions(${APP_NAME} PRIVATE QUICK_LOGIN_ENABLED)
    message(STATUS "Qt WebEngine found - automatic Quick Login enabled")
else()
    message(STATUS "Qt WebEngine not found - Quick Login falls back to manual entry")
endif()

if(WIN32)
    if(MSVC)
        # Let CMake drive rc.exe directly; it quotes paths correctly.
        target_sources(${APP_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.rc")
        target_include_directories(${APP_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/resources")
    else()
        # Compile the Windows resource (icon + version info) manually. CMake's
        # built-in RC rule mishandles the spaces in this project's path, and
        # windres uses different flags than MSVC's rc.exe.
        set(_app_rc_obj "${CMAKE_CURRENT_BINARY_DIR}/app_rc.o")
        add_custom_command(
            OUTPUT "${_app_rc_obj}"
            COMMAND "${CMAKE_COMMAND}" -E chdir "${CMAKE_CURRENT_SOURCE_DIR}/resources"
                    "${CMAKE_RC_COMPILER}" -i app.rc -o "${_app_rc_obj}"
            DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.rc"
                    "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.ico"
            COMMENT "Compiling app.rc"
            VERBATIM)
        target_sources(${APP_NAME} PRIVATE "${_app_rc_obj}")
    endif()
endif()

# ---------------------------------------------------------------------------
# Idle helper (loads steam_api64.dll at runtime, no Qt / no Steamworks SDK)
# ---------------------------------------------------------------------------
add_executable(${HELPER_NAME} WIN32 src/helper/steam_idle_main.cpp)

# ---------------------------------------------------------------------------
# Translations (optional): drop .ts files into i18n/ to build .qm catalogues.
# ---------------------------------------------------------------------------
file(GLOB _ts_files "${CMAKE_CURRENT_SOURCE_DIR}/i18n/*.ts")
if(_ts_files)
    find_package(Qt6 QUIET COMPONENTS LinguistTools)
    if(TARGET Qt6::lrelease)
        qt_add_translations(${APP_NAME} TS_FILES ${_ts_files})
    endif()
endif()

# ---------------------------------------------------------------------------
# Copy the native Steam API next to both executables
# ---------------------------------------------------------------------------
foreach(_tgt ${APP_NAME} ${HELPER_NAME})
    add_custom_command(TARGET ${_tgt} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/libs/steam_api64.dll"
            "$<TARGET_FILE_DIR:${_tgt}>/steam_api64.dll"
        COMMENT "Copying steam_api64.dll next to ${_tgt}")
endforeach()

# ---------------------------------------------------------------------------
# Convenience: deploy Qt runtime into the build output
# ---------------------------------------------------------------------------
if(WIN32)
    get_target_property(_qt_qmake_location Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(_qt_bin_dir "${_qt_qmake_location}" DIRECTORY)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}")
endif()
if(WINDEPLOYQT_EXECUTABLE)
    # Only the GUI links Qt; the helper is a plain Win32 binary.
    add_custom_target(deploy
        COMMAND "${WINDEPLOYQT_EXECUTABLE}" --no-translations "$<TARGET_FILE:${APP_NAME}>"
        DEPENDS ${APP_NAME} ${HELPER_NAME}
        COMMENT "Deploying Qt runtime")
endif()
