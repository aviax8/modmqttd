set(CMAKE_VERBOSE_MAKEFILE ON)

add_executable(modmqttd
    main.cpp
)

#TODO cmake find for mosquitto libraries
target_link_libraries(modmqttd
    modmqttsrv
    fmt::fmt
    spdlog::spdlog
    yaml-cpp::yaml-cpp
    Threads::Threads
    libmodbus::libmodbus
    mosquitto::mosquitto
)
