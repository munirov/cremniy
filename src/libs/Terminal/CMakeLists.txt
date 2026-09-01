add_library(cremniy_libvterm STATIC
    third_party/libvterm/src/encoding.c
    third_party/libvterm/src/keyboard.c
    third_party/libvterm/src/mouse.c
    third_party/libvterm/src/parser.c
    third_party/libvterm/src/pen.c
    third_party/libvterm/src/screen.c
    third_party/libvterm/src/state.c
    third_party/libvterm/src/unicode.c
    third_party/libvterm/src/vterm.c
)

target_include_directories(cremniy_libvterm
    PUBLIC third_party/libvterm/include
    PRIVATE third_party/libvterm/src
)

set_target_properties(cremniy_libvterm PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)

if(MSVC)
    target_compile_options(cremniy_libvterm PRIVATE /W0)
else()
    target_compile_options(cremniy_libvterm PRIVATE -w)
endif()

add_library(cremniy_terminal_core STATIC
    celliterator.cpp
    celliterator.h
    keys.cpp
    keys.h
    ptyprocess.cpp
    ptyprocess.h
    scrollback.cpp
    scrollback.h
    surfaceintegration.h
    terminal_global.h
    terminalsurface.cpp
    terminalsurface.h
    terminalview.cpp
    terminalview.h
)

if(WIN32)
    target_sources(cremniy_terminal_core PRIVATE
        ptyprocess_windows.cpp
        ptyprocess_windows.h
    )
else()
    target_sources(cremniy_terminal_core PRIVATE
        ptyprocess_posix.cpp
        ptyprocess_posix.h
    )
    find_library(CREMNIY_UTIL_LIBRARY util)
    if(CREMNIY_UTIL_LIBRARY)
        target_link_libraries(cremniy_terminal_core PRIVATE ${CREMNIY_UTIL_LIBRARY})
    endif()
endif()

target_include_directories(cremniy_terminal_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(cremniy_terminal_core PUBLIC TERMINALLIB_STATIC_LIBRARY)
target_link_libraries(cremniy_terminal_core
    PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        cremniy_libvterm
)

set_target_properties(cremniy_terminal_core PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)

if(BUILD_TESTING)
    qt_add_executable(cremniy_terminal_pty_test
        tests/ptyprocesstest.cpp
    )
    target_link_libraries(cremniy_terminal_pty_test
        PRIVATE
            cremniy_terminal_core
            Qt6::Test
    )
    add_test(NAME terminal_pty COMMAND cremniy_terminal_pty_test)
endif()
