#include <boost/dll/import.hpp>
#include <catch2/catch_all.hpp>
#include "libmodmqttconv/converterplugin.hpp"
#include "libmodmqttsrv/config.hpp"

TEST_CASE ("LuaConv: A number should be converted by Lua") {
    std::string stdconv_path = "../luaconv/luaconv.so";
    std::shared_ptr<ConverterPlugin> plugin = boost_dll_import<ConverterPlugin>(
        stdconv_path,
        "converter_plugin",
        boost::dll::load_mode::append_decorations
    );
    std::shared_ptr<DataConverter> conv(plugin->getConverter("evaluate"));

    SECTION("when precision is not set") {
        conv->setArgs({"return R0 * 2"});
        const ModbusRegisters input(10);

        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getString() == "20");
    }

    SECTION("when precision is set") {
        conv->setArgs({"return R0 / 3", "3"});
        const ModbusRegisters input(10);

        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getString() == "3.333");
    }
}


TEST_CASE("LuaConv: A 32-bit number should be converted by Lua") {
    std::string stdconv_path = "../luaconv/luaconv.so";
    std::shared_ptr<ConverterPlugin> plugin = boost_dll_import<ConverterPlugin>(
        stdconv_path,
        "converter_plugin",
        boost::dll::load_mode::append_decorations
    );
    std::shared_ptr<DataConverter> conv(plugin->getConverter("evaluate"));

    SECTION("when two registers contain a signed integer") {

        SECTION("and byte order is ABCD") {
            conv->setArgs({"return int32be(R0, R1)"});
            const ModbusRegisters input({0xfedc, 0xba98});
            const int32_t expected = 0xfedcba98;

            MqttValue output = conv->toMqtt(input);

            REQUIRE(output.getDouble() == expected);
            REQUIRE(output.getString() == "-19088744");
        }

        SECTION("and byte order is BADC") {
            conv->setArgs({"return int32(R0, R1)"});
            const ModbusRegisters input({0xdcfe, 0x98ba});
            const int32_t expected = 0xfedcba98;

            MqttValue output = conv->toMqtt(input);

            REQUIRE(output.getDouble() == expected);
            REQUIRE(output.getString() == "-19088744");
        }
    }

    SECTION("when two registers contain an unsigned integer") {

        SECTION("and byte order is ABCD") {
            conv->setArgs({"return uint32be(R0, R1)"});
            const ModbusRegisters input({0xdcfe, 0x98ba});

            MqttValue output = conv->toMqtt(input);

            REQUIRE(output.getDouble() == 0xdcfe98ba);
            REQUIRE(output.getString() == "3707672762");
        }

        SECTION("and byte order is BADC") {
            conv->setArgs({"return uint32(R0, R1)"});
            const ModbusRegisters input({0xdcfe, 0x98ba});

            MqttValue output = conv->toMqtt(input);

            REQUIRE(output.getDouble() == 0xfedcba98);
            REQUIRE(output.getString() == "4275878552");
        }
    }

    SECTION("when two registers contain a float") {
        const float expected = -123.456f; // 0xc2f6e979 in IEEE 754 hex representation
        const std::string expectedString = "-123.456001";

        SECTION("and byte order is ABCD") {
            conv->setArgs({"return flt32be(R0, R1)"});
            const ModbusRegisters input({0xc2f6, 0xe979});

            MqttValue output = conv->toMqtt(input);

            REQUIRE_THAT(output.getDouble(), Catch::Matchers::WithinULP(expected, 0));
            REQUIRE(output.getString() == expectedString);
        }

        SECTION("and byte order is CDAB") {
            conv->setArgs({"return flt32be(R1, R0)"});
            const ModbusRegisters input({0xe979, 0xc2f6});

            MqttValue output = conv->toMqtt(input);

            REQUIRE_THAT(output.getDouble(), Catch::Matchers::WithinULP(expected, 0));
            REQUIRE(output.getString() == expectedString);
        }

        SECTION("and byte order is BADC") {
            conv->setArgs({"return flt32(R0, R1)"});
            const ModbusRegisters input({0xf6c2, 0x79e9});

            MqttValue output = conv->toMqtt(input);

            REQUIRE_THAT(output.getDouble(), Catch::Matchers::WithinULP(expected, 0));
            REQUIRE(output.getString() == expectedString);
        }

        SECTION("and byte order is DCBA") {
            conv->setArgs({"return flt32(R1, R0)"});
            const ModbusRegisters input({0x79e9, 0xf6c2});

            MqttValue output = conv->toMqtt(input);

            REQUIRE_THAT(output.getDouble(), Catch::Matchers::WithinULP(expected, 0));
            REQUIRE(output.getString() == expectedString);
        }

        SECTION("and precision is set") {
            conv->setArgs({"return flt32be(R0, R1)", "3"});
            const ModbusRegisters input({0xc2f6, 0xe979});

            MqttValue output = conv->toMqtt(input);

            REQUIRE(output.getString() == "-123.456");
        }
    }
}

