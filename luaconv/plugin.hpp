#pragma once

#include "libmodmqttconv/converterplugin.hpp"

class LuaConvPlugin : ConverterPlugin
{
    public:
        virtual std::string getName() const { return "lua"; }
        virtual DataConverter* getConverter(const std::string& name);
        virtual ~LuaConvPlugin() {}
};

extern "C" LuaConvPlugin converter_plugin;
LuaConvPlugin converter_plugin;
