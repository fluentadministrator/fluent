#include "fluent.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <fstream>
#include <limits>

// ─── SAY ─────────────────────────────────────────────────────────────────────
// say "Hello"
// say "Hello ", variable1
// say error "..."
// say warning "..."
// say hardware_fluent
// say "make variable5 be deobfuscated"  (inline deobf)
size_t FluentInterpreter::handleSay(const std::vector<std::string>& tok, size_t idx) {
    if (tok.size() < 2) return idx + 1;

    bool is_error   = (tok[1] == "error");
    bool is_warning = (tok[1] == "warning");
    size_t start = (is_error || is_warning) ? 2 : 1;

    // Collect all parts to print
    std::string output;
    for (size_t j = start; j < tok.size(); j++) {
        if (j > start) {
            // commas between parts are separators
            if (tok[j] == ",") continue;
        }
        std::string part = tok[j];
        // inline deobfuscation: "make variable5 be deobfuscated"
        if (part == "make" && j+3 < tok.size() && tok[j+2] == "be" && tok[j+3] == "deobfuscated") {
            FluentValue v = getVar(tok[j+1]);
            if (v.isObfuscated()) output += v.asObfuscated().original;
            else output += v.toString();
            j += 3;
            continue;
        }
        FluentValue v = resolveValue(part);
        output += v.toString();
    }

    if (is_error)   std::cerr << "[ERROR] " << output << "\n";
    else if (is_warning) std::cout << "[WARNING] " << output << "\n";
    else            std::cout << output << "\n";

    return idx + 1;
}

// ─── PARAGRAPH ────────────────────────────────────────────────────────────────
// paragraph "Hello...", continue "...", continue "..."
size_t FluentInterpreter::handleParagraph(const std::vector<std::string>& tok, size_t idx) {
    std::string output;
    bool next_is_text = false;
    for (size_t j = 1; j < tok.size(); j++) {
        if (tok[j] == "paragraph" || tok[j] == "continue") { next_is_text = true; continue; }
        if (tok[j] == ",") continue;
        if (next_is_text || tok[j].front() == '"') {
            output += stripQuotes(tok[j]) + " ";
            next_is_text = false;
        }
    }
    // trim trailing space
    if (!output.empty() && output.back() == ' ') output.pop_back();
    std::cout << output << "\n";
    return idx + 1;
}

// ─── WAIT ─────────────────────────────────────────────────────────────────────
// wait for 5 seconds
// wait for 5 seconds but if variable5 is obfuscated then change into 10 seconds
// wait for 10 seconds but if it reaches 5 seconds then repeat back to 10 seconds
size_t FluentInterpreter::handleWait(const std::vector<std::string>& tok, size_t idx) {
    // parse base duration after "for"
    size_t for_pos = 0;
    for (size_t j = 0; j < tok.size(); j++) if (tok[j] == "for") { for_pos = j; break; }

    long long base_ms = 0;
    double base_amount = 0;
    std::string base_unit = "seconds";
    if (for_pos + 1 < tok.size()) {
        try { base_amount = std::stod(tok[for_pos+1]); } catch (...) {}
        if (for_pos + 2 < tok.size()) base_unit = tok[for_pos+2];
        base_ms = parseDurationMs(tok, for_pos+1);
    }

    // Check for conditional override: "but if variable is X then change into N unit"
    bool has_but = false;
    size_t but_pos = 0;
    for (size_t j = 0; j < tok.size(); j++) if (tok[j] == "but") { has_but = true; but_pos = j; break; }

    if (has_but) {
        // Find "if" and condition
        size_t if_pos = but_pos;
        for (size_t j = but_pos; j < tok.size(); j++) if (tok[j] == "if") { if_pos = j; break; }
        // find "then"
        size_t then_pos = tok.size();
        for (size_t j = if_pos; j < tok.size(); j++) if (tok[j] == "then") { then_pos = j; break; }

        bool cond = evalCondition(tok, if_pos+1, then_pos);

        if (cond) {
            // find override duration after "into" or "be"
            long long override_ms = base_ms;
            for (size_t j = then_pos; j < tok.size(); j++) {
                if ((tok[j] == "into" || tok[j] == "be") && j+2 < tok.size()) {
                    override_ms = parseDurationMs(tok, j+1);
                    break;
                }
            }
            // check for "repeat" keyword
            bool repeat = false;
            for (auto& t : tok) if (t == "repeat") repeat = true;
            if (repeat) {
                // wait the full time, resetting
                std::this_thread::sleep_for(std::chrono::milliseconds(override_ms));
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(override_ms));
            }
            return idx + 1;
        }
    }

    // Check "if it reaches X seconds then ..."
    bool has_reach = false;
    long long reach_ms = 0;
    for (size_t j = 0; j < tok.size(); j++) {
        if (tok[j] == "reaches" && j+2 < tok.size()) {
            has_reach = true;
            reach_ms = parseDurationMs(tok, j+1);
            break;
        }
    }

    if (has_reach) {
        // Sleep for base_ms but trigger at reach_ms
        auto start = std::chrono::steady_clock::now();
        while (running_) {
            auto now = std::chrono::steady_clock::now();
            long long elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
            if (elapsed >= base_ms) break;
            if (elapsed >= reach_ms) {
                // check for "repeat back to base_ms"
                bool repeat = false;
                for (auto& t : tok) if (t == "repeat") repeat = true;
                if (repeat) { start = std::chrono::steady_clock::now(); continue; }
                // check for "instead N seconds"
                bool instead = false;
                long long instead_ms = 0;
                for (size_t j = 0; j < tok.size(); j++) {
                    if (tok[j] == "instead" && j+2 < tok.size()) {
                        instead = true;
                        instead_ms = parseDurationMs(tok, j+1);
                        break;
                    }
                    if (tok[j] == "be" && j+2 < tok.size()) {
                        // "make it be 15 seconds instead"
                        instead = true;
                        instead_ms = parseDurationMs(tok, j+1);
                    }
                }
                if (instead) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(instead_ms));
                    return idx + 1;
                }
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        return idx + 1;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(base_ms));
    return idx + 1;
}

