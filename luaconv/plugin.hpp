#pragma once

#include <boost/config.hpp> // for BOOST_SYMBOL_EXPORT

#include "libmodmqttconv/converterplugin.hpp"

class LuaConvPlugin : ConverterPlugin {
    public:
        virtual std::string getName() const { return "lua"; }
        virtual DataConverter* getConverter(const std::string& name);
        virtual ~LuaConvPlugin() {}
};

extern "C" BOOST_SYMBOL_EXPORT LuaConvPlugin converter_plugin;
LuaConvPlugin converter_plugin;
