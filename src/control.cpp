#include "fluent.hpp"
#include <random>

// ─── CONDITION EVALUATOR ─────────────────────────────────────────────────────
bool FluentInterpreter::evalCondition(const std::vector<std::string>& tok, size_t start, size_t end) {
    // Collect condition tokens between start and end (exclusive)
    if (start >= end) return false;

    // "variable is a text/number/table/obfuscated/boolean/random"
    // "variable is below/above/equal N"
    // "variable is N"
    // "variable is not ..."
    // "variable contains X"
    // "variable has a text inside called X"
    // "hardware is connected to the internet"

    std::string varname = tok[start];

    if (varname == "hardware") {
        // check if hardware is connected to the internet
        bool conn = false;
        for (size_t j = start; j < end; j++)
            if (tok[j] == "internet") conn = true;
        if (conn) {
#ifdef _WIN32
            return system("ping -n 1 8.8.8.8 >nul 2>&1") == 0;
#else
            return system("ping -c1 -W1 8.8.8.8 >/dev/null 2>&1") == 0;
#endif
        }
        return false;
    }

    FluentValue val = resolveValue(varname);
    if (start + 1 >= end) return val.asBool();

    std::string op = tok[start + 1]; // "is", "contains", "has"

    if (op == "contains" && start+2 < end) {
        FluentValue needle = resolveValue(tok[start+2]);
        if (val.isTable()) {
            for (auto& item : val.asTable().items)
                if (item.toString() == needle.toString()) return true;
            return false;
        }
        return val.toString().find(needle.toString()) != std::string::npos;
    }

    if (op == "has" && start+2 < end) {
        // has a text inside called "X"
        for (size_t j = start+2; j < end; j++) {
            if (tok[j] == "called" && j+1 < end) {
                std::string needle = stripQuotes(tok[j+1]);
                return val.toString().find(needle) != std::string::npos;
            }
        }
    }

    if (op == "is" && start+2 < end) {
        std::string what = tok[start+2];
        bool negate = false;
        if (what == "not" && start+3 < end) { negate = true; what = tok[start+3]; }

        bool result = false;
        if (what == "a" && start+3 < end) {
            std::string type = tok[start+3];
            if      (type == "text")       result = val.isText();
            else if (type == "number")     result = val.isNumber();
            else if (type == "table")      result = val.isTable();
            else if (type == "boolean")    result = val.isBool();
            else result = false;
        } else if (what == "obfuscated") {
            result = val.isObfuscated();
        } else if (what == "random") {
            result = val.is_random;
        } else if (what == "true") {
            result = val.isBool() && val.asBool();
        } else if (what == "false") {
            result = val.isBool() && !val.asBool();
        } else if (what == "below" && start+3 < end) {
            double cmp = resolveValue(tok[start+3]).isNumber()
                ? resolveValue(tok[start+3]).asNumber() : 0;
            result = val.isNumber() && val.asNumber() < cmp;
        } else if (what == "above" && start+3 < end) {
            double cmp = resolveValue(tok[start+3]).isNumber()
                ? resolveValue(tok[start+3]).asNumber() : 0;
            result = val.isNumber() && val.asNumber() > cmp;
        } else if (what == "obscured") {
            result = val.isObfuscated();
        } else {
            // "is" direct comparison
            FluentValue cmp = resolveValue(what);
            result = val.toString() == cmp.toString();
        }
        return negate ? !result : result;
    }

    // Fallback: truthy
    if (val.isBool()) return val.asBool();
    if (val.isNumber()) return val.asNumber() != 0;
    if (val.isText()) return !val.asText().empty();
    return false;
}

