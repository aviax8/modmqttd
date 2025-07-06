#pragma once

#include <bit>

#include <sol/sol.hpp>
#include "libmodmqttconv/convexception.hpp"
#include "libmodmqttconv/converter.hpp"

//#define LUACONV_DEBUG

class LuaConverter : public DataConverter {
public:
    static const int MAX_REGISTERS = 10;

    LuaConverter() : mValues(MAX_REGISTERS, 0), mPrecision(-1) {}

    virtual MqttValue toMqtt(const ModbusRegisters& data) const override {
        #ifdef LUACONV_DEBUG
        std::ostringstream ss;
        ss << "LuaConverter: toMqtt(" << dbgLogContext << "): [";
        for (int i = 0; i < data.getCount(); ++i) {
            ss << data.getValue(i);
            if (i < data.getCount() - 1)
                ss << ", ";
        }
        ss << "]";
        std::cout << ss.str() << std::endl;
        #endif

        if (data.getCount() > MAX_REGISTERS)
            throw ConvException("Maximum " + std::to_string(MAX_REGISTERS) + " registers allowed");

        // update register values in Lua env
        for (int i = 0; i < data.getCount(); i++) {
            mValues[i] = data.getValue(i);
            mLua["R" + std::to_string(i)] = mValues[i];
        }

        sol::protected_function_result result = mFunction();
        if (!result.valid()) {
            sol::error err = result;
            throw ConvException(std::string("Lua runtime error: ") + err.what());
        }

        switch (result.get_type()) {
            case sol::type::string: {
                std::string ret = result;
                return MqttValue::fromString(ret);
            }
            case sol::type::boolean: {
                int32_t ret = result;
                return MqttValue::fromInt(ret);
            }
            case sol::type::number: {
                if (mPrecision == 0) {
                    int64_t ret = result;
                    return MqttValue::fromInt(ret);
                } else {
                    double ret = result;
                    return MqttValue::fromDouble(ret, mPrecision);
                }
            }
            default:
                throw ConvException("Unexpected Lua expression return type (valid is string, number, boolean");
        }
    }

    virtual void setArgs(const std::vector<std::string>& args) override {
        #ifdef LUACONV_DEBUG
        std::cout << "LuaConverter: setArgs(" << dbgLogContext << ")" << std::endl;
        #endif

        if (args.empty())
            throw ConvException("Lua expression required");

        // open common libraries
        mLua.open_libraries(
            sol::lib::base,
            sol::lib::math,
            sol::lib::string,
            sol::lib::table,
            sol::lib::utf8
        );

        // add user-defined functions
        mLua.set_function("int32", int32);
        mLua.set_function("uint32", uint32);
        mLua.set_function("flt32", flt32);
        mLua.set_function("flt32be", flt32be);
        mLua.set_function("int16", int16);

        // pre-register variables R0 to R9 with zero value
        for (int i = 0; i < MAX_REGISTERS; i++) {
            mLua["R" + std::to_string(i)] = 0.0;
        }

        // comile Lua expression as a function
        sol::load_result chunk = mLua.load(ConverterTools::getArg(0, args));
        if (!chunk.valid()) {
            sol::error err = chunk;
            throw ConvException(std::string("Lua compile error: ") + err.what());
        }

        mFunction = chunk;

        if (args.size() >= 2)
            mPrecision = ConverterTools::getIntArg(1, args);

        #ifdef LUACONV_DEBUG
        dbgLogContext = ConverterTools::getArg(0, args);
        #endif
    }

    virtual ~LuaConverter() {}

private:
    mutable std::vector<double> mValues;
    mutable sol::state mLua;
    sol::function mFunction;
    int mPrecision;

    #ifdef LUACONV_DEBUG
    std::string dbgLogContext;
    #endif

    static double int32(double high, double low) {
        return ConverterTools::toNumber<int32_t>(high, low, true);
    }

    static double uint32(double high, double low) {
        return ConverterTools::toNumber<uint32_t>(high, low, true);
    }

    static double flt32(double high, double low) {
        return ConverterTools::toNumber<float>(high, low, true);
    }

    static double flt32be(double high, double low) {
        return ConverterTools::toNumber<float>(high, low);
    }

    static double int16(double val) {
        uint16_t tmp = uint16_t(val);
        return static_cast<int16_t>(tmp);
    }

};
