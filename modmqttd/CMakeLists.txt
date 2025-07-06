set(CMAKE_VERBOSE_MAKEFILE ON)

add_executable(modmqttd
    main.cpp
)

if(DEFINED CMAKE_TOOLCHAIN_FILE AND CMAKE_TOOLCHAIN_FILE MATCHES "conan_toolchain.cmake")
    target_link_libraries(modmqttd
        modmqttsrv
        mosquitto::mosquitto
        yaml-cpp::yaml-cpp
        Boost::program_options
        Boost::log
        libmodbus::libmodbus
        ${CMAKE_THREAD_LIBS_INIT}
        atomic
    )
else()
    target_link_libraries(modmqttd
        modmqttsrv
        ${MOSQUITTO_LIBRARIES}
        yaml-cpp
        ${Boost_LIBRARIES}
        ${LIBMODBUS_LIBRARIES}
        ${CMAKE_THREAD_LIBS_INIT}
    )
endif()
