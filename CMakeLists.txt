cmake_minimum_required(VERSION 3.14)
project(libexmdbpp VERSION 1.0.0)

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
target_include_directories(exmdbpp PUBLIC include/exmdbpp)
install(TARGETS exmdbpp LIBRARY)

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
