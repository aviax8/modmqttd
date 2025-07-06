cmake_minimum_required(VERSION 3.13)
project(modmqttd VERSION 1.1.0)

option(USE_STATIC_RUNTIME "Build with static runtime libraries" ON)

option(WITHOUT_TESTS "Do not build unit tests")

set(build_unittests)
if (WITHOUT_TESTS)
    set(build_unittests EXCLUDE_FROM_ALL)
endif()

list(APPEND CMAKE_MODULE_PATH "/opt/boost-armv6")
find_package(Boost 1.88.0 REQUIRED COMPONENTS log_setup log program_options)

set(Boost_USE_STATIC_LIBS    ON)
set(Boost_USE_MULTITHREADED  ON)

if(USE_STATIC_RUNTIME)
    set(Boost_USE_STATIC_RUNTIME  ON)
    set(CMAKE_EXE_LINKER_FLAGS "-static-libstdc++ -static-libgcc")
else()
    set(Boost_USE_STATIC_RUNTIME  OFF)
endif()


if(DEFINED CMAKE_TOOLCHAIN_FILE AND CMAKE_TOOLCHAIN_FILE MATCHES "conan_toolchain.cmake")
    find_package(libmodbus REQUIRED)
    find_package(mosquitto REQUIRED)
else()
    find_package(PkgConfig)
    pkg_check_modules(LIBMODBUS REQUIRED libmodbus)
    pkg_check_modules(MOSQUITTO REQUIRED libmosquitto)
endif()

find_package(Threads)

find_package(yaml-cpp REQUIRED)
find_package(RapidJSON REQUIRED)

find_path(EXPRTK_INCLUDE_DIR "exprtk.hpp")
message(STATUS "EXPRTK_INCLUDE_DIR => ${EXPRTK_INCLUDE_DIR}")

include_directories(${Boost_INCLUDE_DIRS} ${YAML_CPP_INCLUDE_DIR} ${RAPIDJSON_INCLUDE_DIRS} .)
link_directories(${Boost_LIBRARY_DIRS})

add_subdirectory(libmodmqttconv)
add_subdirectory(libmodmqttsrv)
add_subdirectory(modmqttd)
add_subdirectory(unittests ${build_unittests})

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
SET(GCC_COMPILE_FLAGS "-Wformat -Werror=format-security -Wall -Wstrict-aliasing -Wno-psabi")
SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} ${GCC_COMPILE_FLAGS}")

set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib/modmqttd")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

find_package(sol2)
add_subdirectory(luaconv)
add_dependencies(modmqttd luaconv)
if (NOT WITHOUT_TESTS)
    add_dependencies(tests luaconv)
endif()

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

install(TARGETS luaconv DESTINATION lib/modmqttd)
install(TARGETS stdconv DESTINATION lib/modmqttd)

install(FILES modmqttd.service DESTINATION lib/systemd/system)
