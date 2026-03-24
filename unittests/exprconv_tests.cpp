#include <catch2/catch_all.hpp>
#include "libmodmqttconv/converterplugin.hpp"
#include "libmodmqttsrv/config.hpp"
#include "libmodmqttsrv/dll_import.hpp"
#include "plugin_utils.hpp"

#ifdef HAVE_EXPRTK

TEST_CASE ("A number should be converted by exprtk") {
    PluginLoader loader("../exprconv/exprconv.so");

    std::shared_ptr<DataConverter> conv(loader.getConverter("evaluate"));
    ConverterArgValues args(conv->getArgs());

    SECTION("when precision is not set") {
        args.setArgValue("expression", "R0 * 2");
        const ModbusRegisters input(10);

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getString() == "20");
    }

    SECTION("when precision is set") {
        args.setArgValue("expression", "R0 / 3");
        args.setArgValue(ConverterArg::sPrecisionArgName, "3");
        const ModbusRegisters input(10);

        conv->setArgValues(args);
        MqttValue output = conv->toMqtt(input);

        REQUIRE(output.getString() == "3.333");
    }
}

TEST_CASE ("A uint16_t register data should be converted to exprtk value") {
    PluginLoader loader("../exprconv/exprconv.so");

    std::shared_ptr<DataConverter> conv(loader.getConverter("evaluate"));
    ConverterArgValues args(conv->getArgs());

    args.setArgValue("expression", "int16(R0)");
    const ModbusRegisters input(0xFFFF);

    conv->setArgValues(args);
    MqttValue output = conv->toMqtt(input);

    REQUIRE(output.getString() == "-1");
}

TEST_CASE("ExprConv: 20 registers should be processed by exprtk") {
    PluginLoader loader("../exprconv/exprconv.so");

    std::shared_ptr<DataConverter> conv(loader.getConverter("evaluate"));
    ConverterArgValues args(conv->getArgs());

    args.setArgValue("expression", "R0+R1+R2+R3+R4+R5+R6+R7+R8+R9+R10+R11+R12+R13+R14+R15+R16+R17+R18+R19");
	args.setArgValue(ConverterArg::sPrecisionArgName, "0");

    const ModbusRegisters input({1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20});
    conv->setArgValues(args);

    MqttValue output = conv->toMqtt(input);

    REQUIRE(output.getString() == "210");
}

#endif
