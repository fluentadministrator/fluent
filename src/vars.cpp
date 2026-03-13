#include "fluent.hpp"
#include <random>
#include <regex>

// ─── LET ────────────────────────────────────────────────────────────────────
// let variable1 be "String"
// let variable2 be 1
// let variable3 be a table containing "Hi", "Hello"
// let variable4 be a random number from 1 to 500
// let variable5 be "Hello" obscured as "HE####LLO####"
size_t FluentInterpreter::handleLet(const std::vector<std::string>& tok, size_t idx) {
    if (tok.size() < 4) return idx + 1;
    std::string name = tok[1];
    // tok[2] == "be"
    size_t vi = 3;

    // Table: let x be a table containing "a", "b"
    if (vi < tok.size() && tok[vi] == "a" && vi+1 < tok.size() && tok[vi+1] == "table") {
        FluentTable tbl;
        // find "containing"
        size_t ci = vi + 2;
        if (ci < tok.size() && tok[ci] == "containing") ci++;
        // rebuild rest of line to parse comma-separated
        std::string rest;
        for (size_t j = ci; j < tok.size(); j++) { if (j > ci) rest += " "; rest += tok[j]; }
        auto parts = splitCommaArgs(rest);
        for (auto& p : parts) {
            auto v = resolveValue(trim(p));
            tbl.items.push_back(v);
        }
        FluentValue val(tbl);
        setVar(name, val);
        return idx + 1;
    }

    // Random: let x be a random number from 1 to 500
    if (vi < tok.size() && tok[vi] == "a" && vi+1 < tok.size() && tok[vi+1] == "random") {
        double lo = 1, hi = 100;
        for (size_t j = vi; j < tok.size(); j++) {
            if (tok[j] == "from" && j+1 < tok.size()) lo = std::stod(tok[j+1]);
            if (tok[j] == "to"   && j+1 < tok.size()) hi = std::stod(tok[j+1]);
        }
        static std::mt19937 rng(std::random_device{}());
        std::uniform_int_distribution<long long> dist((long long)lo, (long long)hi);
        FluentValue val((double)dist(rng));
        val.is_random = true;
        setVar(name, val);
        return idx + 1;
    }

    // Obfuscated: let x be "Hello" obscured as "HE####LLO"
    {
        // find "obscured"
        size_t oi = vi;
        for (; oi < tok.size(); oi++) if (tok[oi] == "obscured") break;
        if (oi < tok.size() && tok[oi] == "obscured") {
            // value is everything between be and obscured
            std::string orig;
            for (size_t j = vi; j < oi; j++) { if (j > vi) orig += " "; orig += tok[j]; }
            orig = stripQuotes(trim(orig));
            std::string mask;
            if (oi+1 < tok.size() && tok[oi+1] == "as" && oi+2 < tok.size())
                mask = stripQuotes(tok[oi+2]);
            else mask = orig;
            FluentObfuscated ob{orig, mask};
            setVar(name, FluentValue(ob));
            return idx + 1;
        }
    }

    // Simple value
    std::string valStr;
    for (size_t j = vi; j < tok.size(); j++) { if (j > vi) valStr += " "; valStr += tok[j]; }
    setVar(name, resolveValue(trim(valStr)));
    return idx + 1;
}

// ─── CHANGE ──────────────────────────────────────────────────────────────────
// change variable1 to "Hello"
// change variable1 to be deobfuscated
size_t FluentInterpreter::handleChange(const std::vector<std::string>& tok, size_t idx) {
    if (tok.size() < 4) return idx + 1;
    std::string name = tok[1];
    // Handle "change X to be deobfuscated"
    bool deob = false;
    for (auto& t : tok) if (t == "deobfuscated") deob = true;
    if (deob) {
        FluentValue val = getVar(name);
        if (val.isObfuscated())
            setVar(name, FluentValue(val.asObfuscated().original));
        return idx + 1;
    }
    // tok[2] == "to", skip "be" if present
    size_t start = 3;
    if (start < tok.size() && tok[start] == "be") start++;
    std::string valStr;
    for (size_t j = start; j < tok.size(); j++) { if (j > start) valStr += " "; valStr += tok[j]; }
    setVar(name, resolveValue(trim(valStr)));
    return idx + 1;
}