// ─── ASK ─────────────────────────────────────────────────────────────────────
// ask "are you sure?" and have the options "Yes", "no"
// ask "are you sure?" and have the options "Yes", "no" if picked Yes then ... done
size_t FluentInterpreter::handleAsk(size_t idx) {
    auto tok = tokenize(trim(lines_[idx]));

    std::string question;
    std::vector<std::string> options;

    // Extract question
    for (size_t j = 1; j < tok.size(); j++) {
        if (tok[j].front() == '"') { question = stripQuotes(tok[j]); break; }
    }

    // Extract options after "options"
    size_t opt_pos = 0;
    for (size_t j = 0; j < tok.size(); j++) if (tok[j] == "options") { opt_pos = j; break; }
    if (opt_pos > 0) {
        for (size_t j = opt_pos+1; j < tok.size(); j++) {
            if (tok[j].front() == '"') options.push_back(stripQuotes(tok[j]));
            if (tok[j] == "if") break;
        }
    }

    // Display question
    std::cout << "\n? " << question << "\n";
    for (size_t i = 0; i < options.size(); i++)
        std::cout << "  [" << (i+1) << "] " << options[i] << "\n";
    std::cout << "> ";

    std::string input;
    std::getline(std::cin, input);
    // trim
    while (!input.empty() && (input.back()=='\r'||input.back()=='\n'||input.back()==' '))
        input.pop_back();

    // store as last_answer
    setVar("last_answer", FluentValue(input));

    // Check for "if picked X then" block
    size_t picked_pos = 0;
    std::string picked_val;
    for (size_t j = 0; j < tok.size(); j++) {
        if (tok[j] == "picked" && j+1 < tok.size()) {
            picked_pos = j;
            picked_val = stripQuotes(tok[j+1]);
            break;
        }
    }

    if (picked_pos > 0) {
        bool match = (input == picked_val);
        auto [body_start, done_idx] = findBlock(idx);
        if (match) executeBlock(body_start, done_idx);
        return done_idx + 1;
    }

    return idx + 1;
}

// ─── LOG ─────────────────────────────────────────────────────────────────────
// log error variable1 to file.txt
size_t FluentInterpreter::handleLog(const std::vector<std::string>& tok, size_t idx) {
    if (tok.size() < 4) return idx + 1;
    // log error variable1 to file.txt
    // tok[1] = level, tok[2] = varname, tok[3] = "to", tok[4] = filename
    std::string level = tok[1];
    std::string varname = tok[2];
    std::string filename;
    for (size_t j = 3; j < tok.size(); j++) {
        if (tok[j] == "to" && j+1 < tok.size()) { filename = tok[j+1]; break; }
    }
    if (filename.empty()) return idx + 1;

    FluentValue val = getVar(varname);
    std::ofstream f(filename, std::ios::app);
    if (f.is_open()) {
        // timestamp
        auto now = std::chrono::system_clock::now();
        auto t = std::chrono::system_clock::to_time_t(now);
        f << "[" << level << "] " << std::ctime(&t);
        f << val.toString() << "\n";
    }
    return idx + 1;
}

// ─── LIST ─────────────────────────────────────────────────────────────────────
// list import_module1
// list database_Users
size_t FluentInterpreter::handleList(const std::vector<std::string>& tok, size_t idx) {
    if (tok.size() < 2) return idx + 1;
    std::string name = tok[1];

    // Check databases first
    std::string db_prefix = "database_";
    if (name.substr(0, db_prefix.size()) == db_prefix) {
        std::string db_name = name.substr(db_prefix.size());
        if (databases_.count(db_name)) {
            std::cout << "Database '" << db_name << "':\n";
            for (auto& e : databases_[db_name])
                std::cout << "  - " << e << "\n";
            return idx + 1;
        }
    }

    // Check variable
    FluentValue val = getVar(name);
    if (val.isTable()) {
        std::cout << name << ":\n";
        for (auto& item : val.asTable().items)
            std::cout << "  - " << item.toString() << "\n";
    } else {
        std::cout << name << ": " << val.toString() << "\n";
    }
    return idx + 1;
}