// ─── IF ──────────────────────────────────────────────────────────────────────
// if variable1 is a text then
//     ...
// otherwise
//     ...
// done
size_t FluentInterpreter::handleIf(size_t idx) {
    auto tok = tokenize(trim(lines_[idx]));
    // Parse condition: between "if" and "then"
    size_t then_pos = tok.size();
    for (size_t j = 1; j < tok.size(); j++)
        if (tok[j] == "then") { then_pos = j; break; }

    bool cond = evalCondition(tok, 1, then_pos);

    // Find otherwise and done
    auto [body_start, done_idx] = findBlock(idx);
    size_t otherwise_idx = done_idx;
    int depth = 0;
    for (size_t i = body_start; i < done_idx; i++) {
        auto t = tokenize(trim(lines_[i]));
        if (t.empty()) continue;
        if (t[0] == "if" || t[0] == "loop" || t[0] == "parallel" || t[0] == "when" || t[0] == "check") depth++;
        if (t[0] == "done") depth--;
        if (t[0] == "otherwise" && depth == 0) { otherwise_idx = i; break; }
    }

    if (cond) {
        executeBlock(body_start, otherwise_idx);
    } else {
        if (otherwise_idx < done_idx)
            executeBlock(otherwise_idx + 1, done_idx);
    }
    return done_idx + 1;
}

// ─── LOOP ────────────────────────────────────────────────────────────────────
// loop up to 500 seconds while doing
// loop up to 6 minutes while changing variable2 to 10 while doing
// loop only once while doing
size_t FluentInterpreter::handleLoop(size_t idx) {
    auto tok = tokenize(trim(lines_[idx]));
    auto [body_start, done_idx] = findBlock(idx);

    // "loop only once while doing"
    bool only_once = false;
    for (size_t j = 0; j < tok.size(); j++)
        if (tok[j] == "once") { only_once = true; break; }

    if (only_once) {
        executeBlock(body_start, done_idx);
        return done_idx + 1;
    }

    // Get duration: "loop up to N unit while ..."
    long long duration_ms = 1000;
    for (size_t j = 0; j < tok.size(); j++) {
        if (tok[j] == "to" && j+2 < tok.size()) {
            try {
                double v = std::stod(tok[j+1]);
                std::string unit = tok[j+2];
                if (unit=="seconds"||unit=="second") duration_ms = (long long)(v*1000);
                else if (unit=="minutes"||unit=="minute") duration_ms = (long long)(v*60000);
                else if (unit=="hours"||unit=="hour")   duration_ms = (long long)(v*3600000LL);
            } catch (...) {}
            break;
        }
    }

    // Inline variable change: "while changing variable2 to 10 while doing"
    std::string change_var;
    FluentValue change_val;
    bool has_change = false;
    for (size_t j = 0; j < tok.size(); j++) {
        if (tok[j] == "changing" && j+3 < tok.size()) {
            has_change = true;
            change_var = tok[j+1];
            // tok[j+2] == "to"
            change_val = resolveValue(tok[j+3]);
        }
    }

    auto start = std::chrono::steady_clock::now();
    while (running_) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
        if (elapsed >= duration_ms) break;
        if (has_change) setVar(change_var, change_val);
        executeBlock(body_start, done_idx);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return done_idx + 1;
}

// ─── REPEAT ──────────────────────────────────────────────────────────────────
// repeat variable2 while adding 10 stop only when variable2 is 500
// repeat variable2 while reducing 10 stop only when variable2 is 100
// repeat variable2 if variable1 has a text inside called "String" and stop only when...
size_t FluentInterpreter::handleRepeat(size_t idx) {
    auto tok = tokenize(trim(lines_[idx]));
    if (tok.size() < 2) return idx + 1;

    std::string var_name = tok[1];
    double step = 0;
    bool adding = false, reducing = false;

    for (size_t j = 2; j < tok.size(); j++) {
        if (tok[j] == "adding" && j+1 < tok.size()) {
            adding = true;
            try { step = std::stod(tok[j+1]); } catch (...) {}
        }
        if (tok[j] == "reducing" && j+1 < tok.size()) {
            reducing = true;
            try { step = std::stod(tok[j+1]); } catch (...) {}
        }
    }

    // find stop condition
    std::vector<std::string> stop_cond;
    for (size_t j = 0; j < tok.size(); j++) {
        if (tok[j] == "when" && j+1 < tok.size()) {
            for (size_t k = j+1; k < tok.size(); k++) stop_cond.push_back(tok[k]);
            break;
        }
    }

    // iterate up to some safety limit
    int safety = 100000;
    while (running_ && safety-- > 0) {
        // check stop condition
        if (!stop_cond.empty()) {
            if (evalCondition(stop_cond, 0, stop_cond.size())) break;
        } else break;

        if (adding)   { FluentValue& v = getOrCreateVar(var_name); if (v.isNumber()) v = FluentValue(v.asNumber() + step); }
        if (reducing) { FluentValue& v = getOrCreateVar(var_name); if (v.isNumber()) v = FluentValue(v.asNumber() - step); }
    }
    return idx + 1;
}

