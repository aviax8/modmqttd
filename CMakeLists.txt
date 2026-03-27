cmake_minimum_required(VERSION 3.13)


list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(CheckSymbolExists)
include(git_version)

project(modmqttd VERSION 0.0.0)
get_version_from_git()

option(USE_STATIC_RUNTIME "Build with static runtime libraries" ON)
if(USE_STATIC_RUNTIME)
    add_link_options(-static-libstdc++ -static-libgcc)
endif()

option(WITHOUT_TESTS "Do not build unit tests")
set(build_unittests)
if (WITHOUT_TESTS)
    set(build_unittests EXCLUDE_FROM_ALL)
endif()

set(CMAKE_VERBOSE_MAKEFILE ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(APPEND CMAKE_REQUIRED_DEFINITIONS -D_GNU_SOURCE)
list(APPEND CMAKE_REQUIRED_LIBRARIES pthread)
CHECK_SYMBOL_EXISTS(pthread_setname_np pthread.h HAVE_PTHREAD_SETNAME_NP)
list(REMOVE_ITEM CMAKE_REQUIRED_DEFINITIONS -D_GNU_SOURCE)

find_package(libmodbus QUIET)

if(NOT libmodbus_FOUND)
    # Fallback to pkg-config
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(PC_LIBMODBUS REQUIRED IMPORTED_TARGET libmodbus)

    add_library(libmodbus::libmodbus INTERFACE IMPORTED)
    target_link_libraries(libmodbus::libmodbus INTERFACE PkgConfig::PC_LIBMODBUS)
endif()

find_package(mosquitto QUIET)

if(NOT mosquitto_FOUND)
    # Fallback to pkg-config
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(PC_MOSQUITTO REQUIRED IMPORTED_TARGET libmosquitto)

    add_library(mosquitto::mosquitto INTERFACE IMPORTED)
    target_link_libraries(mosquitto::mosquitto INTERFACE PkgConfig::PC_MOSQUITTO)
endif()

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

find_package(yaml-cpp REQUIRED)
find_package(RapidJSON REQUIRED)
find_package(spdlog REQUIRED)
find_package(fmt REQUIRED)

add_compile_definitions(SPDLOG_COMPILED_LIB)

find_path(EXPRTK_INCLUDE_DIR "exprtk.hpp")
message(STATUS "EXPRTK_INCLUDE_DIR => ${EXPRTK_INCLUDE_DIR}")

include_directories(${YAML_CPP_INCLUDE_DIR} ${RAPIDJSON_INCLUDE_DIRS} .)

add_subdirectory(libmodmqttconv)
add_subdirectory(libmodmqttsrv)
add_subdirectory(modmqttd)
add_subdirectory(unittests ${build_unittests})

SET(GCC_COMPILE_FLAGS "-Wformat -Werror=format-security -Wall -Wstrict-aliasing -Wno-psabi")
SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} ${GCC_COMPILE_FLAGS}")

set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib/modmqttd")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

add_subdirectory(stdconv)
add_dependencies(modmqttd stdconv)
if (NOT WITHOUT_TESTS)
    add_dependencies(tests stdconv)
endif()

if (EXPRTK_INCLUDE_DIR)
    include_directories(${EXPRTK_INCLUDE_DIR})
    add_subdirectory(exprconv)
    add_dependencies(modmqttd exprconv)

    if (NOT WITHOUT_TESTS)
        target_compile_definitions(tests PRIVATE HAVE_EXPRTK)
        add_dependencies(tests exprconv)
    endif()

    install(TARGETS exprconv DESTINATION lib/modmqttd)
endif()

install (TARGETS modmqttd DESTINATION bin)

install (TARGETS modmqttsrv DESTINATION lib)
install (DIRECTORY "${PROJECT_SOURCE_DIR}/libmodmqttconv" DESTINATION include FILES_MATCHING PATTERN "*.hpp")

install(TARGETS stdconv DESTINATION lib/modmqttd)

install(FILES modmqttd.service DESTINATION lib/systemd/system)