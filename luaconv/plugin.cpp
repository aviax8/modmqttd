#include "plugin.hpp"
#include "luaconv.hpp"

DataConverter*
LuaConvPlugin::getConverter(const std::string& name) {
    if(name == "evaluate") {
        return new LuaConverter();
    }
    return nullptr;
}
