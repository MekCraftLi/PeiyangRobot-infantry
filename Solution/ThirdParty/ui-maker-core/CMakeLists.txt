cmake_minimum_required(VERSION 3.20)

project(ui-maker-core VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(UIMAKER_BUILD_EXAMPLES "Build ui-maker-core example executable" OFF)

add_library(ui-maker-core STATIC)
add_library(uimaker::core ALIAS ui-maker-core)

target_sources(ui-maker-core
    PRIVATE
        src/ui_maker_core.cpp
        src/ui_render.cpp
)

target_include_directories(ui-maker-core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_compile_features(ui-maker-core PUBLIC cxx_std_20)

if(UIMAKER_BUILD_EXAMPLES)
    add_executable(ui-maker-core-example
        examples/basic_example.cpp
    )
    target_link_libraries(ui-maker-core-example PRIVATE ui-maker-core)
endif()

install(TARGETS ui-maker-core
    EXPORT ui-maker-coreTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT ui-maker-coreTargets
    FILE ui-maker-coreTargets.cmake
    NAMESPACE uimaker::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ui-maker-core
)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ui-maker-coreConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/ui-maker-coreConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ui-maker-core
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/ui-maker-coreConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/ui-maker-coreConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/ui-maker-coreConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ui-maker-core
)
