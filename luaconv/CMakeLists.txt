add_library(luaconv
    MODULE
    plugin.hpp
    plugin.cpp
)

target_link_libraries(luaconv PRIVATE
    sol2::sol2
)

if(USE_STATIC_RUNTIME)
    target_link_options(luaconv PRIVATE
        -static-libstdc++
        -static-libgcc
    )
endif()

SET_TARGET_PROPERTIES(luaconv PROPERTIES PREFIX "")