// ─── ADD ─────────────────────────────────────────────────────────────────────
// add 5 to variable2
// add "Goodbye" to variable3
size_t FluentInterpreter::handleAdd(const std::vector<std::string>& tok, size_t idx) {
    if (tok.size() < 4) return idx + 1;
    // find "to"
    size_t ti = 1;
    for (; ti < tok.size(); ti++) if (tok[ti] == "to") break;
    if (ti >= tok.size()) return idx + 1;

    std::string valStr;
    for (size_t j = 1; j < ti; j++) { if (j > 1) valStr += " "; valStr += tok[j]; }
    std::string varname = tok[ti+1];

    FluentValue& var = getOrCreateVar(varname);
    FluentValue addVal = resolveValue(trim(valStr));

    if (var.isTable()) {
        var.asTable().items.push_back(addVal);
    } else if (var.isNumber() && addVal.isNumber()) {
        var = FluentValue(var.asNumber() + addVal.asNumber());
    } else {
        // concatenate text
        var = FluentValue(var.toString() + addVal.toString());
    }
    return idx + 1;
}

// ─── REMOVE ──────────────────────────────────────────────────────────────────
// remove "Hi" from variable3
size_t FluentInterpreter::handleRemove(const std::vector<std::string>& tok, size_t idx) {
    if (tok.size() < 4) return idx + 1;
    size_t fi = 1;
    for (; fi < tok.size(); fi++) if (tok[fi] == "from") break;
    if (fi >= tok.size()) return idx + 1;

    std::string valStr;
    for (size_t j = 1; j < fi; j++) { if (j > 1) valStr += " "; valStr += tok[j]; }
    std::string varname = tok[fi+1];
    FluentValue removeVal = resolveValue(trim(valStr));

    FluentValue& var = getOrCreateVar(varname);
    if (var.isTable()) {
        auto& items = var.asTable().items;
        items.erase(std::remove_if(items.begin(), items.end(), [&](const FluentValue& v) {
            return v.toString() == removeVal.toString();
        }), items.end());
    }
    return idx + 1;
}

// ─── SUBTRACT ────────────────────────────────────────────────────────────────
size_t FluentInterpreter::handleSubtract(const std::vector<std::string>& tok, size_t idx) {
    if (tok.size() < 4) return idx + 1;
    // subtract 5 to/from variable2
    double amount = 0;
    try { amount = std::stod(tok[1]); } catch (...) {}
    std::string varname = tok.back();
    FluentValue& var = getOrCreateVar(varname);
    if (var.isNumber()) var = FluentValue(var.asNumber() - amount);
    return idx + 1;
}

// ─── DIVIDE ──────────────────────────────────────────────────────────────────
size_t FluentInterpreter::handleDivide(const std::vector<std::string>& tok, size_t idx) {
    if (tok.size() < 4) return idx + 1;
    double amount = 0;
    try { amount = std::stod(tok[1]); } catch (...) {}
    std::string varname = tok.back();
    FluentValue& var = getOrCreateVar(varname);
    if (var.isNumber() && amount != 0) var = FluentValue(var.asNumber() / amount);
    return idx + 1;
}

// ─── MULTIPLY ────────────────────────────────────────────────────────────────
size_t FluentInterpreter::handleMultiply(const std::vector<std::string>& tok, size_t idx) {
    if (tok.size() < 4) return idx + 1;
    double amount = 0;
    try { amount = std::stod(tok[1]); } catch (...) {}
    std::string varname = tok.back();
    FluentValue& var = getOrCreateVar(varname);
    if (var.isNumber()) var = FluentValue(var.asNumber() * amount);
    return idx + 1;
}

// ─── OBFUSCATE ────────────────────────────────────────────────────────────────
// obfuscate variable2 and turn the obscured text to "lol obfuscated"
size_t FluentInterpreter::handleObfuscate(const std::vector<std::string>& tok, size_t idx) {
    if (tok.size() < 2) return idx + 1;
    std::string name = tok[1];
    FluentValue val = getVar(name);
    std::string orig = val.toString();
    std::string mask = orig;
    // find mask after "to"
    for (size_t j = 2; j < tok.size(); j++) {
        if (tok[j] == "to" && j+1 < tok.size()) {
            mask = stripQuotes(tok[j+1]);
            break;
        }
    }
    setVar(name, FluentValue(FluentObfuscated{orig, mask}));
    return idx + 1;
}

