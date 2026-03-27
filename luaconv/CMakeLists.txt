add_library(luaconv
    MODULE
    plugin.hpp
    plugin.cpp
)

target_link_libraries(luaconv PRIVATE
    sol2::sol2
)

SET_TARGET_PROPERTIES(luaconv PROPERTIES PREFIX "")

