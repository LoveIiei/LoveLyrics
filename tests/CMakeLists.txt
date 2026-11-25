# SPDX-License-Identifier: AGPL-3.0-or-later

function(lovelyrics_add_test name)
    qt_add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE Qt6::Test lovelyrics_core)
    target_compile_definitions(${name} PRIVATE
        TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
        FAKE_MPRIS="${PROJECT_SOURCE_DIR}/tools/fake_mpris.py")
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endfunction()

lovelyrics_add_test(tst_lrcparser)
lovelyrics_add_test(tst_textdecoding)
lovelyrics_add_test(tst_clock)
lovelyrics_add_test(tst_cursor)
lovelyrics_add_test(tst_media)

# Integration: fake MPRIS player on a private session bus.
lovelyrics_add_test(tst_mpris)
find_program(DBUS_RUN_SESSION dbus-run-session)
if(DBUS_RUN_SESSION)
    add_test(NAME tst_mpris_isolated COMMAND ${DBUS_RUN_SESSION} -- $<TARGET_FILE:tst_mpris>)
    set_tests_properties(tst_mpris_isolated PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" LABELS integration TIMEOUT 60)
endif()
# The plain variant would run on the user's session bus; only the isolated one is registered.
set_tests_properties(tst_mpris PROPERTIES DISABLED TRUE LABELS integration)
