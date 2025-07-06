add_library(stdconv
    MODULE
    bits.hpp
    float32.hpp
    int16.hpp
    int32.hpp
    map.cpp
    map.hpp
    plugin.hpp
    plugin.cpp
    scale.hpp
    single_arg_ops.hpp
    uint32.hpp
)

if(USE_STATIC_RUNTIME)
    target_link_options(stdconv PRIVATE
        -static-libstdc++
        -static-libgcc
    )
endif()

SET_TARGET_PROPERTIES(stdconv PROPERTIES PREFIX "")
