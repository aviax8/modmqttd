#pragma once

#include <bit>
#include <chrono>
#include <ctime>
#include <format>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "libmodmqttconv/converter.hpp"
#include "libmodmqttconv/convexception.hpp"
#include <sol/sol.hpp>

// #define LUACONV_DEBUG

class LuaConverter : public DataConverter
{
    public:
        static const int MAX_REGISTERS = 20;

        LuaConverter()
            : mValues(MAX_REGISTERS, 0)
            , mPrecision(-1)
        {
        }

        virtual MqttValue toMqtt(const ModbusRegisters& data) const override
        {
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

            if (!mFunction.valid()) {
                throw ConvException("Lua toMqtt runtime error: No expression was set");
            }

            if (data.getCount() > MAX_REGISTERS)
                throw ConvException("Lua toMqtt runtime error: Maximum " + std::to_string(MAX_REGISTERS)
                                    + " registers allowed");

            // update register values in Lua env
            for (int i = 0; i < data.getCount(); i++) {
                mValues[i] = data.getValue(i);
                mLua["R" + std::to_string(i)] = mValues[i];
            }

            sol::protected_function_result result = mFunction();
            if (!result.valid()) {
                sol::error err = result;
                throw ConvException(std::string("Lua toMqtt runtime error: ") + err.what());
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
                        return MqttValue::fromInt64(ret);
                    }
                    else {
                        double ret = result;
                        return MqttValue::fromDouble(ret, mPrecision);
                    }
                }
                default:
                    throw ConvException("Lua toMqtt runtime error: Unexpected Lua expression return type (valid is "
                                        "string, number, boolean)");
            }
        }

        virtual ModbusRegisters toModbus(const MqttValue& value, int registerCount) const override
        {
            #ifdef LUACONV_DEBUG
            std::cout << "LuaConverter: toModbus(" << dbgLogContext << ")" << std::endl;
            #endif

            // Set Lua variable to MQTT payload
            switch (value.getSourceType()) {
                case MqttValue::SourceType::INT:
                case MqttValue::SourceType::INT64:
                case MqttValue::SourceType::DOUBLE:
                    mLua["V"] = value.getDouble();
                    break;
                case MqttValue::SourceType::BINARY:
                    mLua["V"] = value.getString();
                    break;
                default:
                    throw ConvException("Lua toModbus runtime error: Unsupported MQTT value type for Lua conversion");
            }

            // Calling Lua function
            sol::protected_function_result result = mFunction();
            if (!result.valid()) {
                sol::error err = result;
                throw ConvException(std::string("Lua toModbus runtime error: ") + err.what());
            }

            // Convert z Lua result to ModbusRegisters
            ModbusRegisters regs;

            switch (result.get_type()) {
                case sol::type::number: {
                    // A number - 1 register
                    uint16_t regVal = static_cast<uint16_t>(result.get<double>());
                    regs.appendValue(regVal);
                    break;
                }

                case sol::type::table: {
                    // Lua table - multiple registers
                    sol::table tbl = result;
                    for (auto& kv : tbl) {
                        sol::object val = kv.second;
                        if (val.get_type() == sol::type::number) {
                            regs.appendValue(static_cast<uint16_t>(val.as<double>()));
                        }
                        else {
                            throw ConvException(
                                "Lua toModbus runtime error: Invalid element type in Lua table (expected number)");
                        }
                    }
                    break;
                }

                default:
                    throw ConvException(
                        "Lua toModbus runtime error: Unexpected Lua return type (expected number or table)");
            }

            // Check number of returned registers
            if (registerCount > 0 && regs.getCount() != registerCount) {
                std::ostringstream ss;
                ss << "Lua toModbus runtime error: Expected " << registerCount << " registers, got " << regs.getCount()
                   << " from Lua";
                throw ConvException(ss.str());
            }

            return regs;
        }

        virtual ConverterArgs getArgs() const override
        {
            ConverterArgs ret;
            ret.add("expression", ConverterArgType::STRING, "");
            ret.add(ConverterArg::sPrecisionArgName, ConverterArgType::INT, ConverterArgValue::NO_PRECISION);
            return ret;
        }

        virtual void setArgValues(const ConverterArgValues& values) override
        {
            #ifdef LUACONV_DEBUG
            std::cout << "LuaConverter: setArgValues(" << dbgLogContext << ")" << std::endl;
            #endif

            const std::string expression = values["expression"].as_str();
            if (expression.empty())
                throw ConvException("Lua expression required");

            // Reset Lua state before compiling a new expression.
            mLua = sol::state();
            mFunction = sol::function();

            // open common libraries
            mLua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::table, sol::lib::utf8);

            // add user-defined functions
            mLua.set_function("int32", int32);
            mLua.set_function("int32bs", int32bs);
            mLua.set_function("uint32", uint32);
            mLua.set_function("uint32bs", uint32bs);
            mLua.set_function("flt32", flt32);
            mLua.set_function("flt32bs", flt32bs);
            mLua.set_function("int16", int16);
            mLua.set_function("int16bs", int16bs);
            mLua.set_function("uint16", uint16);
            mLua.set_function("uint16bs", uint16bs);
            mLua.set_function("bit_positions", bit_positions);

            mLua.set_function("clock_usec", clock_usec);

            // wrapper due to optional parameters
            mLua.set_function("format_clock", [](double usec_since_epoch, sol::optional<std::string> format,
                                                 sol::optional<bool> local) {
                return LuaConverter::format_clock(usec_since_epoch, format.value_or("%Y-%m-%dT%H:%M:%SZ"),
                                                  local.value_or(false));
            });

            // pre-register variables R0 to R19 with zero value
            for (int i = 0; i < MAX_REGISTERS; i++) {
                mLua["R" + std::to_string(i)] = 0.0;
            }

            // Register input value placeholder for write conversions.
            mLua["V"] = 0.0;

            // comile Lua expression as a function
            sol::load_result chunk = mLua.load(expression);
            if (!chunk.valid()) {
                sol::error err = chunk;
                throw ConvException(std::string("Lua compile error: ") + err.what());
            }

            mFunction = chunk;
            mPrecision = values[ConverterArg::sPrecisionArgName].as_int();

            #ifdef LUACONV_DEBUG
            dbgLogContext = expression;
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

        static double int32(const double highRegister, const double lowRegister) {
            return ConverterTools::toNumber<int32_t>(highRegister, lowRegister, false);
        }

        static double int32bs(const double highRegister, const double lowRegister) {
            return ConverterTools::toNumber<int32_t>(highRegister, lowRegister, true);
        }

        static double uint32(const double highRegister, const double lowRegister) {
            return ConverterTools::toNumber<uint32_t>(highRegister, lowRegister, false);
        }

        static double uint32bs(const double highRegister, const double lowRegister) {
            return ConverterTools::toNumber<uint32_t>(highRegister, lowRegister, true);
        }

        static double flt32(const double highRegister, const double lowRegister) {
            return ConverterTools::toNumber<float>(highRegister, lowRegister, false);
        }

        static double flt32bs(const double highRegister, const double lowRegister) {
            return ConverterTools::toNumber<float>(highRegister, lowRegister, true);
        }

        static double int16(const double regValue) {
            uint16_t val = uint16_t(regValue);
            return (int16_t)val;
        }

        static double int16bs(const double regValue) {
            uint16_t val = uint16_t(regValue);
            val = ConverterTools::setByteOrder(val, true);
            return (int16_t)(val);
        }

        static double uint16(const double regValue) {
            return uint16_t(regValue);
        }

        static double uint16bs(const double regValue) {
            uint16_t val = uint16_t(regValue);
            val = ConverterTools::setByteOrder(val, true);
            return val;
        }

        /// Return a comma-separated list of bit positions set to 1 in the argument.
        /// E.g. for bit_positions(6) returns "1,2"
        /// E.g. for bit_positions(6, 1) returns "2,3"
        static std::string bit_positions(uint64_t value, int lsb_base = 0)
        {
            std::string result;
            while (value != 0) {
                unsigned bit = std::countr_zero(value);
                if (!result.empty()) {
                    result += ",";
                }
                result += std::to_string(static_cast<int>(bit) + lsb_base);
                value &= ~(uint64_t(1) << bit);
            }
            return result;
        }

        /// Returns current system clock as microseconds since Unix epoch (UTC).
        /// Note: Returned as double (precision ~15 digits).
        static double clock_usec()
        {
            using namespace std::chrono;
            auto now = system_clock::now().time_since_epoch();
            auto us = duration_cast<microseconds>(now).count();
            return static_cast<double>(us);
        }

        /// Thread-safe conversion to local time
        static std::tm localtime_safe(std::time_t t)
        {
            std::tm tm{};

            #ifdef _WIN32
            localtime_s(&tm, &t);
            #else
            localtime_r(&t, &tm);
            #endif

            return tm;
        }

        /// Thread-safe conversion to UTC time
        static std::tm gmtime_safe(std::time_t t)
        {
            std::tm tm{};

            #ifdef _WIN32
            gmtime_s(&tm, &t);
            #else
            gmtime_r(&t, &tm);
            #endif

            return tm;
        }

        /// Formats epoch microseconds into string.
        ///
        /// @param usec_since_epoch Time since Unix epoch in microseconds (UTC base)
        /// @param format Format string (strftime compatible + %f for microseconds)
        /// @param local If true, format as local time; otherwise UTC time
        ///
        /// Example:
        ///   format_clock(ts, "%Y-%m-%dT%H:%M:%S.%f", true)
        ///
        /// %f = microseconds (000000 - 999999)
        static std::string
        format_clock(double usec_since_epoch, const std::string& format = "%Y-%m-%dT%H:%M:%SZ", bool local = false)
        {
            using namespace std::chrono;

            // Convert to integer microseconds
            int64_t usec = static_cast<int64_t>(usec_since_epoch);

            // Split into seconds + microseconds remainder
            std::time_t sec = static_cast<std::time_t>(usec / 1000000);
            int64_t micros = usec % 1000000;
            if (micros < 0) {
                micros += 1000000;
                --sec;
            }

            // Convert to tm (UTC or local)
            std::tm tm = local ? localtime_safe(sec) : gmtime_safe(sec);

            // Result string
            std::string result;
            result.reserve(format.size() + 8);  // 2 additional year digits + 6 microseconds

            // Pre-format microseconds as zero-padded 6 digits
            std::string microsStr = std::format("{:06}", micros);

            // Helper to append strftime-formatted fragment
            auto append_strftime = [&](std::string_view fmt_part) {
                if (fmt_part.empty()) {
                    return;
                }

                char buffer[128];
                std::size_t n = std::strftime(buffer, sizeof(buffer), std::string(fmt_part).c_str(), &tm);
                if (n == 0) {
                    result.append("format_clock: strftime failed");
                }
                else {
                    result.append(buffer, n);
                }
            };

            // Replace all %f occurrences manually
            std::size_t start = 0;
            while (true) {
                std::size_t pos = format.find("%f", start);
                if (pos == std::string::npos) {
                    append_strftime(std::string_view(format).substr(start));
                    break;
                }

                append_strftime(std::string_view(format).substr(start, pos - start));
                result.append(microsStr);
                start = pos + 2;
            }

            return result;
        }
};