TEST_CASE ("LuaConv: A uint16_t register data should be converted to Lua value") {
    std::string stdconv_path = "../luaconv/luaconv.so";
    std::shared_ptr<ConverterPlugin> plugin = boost_dll_import<ConverterPlugin>(
        stdconv_path,
        "converter_plugin",
        boost::dll::load_mode::append_decorations
    );
    std::shared_ptr<DataConverter> conv(plugin->getConverter("evaluate"));

    conv->setArgs({"return int16(R0)"});
    const ModbusRegisters input(0xFFFF);

    MqttValue output = conv->toMqtt(input);

    REQUIRE(output.getString() == "-1");
}

TEST_CASE ("LuaConv: A 64-bit number should be converted to list of bits in Lua") {
    std::string stdconv_path = "../luaconv/luaconv.so";
    std::shared_ptr<ConverterPlugin> plugin = boost_dll_import<ConverterPlugin>(
        stdconv_path,
        "converter_plugin",
        boost::dll::load_mode::append_decorations
    );
    std::shared_ptr<DataConverter> conv(plugin->getConverter("evaluate"));

    SECTION("when a number is 0") {
        conv->setArgs({"return bit_positions(R0)"});
        const ModbusRegisters input({0x00});

        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getString() == "");
    }

    SECTION("when a number contains bit 63") {
      conv->setArgs({"return bit_positions(uint32be(R3, R2)<<32 | uint32be(R1, R0))"});
      const ModbusRegisters input({0x0201, 0x1002, 0x2004, 0x8040});

      MqttValue output = conv->toMqtt(input);

      REQUIRE(output.getString() == "0,9,17,28,34,45,54,63");
    }

    SECTION("when lsb_base is 1") {
      conv->setArgs({"return bit_positions(R0, 1)"});
      const ModbusRegisters input({0x4201});

      MqttValue output = conv->toMqtt(input);

      REQUIRE(output.getString() == "1,10,15");
    }

    SECTION("when lsb_base is -10") {
      conv->setArgs({"return bit_positions(R0, -10)"});
      const ModbusRegisters input({0x4201});

      MqttValue output = conv->toMqtt(input);

      REQUIRE(output.getString() == "-10,-1,4");
    }
    SECTION("when lsb_base is 32") {
      conv->setArgs({"return bit_positions(R0, 32)"});
      const ModbusRegisters input({0x4201});

      MqttValue output = conv->toMqtt(input);

      REQUIRE(output.getString() == "32,41,46");
    }

}

TEST_CASE("LuaConv: A map expression should be evaluated by Lua") {
    std::string stdconv_path = "../luaconv/luaconv.so";
    std::shared_ptr<ConverterPlugin> plugin = boost_dll_import<ConverterPlugin>(
        stdconv_path,
        "converter_plugin",
        boost::dll::load_mode::append_decorations
    );
    std::shared_ptr<DataConverter> conv(plugin->getConverter("evaluate"));

    conv->setArgs({"local v=R0&0x0F; return ({ [0]='map0', [1]='map1', [2]='map2', [3]='map3' })[v] or (v)"});

    SECTION("when a register is 0") {
        const ModbusRegisters input({0});

        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getString() == "map0");
    }

    SECTION("when a register is 3") {
        const ModbusRegisters input({3});

        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getString() == "map3");
    }

    SECTION("when a register is 4") {
        const ModbusRegisters input({4});

        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getDouble() == 4.0);
    }
}

