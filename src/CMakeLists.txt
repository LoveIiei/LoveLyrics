# SPDX-License-Identifier: AGPL-3.0-or-later

configure_file(app/AppInfo.h.in ${CMAKE_CURRENT_BINARY_DIR}/generated/AppInfo.h @ONLY)

# Backend: everything except main.cpp, so tests can link it.
add_library(lovelyrics_core STATIC
    app/AppController.cpp app/AppController.h
    app/AutostartController.cpp app/AutostartController.h
    app/SingleInstance.cpp app/SingleInstance.h
    app/WindowManager.cpp app/WindowManager.h
    app/FrameLimiter.cpp app/FrameLimiter.h
    app/Settings.cpp app/Settings.h
    cards/CardColor.cpp cards/CardColor.h
    cards/CardController.cpp cards/CardController.h
    cards/CardRenderer.cpp cards/CardRenderer.h
    cards/TextWrap.cpp cards/TextWrap.h
    lyrics/LrcParser.cpp lyrics/LrcParser.h
    lyrics/LyricsCursor.cpp lyrics/LyricsCursor.h
    lyrics/LyricsDocument.h
    lyrics/LyricsLoader.cpp lyrics/LyricsLoader.h
    lyrics/LyricsModel.cpp lyrics/LyricsModel.h
    lyrics/SongStructure.cpp lyrics/SongStructure.h
    lyrics/TextDecoding.cpp lyrics/TextDecoding.h
    media/CoverLoader.cpp media/CoverLoader.h
    media/PaletteExtractor.cpp media/PaletteExtractor.h
    media/TagReader.cpp media/TagReader.h
    media/TrackResolver.cpp media/TrackResolver.h
    mpris/MprisClient.cpp mpris/MprisClient.h
    playback/PlaybackClock.cpp playback/PlaybackClock.h
)
target_include_directories(lovelyrics_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_link_libraries(lovelyrics_core PUBLIC
    Qt6::Core Qt6::Gui Qt6::Quick Qt6::Qml Qt6::DBus Qt6::Concurrent
    TagLib::tag ICU::uc)
target_compile_options(lovelyrics_core PRIVATE -Wall -Wextra)
