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

qt_standard_project_setup(REQUIRES 6.8)
qt_policy(SET QTP0004 NEW)

include(GNUInstallDirs)

add_subdirectory(src)

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()

install(FILES packaging/${APP_ID}.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES packaging/icons/${APP_ID}.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
