# SPDX-License-Identifier: AGPL-3.0-or-later

configure_file(app/AppInfo.h.in ${CMAKE_CURRENT_BINARY_DIR}/generated/AppInfo.h @ONLY)

qt_add_executable(${APP_ID}
    main.cpp
)

set_target_properties(${APP_ID} PROPERTIES OUTPUT_NAME ${APP_ID})
target_include_directories(${APP_ID} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated ${CMAKE_CURRENT_SOURCE_DIR})

qt_add_qml_module(${APP_ID}
    URI LoveLyrics
    VERSION 1.0
    QML_FILES
        ../qml/Main.qml
    NO_RESOURCE_TARGET_PATH
)

target_link_libraries(${APP_ID} PRIVATE Qt6::Quick Qt6::Qml Qt6::QuickControls2)

install(TARGETS ${APP_ID} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
