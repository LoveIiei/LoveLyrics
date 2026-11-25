# SPDX-License-Identifier: AGPL-3.0-or-later

function(lovelyrics_add_test name)
    qt_add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE Qt6::Test ${ARGN})
    target_compile_definitions(${name} PRIVATE TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endfunction()

lovelyrics_add_test(tst_smoke)