TEST_CASE("LuaConv: A number should be converted to hex string") {
    std::string stdconv_path = "../luaconv/luaconv.so";
    std::shared_ptr<ConverterPlugin> plugin = boost_dll_import<ConverterPlugin>(
        stdconv_path,
        "converter_plugin",
        boost::dll::load_mode::append_decorations
    );
    std::shared_ptr<DataConverter> conv(plugin->getConverter("evaluate"));

    conv->setArgs({"return string.format('%04X', R0)"});

    const ModbusRegisters input({0xFC81});

    MqttValue output = conv->toMqtt(input);

    REQUIRE(output.getString() == "FC81");
}

TEST_CASE("LuaConv: 20 registers should be processed") {
    std::string stdconv_path = "../luaconv/luaconv.so";
    std::shared_ptr<ConverterPlugin> plugin = boost_dll_import<ConverterPlugin>(
        stdconv_path,
        "converter_plugin",
        boost::dll::load_mode::append_decorations
    );
    std::shared_ptr<DataConverter> conv(plugin->getConverter("evaluate"));

    conv->setArgs({"return R0+R1+R2+R3+R4+R5+R6+R7+R8+R9+R10+R11+R12+R13+R14+R15+R16+R17+R18+R19"});

    const ModbusRegisters input({1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20});

    MqttValue output = conv->toMqtt(input);

    REQUIRE(output.getString() == "210");
}

TEST_CASE("LuaConv: clock_usec should return current epoch time in microseconds") {
    using namespace std::chrono;

    std::string stdconv_path = "../luaconv/luaconv.so";
    std::shared_ptr<ConverterPlugin> plugin = boost_dll_import<ConverterPlugin>(
        stdconv_path,
        "converter_plugin",
        boost::dll::load_mode::append_decorations
    );
    std::shared_ptr<DataConverter> conv(plugin->getConverter("evaluate"));

    conv->setArgs({"return clock_usec()"});

    auto before = duration_cast<microseconds>(
        system_clock::now().time_since_epoch()
    ).count();

    const ModbusRegisters input({});
    MqttValue output = conv->toMqtt(input);

    auto after = duration_cast<microseconds>(
        system_clock::now().time_since_epoch()
    ).count();

    double value = output.getDouble();

    REQUIRE(value >= static_cast<double>(before));
    REQUIRE(value <= static_cast<double>(after));
}

TEST_CASE("LuaConv: format_clock should format epoch microseconds as default UTC ISO string") {
    std::string stdconv_path = "../luaconv/luaconv.so";
    std::shared_ptr<ConverterPlugin> plugin = boost_dll_import<ConverterPlugin>(
        stdconv_path,
        "converter_plugin",
        boost::dll::load_mode::append_decorations
    );
    std::shared_ptr<DataConverter> conv(plugin->getConverter("evaluate"));

    conv->setArgs({"return format_clock(0)"});

    const ModbusRegisters input({});
    MqttValue output = conv->toMqtt(input);

    REQUIRE(output.getString() == "1970-01-01T00:00:00Z");
}

TEST_CASE("LuaConv: format_clock should replace percent-f with microseconds") {
    std::string stdconv_path = "../luaconv/luaconv.so";
    std::shared_ptr<ConverterPlugin> plugin = boost_dll_import<ConverterPlugin>(
        stdconv_path,
        "converter_plugin",
        boost::dll::load_mode::append_decorations
    );
    std::shared_ptr<DataConverter> conv(plugin->getConverter("evaluate"));

    conv->setArgs({"return format_clock(1234567, '%Y-%m-%dT%H:%M:%S.%fZ', false)"});

    const ModbusRegisters input({});
    MqttValue output = conv->toMqtt(input);

    REQUIRE(output.getString() == "1970-01-01T00:00:01.234567Z");
}

