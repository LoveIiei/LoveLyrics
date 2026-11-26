# SPDX-License-Identifier: AGPL-3.0-or-later
cmake_minimum_required(VERSION 3.21)

project(lovelyrics
    VERSION 0.1.0
    DESCRIPTION "Immersive synced lyrics for Strawberry Music Player"
    LANGUAGES CXX)

# One place to rename the app. APP_ID is used for the binary, config dir,
# desktop file and D-Bus name; APP_NAME is what users see.
set(APP_ID "lovelyrics")
set(APP_NAME "Love Lyrics")
set(APP_DBUS_NAME "org.lovelyrics.App")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Quick Qml QuickControls2 DBus Concurrent ShaderTools)
find_package(TagLib 2 REQUIRED CONFIG)
# ICU (already required by qt6-base): dictionary word breaking for Chinese / Japanese.
find_package(ICU REQUIRED COMPONENTS uc)

qt_standard_project_setup(REQUIRES 6.8)
qt_policy(SET QTP0004 NEW)

include(GNUInstallDirs)

add_subdirectory(src)

# The app: main.cpp + the QML module. Defined here so QML resources live at qrc:/qml/...
qt_add_executable(${APP_ID} src/main.cpp)
qt_add_qml_module(${APP_ID}
    URI LoveLyrics
    VERSION 1.0
    NO_RESOURCE_TARGET_PATH
    QML_FILES
        qml/Main.qml
        qml/DebugPage.qml
        qml/ControlsOverlay.qml
        qml/SettingsPage.qml
        qml/CardEditor.qml
        qml/background/Ambient.qml
        qml/components/CrossfadeImage.qml
        qml/components/IconButton.qml
        qml/components/IdleScreen.qml
        qml/components/InstrumentalDots.qml
        qml/components/NoLyricsScreen.qml
        qml/components/RoundedImage.qml
        qml/components/Theme.qml
        qml/components/Toast.qml
        qml/components/TrackBadge.qml
        qml/components/wordlayout.js
        qml/styles/registry.js
        qml/styles/Flow/Style.qml
        qml/styles/Kinetic/Style.qml
        qml/styles/Stage/Style.qml
        qml/styles/Still/Style.qml
        qml/styles/Partita/Style.qml
        qml/styles/Partita/partita.js
    RESOURCES
        qml/icons/play.svg
        qml/icons/pause.svg
        qml/icons/next.svg
        qml/icons/prev.svg
        qml/icons/fullscreen.svg
        qml/icons/fullscreen-exit.svg
        qml/icons/translate.svg
        qml/icons/style.svg
        qml/icons/settings.svg
        qml/icons/card.svg
)
qt_add_shaders(${APP_ID} "lovelyrics_shaders"
    PREFIX "/"
    FILES
        shaders/grain.frag
        shaders/gradient.frag
)
target_link_libraries(${APP_ID} PRIVATE lovelyrics_core Qt6::QuickControls2)
target_compile_options(${APP_ID} PRIVATE -Wall -Wextra)
install(TARGETS ${APP_ID} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()

install(FILES packaging/${APP_ID}.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES packaging/icons/${APP_ID}.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
