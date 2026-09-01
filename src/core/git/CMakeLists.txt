add_library(cremniy_git_core STATIC
    blamelineinfo.h
    gitblameservice.cpp
    gitblameservice.h
    gitmanager.cpp
    gitmanager.h
    internal/gitrepository.cpp
    internal/gitrepository.h
    internal/gitblameengine.cpp
    internal/gitblameengine.h
    internal/gitbranchservice.cpp
    internal/gitbranchservice.h
    internal/gitcommitservice.cpp
    internal/gitcommitservice.h
    internal/gitremoteservice.cpp
    internal/gitremoteservice.h
    internal/gitindexservice.cpp
    internal/gitindexservice.h
    internal/gitmergeservice.cpp
    internal/gitmergeservice.h
    internal/gitstashservice.cpp
    internal/gitstashservice.h
)

target_include_directories(cremniy_git_core
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

if(CREMNIY_LIBGIT2_INCLUDE_DIRS)
    target_include_directories(cremniy_git_core PRIVATE ${CREMNIY_LIBGIT2_INCLUDE_DIRS})
endif()

target_link_libraries(cremniy_git_core
    PUBLIC
        Qt6::Core
    PRIVATE
        cremniy_settings_core
        Qt6::Concurrent
        ${CREMNIY_LIBGIT2_TARGET}
)

if(BUILD_TESTING)
    qt_add_executable(gitcoretest
        tests/gitcoretest.cpp
    )
    target_link_libraries(gitcoretest PRIVATE cremniy_git_core Qt6::Test)
    add_test(NAME gitcoretest COMMAND gitcoretest)
endif()
