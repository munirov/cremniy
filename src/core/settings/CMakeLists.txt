add_library(cremniy_settings_core STATIC
    appsettings.cpp
    appsettings.h
    settingsregistry.cpp
    settingsregistry.h
)

target_include_directories(cremniy_settings_core
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

target_link_libraries(cremniy_settings_core PUBLIC Qt6::Core)
