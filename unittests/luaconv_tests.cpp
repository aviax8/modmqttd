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