TEST_CASE("LuaConv: format_clock should support custom format with percent-f") {
    std::string stdconv_path = "../luaconv/luaconv.so";
    std::shared_ptr<ConverterPlugin> plugin = boost_dll_import<ConverterPlugin>(
        stdconv_path,
        "converter_plugin",
        boost::dll::load_mode::append_decorations
    );
    std::shared_ptr<DataConverter> conv(plugin->getConverter("evaluate"));

    conv->setArgs({"return format_clock(1234567, '%Y/%m/%d %H:%M:%S.%f', false)"});

    const ModbusRegisters input({});
    MqttValue output = conv->toMqtt(input);

    REQUIRE(output.getString() == "1970/01/01 00:00:01.234567");
}

TEST_CASE("LuaConv: format_clock should work without percent-f in format string") {
    std::string stdconv_path = "../luaconv/luaconv.so";
    std::shared_ptr<ConverterPlugin> plugin = boost_dll_import<ConverterPlugin>(
        stdconv_path,
        "converter_plugin",
        boost::dll::load_mode::append_decorations
    );
    std::shared_ptr<DataConverter> conv(plugin->getConverter("evaluate"));

    conv->setArgs({"return format_clock(1234567, '%Y-%m-%dT%H:%M:%SZ', false)"});

    const ModbusRegisters input({});
    MqttValue output = conv->toMqtt(input);

    REQUIRE(output.getString() == "1970-01-01T00:00:01Z");
}

TEST_CASE("LuaConv: format_clock should format local time when local is true") {
    std::string stdconv_path = "../luaconv/luaconv.so";
    std::shared_ptr<ConverterPlugin> plugin = boost_dll_import<ConverterPlugin>(
        stdconv_path,
        "converter_plugin",
        boost::dll::load_mode::append_decorations
    );
    std::shared_ptr<DataConverter> conv(plugin->getConverter("evaluate"));

    conv->setArgs({"return format_clock(0, '%Y-%m-%dT%H:%M:%S.%f', true)"});

    const ModbusRegisters input({});
    MqttValue output = conv->toMqtt(input);

    std::time_t t = 0;
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif

    char buffer[64];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S.000000", &tm);

    REQUIRE(output.getString() == buffer);
}

TEST_CASE("LuaConv: format_clock should accept output of clock_usec") {
    std::string stdconv_path = "../luaconv/luaconv.so";
    std::shared_ptr<ConverterPlugin> plugin = boost_dll_import<ConverterPlugin>(
        stdconv_path,
        "converter_plugin",
        boost::dll::load_mode::append_decorations
    );
    std::shared_ptr<DataConverter> conv(plugin->getConverter("evaluate"));

    conv->setArgs({"return format_clock(clock_usec())"});

    const ModbusRegisters input({});
    MqttValue output = conv->toMqtt(input);

    std::string s = output.getString();

    REQUIRE(s.size() == 20);
    REQUIRE(s[4] == '-');
    REQUIRE(s[7] == '-');
    REQUIRE(s[10] == 'T');
    REQUIRE(s[13] == ':');
    REQUIRE(s[16] == ':');
    REQUIRE(s[19] == 'Z');
}

TEST_CASE("LuaConv: format_clock should format clock_usec with explicit microseconds format") {
    std::string stdconv_path = "../luaconv/luaconv.so";
    std::shared_ptr<ConverterPlugin> plugin = boost_dll_import<ConverterPlugin>(
        stdconv_path,
        "converter_plugin",
        boost::dll::load_mode::append_decorations
    );
    std::shared_ptr<DataConverter> conv(plugin->getConverter("evaluate"));

    conv->setArgs({"return format_clock(clock_usec(), '%Y-%m-%dT%H:%M:%S.%fZ', false)"});

    const ModbusRegisters input({});
    MqttValue output = conv->toMqtt(input);

    std::string s = output.getString();

    REQUIRE(s.size() == 27);
    REQUIRE(s[4] == '-');
    REQUIRE(s[7] == '-');
    REQUIRE(s[10] == 'T');
    REQUIRE(s[13] == ':');
    REQUIRE(s[16] == ':');
    REQUIRE(s[19] == '.');
    REQUIRE(s[26] == 'Z');
}

