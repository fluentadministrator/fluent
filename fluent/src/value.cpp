#include "fluent.hpp"
#include <sstream>
#include <iomanip>

std::string FluentValue::toString() const {
    if (isNull())       return "null";
    if (isNumber()) {
        double v = asNumber();
        if (v == (long long)v) return std::to_string((long long)v);
        std::ostringstream oss; oss << v; return oss.str();
    }
    if (isText())       return asText();
    if (isBool())       return asBool() ? "true" : "false";
    if (isObfuscated()) return asObfuscated().mask;
    if (isTable()) {
        std::string r = "[";
        const auto& items = asTable().items;
        for (size_t i = 0; i < items.size(); ++i) {
            if (i) r += ", ";
            r += items[i].toString();
        }
        return r + "]";
    }
    return "null";
}

std::string FluentValue::typeName() const {
    if (isNull())       return "null";
    if (isNumber())     return "number";
    if (isText())       return "text";
    if (isBool())       return "boolean";
    if (isTable())      return "table";
    if (isObfuscated()) return "obfuscated";
    return "unknown";
}
