#include "conv_name_parser.hpp"

#include <cctype>
#include <exception>
#include <memory>
#include <regex>
#include <string>
#include <string_view>

#include "libmodmqttconv/convargs.hpp"

#include "exceptions.hpp"
#include "strutils.hpp"

using namespace std::string_literals;

namespace modmqttd {

class ConverterArgParser {
    public:
        ConverterArgParser(const ConverterArgs& args) : mArgs(args)
        {}

        ConverterArgValues parse(const std::string& argStr);
    private:
        typedef enum {
            SCAN,
            ARGVALUE,
            ESCAPE
        } aState;

        ConverterArgs mArgs;
        bool mUseArgOrder = true;
        std::stack<aState> mCurrentState;
        std::string mArgName;
        std::string mArgValue;
        ConverterArgs::const_iterator mCurrentPosArgIterator;
        std::vector<std::string> mProcessedArgs;

        void setArgValue(ConverterArgValues& values);
        char getEscapedChar(const char& c) { return c; };
        const std::string& validateArgName(const std::string& argName);
};

const std::string&
ConverterArgParser::validateArgName(const std::string& argName) {
    //TODO check for chars outside a-zA-Z0-9
    return argName;
};

void
ConverterArgParser::setArgValue(ConverterArgValues& values) {
    try {
        ConverterArgType argType = ConverterArgType::INVALID;
        if (!mArgName.empty()) {
            mUseArgOrder = false;
        } else {
            if (!mUseArgOrder)
                throw ConvNameParserException("Cannot use positional argument after named argument "s + std::to_string(mArgs.size()));

            if (mCurrentPosArgIterator == mArgs.end())
                throw ConvNameParserException("Too many arguments provided, need "s + std::to_string(mArgs.size()));
                mArgName = mCurrentPosArgIterator->mName;
            mCurrentPosArgIterator++;
        }

        auto it = std::find(mProcessedArgs.begin(), mProcessedArgs.end(), mArgName);
        if (it != mProcessedArgs.end())
            throw ConvNameParserException(mArgName + " already set");

        values.setArgValue(mArgName, mArgValue);
        mProcessedArgs.push_back(mArgName);
        mArgName.clear();
        mArgValue.clear();
    } catch (const std::exception& ex) {
        throw ConvNameParserException("Error setting argument " + mArgName + ":" + ex.what());
    }
};

ConverterArgValues
ConverterArgParser::parse(const std::string& argStr) {
    ConverterArgValues ret(mArgs);

    mProcessedArgs.clear();
    mUseArgOrder = true;
    mCurrentPosArgIterator = mArgs.begin();
    mArgName.clear();
    mArgValue.clear();

    auto trimView = [](std::string_view v) -> std::string_view {
        while (!v.empty() && std::isspace(static_cast<unsigned char>(v.front())))
            v.remove_prefix(1);
        while (!v.empty() && std::isspace(static_cast<unsigned char>(v.back())))
            v.remove_suffix(1);
        return v;
    };

    auto isQuoted = [](std::string_view v) -> bool {
        return v.size() >= 2 &&
               ((v.front() == '\'' && v.back() == '\'') ||
                (v.front() == '"'  && v.back() == '"'));
    };

    auto unquote = [isQuoted](std::string_view v) -> std::string {
        if (isQuoted(v))
            v = v.substr(1, v.size() - 2);
        return std::string(v);
    };

    auto findTopLevelEqual = [](std::string_view v) -> std::size_t {
        char quote = '\0';
        bool escape = false;

        for (std::size_t i = 0; i < v.size(); ++i) {
            const char c = v[i];

            if (escape) {
                escape = false;
                continue;
            }

            if (quote != '\0') {
                if (c == '\\') {
                    escape = true;
                } else if (c == quote) {
                    quote = '\0';
                }
                continue;
            }

            if (c == '\'' || c == '"') {
                quote = c;
                continue;
            }

            if (c == '=') {
                return i;
            }
        }

        return std::string_view::npos;
    };

    auto flushToken = [&](std::string_view rawToken) {
        rawToken = trimView(rawToken);

        if (rawToken.empty()) {
            throw ConvNameParserException("Argument " + std::to_string(ret.count() + 1) + " is empty");
        }

        const std::size_t eqPos = findTopLevelEqual(rawToken);

        mArgName.clear();
        mArgValue.clear();

        if (eqPos == std::string_view::npos) {
            // Positional argument; quoted text stays a single value.
            mArgValue = unquote(trimView(rawToken));
        } else {
            auto nameView = trimView(rawToken.substr(0, eqPos));
            auto valueView = trimView(rawToken.substr(eqPos + 1));

            if (nameView.empty())
                throw ConvNameParserException("Missing name for argument " + std::to_string(ret.count() + 1));
            if (valueView.empty())
                throw ConvNameParserException("Missing value for argument " + std::to_string(ret.count() + 1));

            if (isQuoted(nameView))
                throw ConvNameParserException(
                    "Name for argument " + std::to_string(ret.count() + 1) + " cannot be quoted");

            mArgName = validateArgName(std::string(nameView));
            mArgValue = unquote(valueView);
        }

        setArgValue(ret);
    };

    std::string_view input(argStr);

    // empty argument list is valid, e.g. std.uint8()
    if (trimView(input).empty()) {
        return ret;
    }

    std::size_t tokenStart = 0;
    char quote = '\0';
    bool escape = false;

    for (std::size_t i = 0; i < input.size(); ++i) {
        const char c = input[i];

        if (escape) {
            escape = false;
            continue;
        }

        if (quote != '\0') {
            if (c == '\\') {
                escape = true;
            } else if (c == quote) {
                quote = '\0';
            }
            continue;
        }

        if (c == '\'' || c == '"') {
            quote = c;
            continue;
        }

        if (c == ',') {
            flushToken(input.substr(tokenStart, i - tokenStart));
            tokenStart = i + 1;
        }
    }

    if (quote != '\0')
        throw ConvNameParserException(
            "Argument " + std::to_string(ret.count() + 1) + " is an unterminated string");

    flushToken(input.substr(tokenStart));
    return ret;
}

ConverterSpecification
ConverterNameParser::parse(const std::string& spec) {
    std::regex re(RE_CONV);

    std::string val(spec);
    StrUtils::trim(val);

    std::cmatch matches;
    if (!std::regex_match(val.c_str(), matches, re))
        throw ConvNameParserException("Supply converter spec in form: plugin.converter(value1, param=value2, …)");

    ConverterSpecification ret;

    ret.plugin = matches[1];
    ret.converter = matches[2];

    ret.arguments = matches[3];
    return ret;
}

ConverterArgValues
ConverterNameParser::parseArgs(const ConverterArgs& args, const std::string& arguments) {
    ConverterArgParser p(args);
    return p.parse(arguments);
}

}; //namespace
