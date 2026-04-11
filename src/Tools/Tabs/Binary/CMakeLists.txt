target_sources(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/binarytab.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/binarytab.h
    ${CMAKE_CURRENT_SOURCE_DIR}/formatpage.h
    ${CMAKE_CURRENT_SOURCE_DIR}/formatpage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/formatpagefactory.h
    ${CMAKE_CURRENT_SOURCE_DIR}/formatpagefactory.cpp
)



target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

file(GLOB TOOLTAB_DIRS CONFIGURE_DEPENDS FormatPages/*)
foreach(dir ${TOOLTAB_DIRS})
    if(IS_DIRECTORY ${dir})
        add_subdirectory(${dir})
    endif()
endforeach()