// ─── CHECK ───────────────────────────────────────────────────────────────────
// check variable2 if its odd if it is odd then ... otherwise ... done
size_t FluentInterpreter::handleCheck(size_t idx) {
    auto tok = tokenize(trim(lines_[idx]));
    // check varname if its odd/even/prime if it is ... then
    if (tok.size() < 4) return idx + 1;

    std::string varname = tok[1];
    FluentValue val = getVar(varname);

    std::string check_type;
    for (size_t j = 3; j < tok.size(); j++) {
        if (tok[j] == "odd")   check_type = "odd";
        if (tok[j] == "even")  check_type = "even";
        if (tok[j] == "prime") check_type = "prime";
    }

    bool result = false;
    if (val.isNumber()) {
        if (check_type == "odd")   result = isOdd(val.asNumber());
        if (check_type == "even")  result = isEven(val.asNumber());
        if (check_type == "prime") result = isPrime((long long)val.asNumber());
    }

    // Now find the if block
    auto [body_start, done_idx] = findBlock(idx);
    size_t otherwise_idx = done_idx;
    for (size_t i = body_start; i < done_idx; i++) {
        auto t = tokenize(trim(lines_[i]));
        if (!t.empty() && t[0] == "otherwise") { otherwise_idx = i; break; }
    }

    if (result) {
        executeBlock(body_start, otherwise_idx);
    } else {
        if (otherwise_idx < done_idx)
            executeBlock(otherwise_idx + 1, done_idx);
    }
    return done_idx + 1;
}

// ─── WHEN ────────────────────────────────────────────────────────────────────
// when variable2 changes into 5 then ... otherwise ... done
size_t FluentInterpreter::handleWhen(size_t idx) {
    auto tok = tokenize(trim(lines_[idx]));
    auto [body_start, done_idx] = findBlock(idx);

    if (tok.size() < 4) return done_idx + 1;
    std::string varname = tok[1];

    // Build watcher in background thread
    std::string target_str;
    bool check_becomes = false;

    for (size_t j = 2; j < tok.size(); j++) {
        if ((tok[j] == "changes" || tok[j] == "becomes") && j+2 < tok.size()) {
            // skip "into"/"true"/"false"
            if (tok[j+1] == "into" && j+2 < tok.size())
                target_str = stripQuotes(tok[j+2]);
            else
                target_str = tok[j+1];
            check_becomes = (tok[j] == "becomes");
            break;
        }
    }

    // Find otherwise
    size_t otherwise_idx = done_idx;
    for (size_t i = body_start; i < done_idx; i++) {
        auto t = tokenize(trim(lines_[i]));
        if (!t.empty() && t[0] == "otherwise") { otherwise_idx = i; break; }
    }

    // Snapshot the current value and compare
    FluentValue current = getVar(varname);
    bool matched = false;

    if (!target_str.empty()) {
        if (target_str == "true")  matched = current.isBool() && current.asBool();
        else if (target_str == "false") matched = current.isBool() && !current.asBool();
        else matched = current.toString() == target_str;
    }

    if (matched) {
        executeBlock(body_start, otherwise_idx);
    } else {
        if (otherwise_idx < done_idx)
            executeBlock(otherwise_idx + 1, done_idx);
    }
    return done_idx + 1;
}
