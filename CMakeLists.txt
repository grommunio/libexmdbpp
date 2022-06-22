cmake_minimum_required(VERSION 3.14)
project(libexmdbpp VERSION 1.9)

include(GNUInstallDirs)

find_package(Python3 COMPONENTS Interpreter Development)
find_package(pybind11 CONFIG)

set(CMAKE_CXX_STANDARD 17)

add_library(exmdbpp SHARED
            src/ExmdbClient.cpp
            src/queries.cpp
            src/requests.cpp
            src/structures.cpp
            src/util.cpp)
set_target_properties(exmdbpp PROPERTIES SOVERSION 0)
target_include_directories(exmdbpp PUBLIC
                           $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/exmdbpp>
                           $<INSTALL_INTERFACE:include/exmdbpp>)
install(TARGETS exmdbpp
        EXPORT exmdbppTarget
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

if(pybind11_FOUND)
    option(BUILD_PYEXMDB "Build libexmdbpp Python bindings" ON)
    if(BUILD_PYEXMDB)
        pybind11_add_module(pyexmdb bindings/pyexmdb.cpp)
        target_include_directories(pyexmdb PRIVATE include/exmdbpp)
        target_link_libraries(pyexmdb PRIVATE exmdbpp)
        set_target_properties(pyexmdb PROPERTIES INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")
        install(TARGETS pyexmdb LIBRARY DESTINATION ${Python3_SITEARCH})
    endif(BUILD_PYEXMDB)
endif(pybind11_FOUND)

option(INSTALL_DEVFILES "Install libexmdbpp headers and CMake targets" OFF)
if(INSTALL_DEVFILES)
    set(INSTALL_CMAKE_DIR ${CMAKE_INSTALL_DATADIR}/exmdbpp/cmake)
    include(CMakePackageConfigHelpers)
    configure_package_config_file(Config.cmake.in exmdbppConfig.cmake INSTALL_DESTINATION ${INSTALL_CMAKE_DIR})
    write_basic_package_version_file(exmdbppConfigVersion.cmake VERSION "${PROJECT_VERSION}" COMPATIBILITY SameMajorVersion)

    install(DIRECTORY include/exmdbpp TYPE INCLUDE)
    install(EXPORT exmdbppTarget
            FILE exmdbppTargets.cmake
            NAMESPACE exmdbpp::
            DESTINATION ${INSTALL_CMAKE_DIR})
    install(FILES
            ${CMAKE_CURRENT_BINARY_DIR}/exmdbppConfig.cmake
            ${CMAKE_CURRENT_BINARY_DIR}/exmdbppConfigVersion.cmake
            DESTINATION ${INSTALL_CMAKE_DIR})
endif(INSTALL_DEVFILES)