// ─── SET ─────────────────────────────────────────────────────────────────────
// set variable5 to be deobfuscated
size_t FluentInterpreter::handleSet(const std::vector<std::string>& tok, size_t idx) {
    if (tok.size() < 4) return idx + 1;
    std::string name = tok[1];
    // check for deobfuscated
    bool deob = false;
    for (auto& t : tok) if (t == "deobfuscated") deob = true;
    if (deob) {
        FluentValue val = getVar(name);
        if (val.isObfuscated()) {
            setVar(name, FluentValue(val.asObfuscated().original));
        }
        return idx + 1;
    }
    // set varname to value
    if (tok.size() >= 4 && tok[2] == "to") {
        std::string valStr;
        for (size_t j = 3; j < tok.size(); j++) { if (j > 3) valStr += " "; valStr += tok[j]; }
        setVar(name, resolveValue(trim(valStr)));
    }
    return idx + 1;
}

// ─── MAKE ─────────────────────────────────────────────────────────────────────
// make variable1 global
// make variable1 not global
// make a database containing "user1","user2" and type be "Users"
// make variable1 private and only public if ...
size_t FluentInterpreter::handleMake(const std::vector<std::string>& tok, size_t idx) {
    if (tok.size() < 2) return idx + 1;

    // make a database containing ...
    if (tok.size() >= 2 && tok[1] == "a" && tok.size() >= 3 && tok[2] == "database") {
        std::string type_name;
        std::vector<std::string> entries;
        for (size_t j = 3; j < tok.size(); j++) {
            if (tok[j] == "type" && j+2 < tok.size() && tok[j+1] == "be")
                type_name = stripQuotes(tok[j+2]);
        }
        size_t ci = 3;
        for (; ci < tok.size(); ci++) if (tok[ci] == "containing") break;
        if (ci < tok.size()) {
            std::string rest;
            for (size_t j = ci+1; j < tok.size(); j++) {
                if (tok[j] == "and") break;
                if (j > ci+1) rest += " ";
                rest += tok[j];
            }
            auto parts = splitCommaArgs(rest);
            for (auto& p : parts) entries.push_back(stripQuotes(trim(p)));
        }
        if (!type_name.empty()) databases_[type_name] = entries;
        return idx + 1;
    }

    std::string name = tok[1];

    // make variable1 global
    if (tok.size() >= 3 && tok[2] == "global") {
        FluentValue val = getVar(name);
        val.is_global = true;
        vars_.erase(name);
        globals_[name] = val;
        return idx + 1;
    }

    // make variable1 not global
    if (tok.size() >= 4 && tok[2] == "not" && tok[3] == "global") {
        if (globals_.count(name)) {
            vars_[name] = globals_[name];
            vars_[name].is_global = false;
            globals_.erase(name);
        }
        return idx + 1;
    }

    // make variable1 private ...
    if (tok.size() >= 3 && tok[2] == "private") {
        FluentValue& var = getOrCreateVar(name);
        var.is_private = true;
        std::string cond;
        for (size_t j = 3; j < tok.size(); j++) { if (j > 3) cond += " "; cond += tok[j]; }
        var.private_condition = cond;
        return idx + 1;
    }

    return idx + 1;
}

// ─── IMPORT ──────────────────────────────────────────────────────────────────
size_t FluentInterpreter::handleImport(const std::vector<std::string>& tok, size_t idx) {
    if (tok.size() < 2) return idx + 1;
    std::string name = stripQuotes(tok[1]);
    // Try to load a .fluent module file
    if (fs::exists(name + ".fluent")) {
        FluentInterpreter sub(name + ".fluent");
        sub.run();
        // export its globals
        for (auto& [k,v] : sub.globals_) globals_[k] = v;
        // also store module name
        modules_[name] = {};
        // create import_name variable
        setVar("import_" + name, FluentValue(name));
    } else {
        // store as string import
        setVar("import_" + name, FluentValue(name));
    }
    return idx + 1;
}
