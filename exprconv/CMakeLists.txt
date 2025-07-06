add_library(exprconv
    MODULE
    plugin.hpp
    plugin.cpp
)

if(USE_STATIC_RUNTIME)
    target_link_options(exprconv PRIVATE
        -static-libstdc++
        -static-libgcc
    )
endif()

SET_TARGET_PROPERTIES(exprconv PROPERTIES PREFIX "")