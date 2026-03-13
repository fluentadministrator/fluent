#include "fluent.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <fstream>
#include <sstream>

// ─── SAY ─────────────────────────────────────────────────────────────────────
size_t FluentInterpreter::handleSay(const std::vector<std::string>& tok, size_t idx) {
    if (tok.size() < 2) return idx + 1;

    bool is_error   = (tok[1] == "error");
    bool is_warning = (tok[1] == "warning");
    size_t start = (is_error || is_warning) ? 2 : 1;

    std::string output;
    for (size_t j = start; j < tok.size(); j++) {
        if (tok[j] == ",") continue;

        // inline deobfuscation
        if (tok[j] == "make" && j+3 < tok.size() && tok[j+2] == "be" && tok[j+3] == "deobfuscated") {
            FluentValue v = getVar(tok[j+1]);
            output += v.isObfuscated() ? v.asObfuscated().original : v.toString();
            j += 3;
            continue;
        }

        // Quoted string
        if (tok[j].size() >= 2 && tok[j].front() == '"' && tok[j].back() == '"') {
            output += tok[j].substr(1, tok[j].size() - 2);
            continue;
        }

        // Bare token - resolve as variable first (hardware builtins, user vars, literals)
        FluentValue v = getVar(tok[j]);
        if (!v.isNull()) { output += v.toString(); continue; }
        // fallback to literal resolution (numbers, booleans)
        v = resolveValue(tok[j]);
        output += v.toString();
    }

    if (is_error)        std::cerr << "[ERROR] " << output << "\n";
    else if (is_warning) std::cout << "[WARNING] " << output << "\n";
    else                 std::cout << output << "\n";

    return idx + 1;
}

// ─── PARAGRAPH ────────────────────────────────────────────────────────────────
size_t FluentInterpreter::handleParagraph(const std::vector<std::string>& tok, size_t idx) {
    std::string output;
    for (size_t j = 1; j < tok.size(); j++) {
        if (tok[j] == "paragraph" || tok[j] == "continue" || tok[j] == ",") continue;
        if (tok[j].front() == '"')
            output += stripQuotes(tok[j]) + " ";
    }
    if (!output.empty() && output.back() == ' ') output.pop_back();
    std::cout << output << "\n";
    return idx + 1;
}

// ─── WAIT ─────────────────────────────────────────────────────────────────────
size_t FluentInterpreter::handleWait(const std::vector<std::string>& tok, size_t idx) {
    size_t for_pos = 0;
    for (size_t j = 0; j < tok.size(); j++) if (tok[j] == "for") { for_pos = j; break; }
    long long base_ms = for_pos + 1 < tok.size() ? parseDurationMs(tok, for_pos + 1) : 0;

    // "but if ... then change into N"
    for (size_t j = 0; j < tok.size(); j++) {
        if (tok[j] == "but") {
            size_t if_p = j, then_p = tok.size();
            for (size_t k = j; k < tok.size(); k++) if (tok[k] == "if") { if_p = k; break; }
            for (size_t k = if_p; k < tok.size(); k++) if (tok[k] == "then") { then_p = k; break; }
            if (evalCondition(tok, if_p + 1, then_p)) {
                for (size_t k = then_p; k < tok.size(); k++)
                    if ((tok[k] == "into" || tok[k] == "be") && k+2 < tok.size())
                        { base_ms = parseDurationMs(tok, k+1); break; }
            }
            break;
        }
    }

    // "if it reaches N then ..."
    for (size_t j = 0; j < tok.size(); j++) {
        if (tok[j] == "reaches" && j+2 < tok.size()) {
            long long reach_ms = parseDurationMs(tok, j+1);
            bool do_repeat = false;
            long long instead_ms = 0;
            for (auto& t : tok) if (t == "repeat") do_repeat = true;
            for (size_t k = 0; k < tok.size(); k++)
                if (tok[k] == "be" && k+2 < tok.size()) instead_ms = parseDurationMs(tok, k+1);

            auto t0 = std::chrono::steady_clock::now();
            while (running_) {
                long long el = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - t0).count();
                if (el >= base_ms) break;
                if (el >= reach_ms) {
                    if (do_repeat) { t0 = std::chrono::steady_clock::now(); continue; }
                    if (instead_ms > 0) { std::this_thread::sleep_for(std::chrono::milliseconds(instead_ms)); return idx+1; }
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            return idx + 1;
        }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(base_ms));
    return idx + 1;
}

// ─── ASK ─────────────────────────────────────────────────────────────────────
size_t FluentInterpreter::handleAsk(size_t idx) {
    auto tok = tokenize(trim(lines_[idx]));
    std::string question;
    std::vector<std::string> options;

    for (size_t j = 1; j < tok.size(); j++)
        if (!tok[j].empty() && tok[j].front() == '"') { question = stripQuotes(tok[j]); break; }

    size_t opt_pos = 0;
    for (size_t j = 0; j < tok.size(); j++) if (tok[j] == "options") { opt_pos = j; break; }
    if (opt_pos > 0)
        for (size_t j = opt_pos+1; j < tok.size(); j++) {
            if (tok[j] == "if") break;
            if (!tok[j].empty() && tok[j].front() == '"') options.push_back(stripQuotes(tok[j]));
        }

    // Display prompt
    std::cout << "\n? " << question << "\n";
    for (size_t i = 0; i < options.size(); i++)
        std::cout << "  [" << (i+1) << "] " << options[i] << "\n";
    std::cout << "> " << std::flush;

    std::string input;
    std::getline(std::cin, input);
    // trim CR/LF/spaces
    while (!input.empty() && ((unsigned char)input.back() <= 32)) input.pop_back();

    // Resolve input: if user typed a number like "1", map to option label
    std::string resolved_input = input;
    try {
        int idx_num = std::stoi(input);
        if (idx_num >= 1 && idx_num <= (int)options.size())
            resolved_input = options[idx_num - 1];
    } catch (...) {}

    // Store both raw and resolved
    setVar("last_answer", FluentValue(resolved_input));

    // Find "if picked X then" block
    size_t picked_pos = 0;
    std::string picked_val;
    for (size_t j = 0; j < tok.size(); j++)
        if (tok[j] == "picked" && j+1 < tok.size()) {
            picked_pos = j;
            picked_val = stripQuotes(tok[j+1]);
            break;
        }

    if (picked_pos > 0) {
        // Match against label OR number
        bool match = (resolved_input == picked_val) || (input == picked_val);
        auto [body_start, done_idx] = findBlock(idx);
        // Find otherwise within the block
        size_t otherwise_idx = done_idx;
        for (size_t i = body_start; i < done_idx; i++) {
            auto t = tokenize(trim(lines_[i]));
            if (!t.empty() && t[0] == "otherwise") { otherwise_idx = i; break; }
        }
        if (match) {
            executeBlock(body_start, otherwise_idx);
        } else {
            if (otherwise_idx < done_idx)
                executeBlock(otherwise_idx + 1, done_idx);
        }
        return done_idx + 1;
    }

    // No "if picked" on same line - check if NEXT non-empty line is "if picked"
    // so user can write ask on one line, then if picked on next line
    size_t next = idx + 1;
    while (next < lines_.size() && trim(lines_[next]).empty()) next++;
    if (next < lines_.size()) {
        auto ntok = tokenize(trim(lines_[next]));
        if (!ntok.empty() && ntok[0] == "if" && ntok.size() >= 3 && ntok[1] == "picked") {
            // delegate to handleIf but inject the match result into a temp var
            std::string pick_target = stripQuotes(ntok[2]);
            bool match = (resolved_input == pick_target) || (input == pick_target);
            // temporarily set a var __pick_match__ and run the if block
            setVar("__pick_match__", FluentValue(match));
            return handleIf(next);
        }
    }

    return idx + 1;
}

// ─── LOG ─────────────────────────────────────────────────────────────────────
size_t FluentInterpreter::handleLog(const std::vector<std::string>& tok, size_t idx) {
    if (tok.size() < 4) return idx + 1;
    std::string level = tok[1], varname = tok[2], filename;
    for (size_t j = 3; j < tok.size(); j++)
        if (tok[j] == "to" && j+1 < tok.size()) { filename = tok[j+1]; break; }
    if (filename.empty()) return idx + 1;
    FluentValue val = getVar(varname);
    std::ofstream f(filename, std::ios::app);
    if (f.is_open()) {
        auto now = std::chrono::system_clock::now();
        auto t   = std::chrono::system_clock::to_time_t(now);
        char tbuf[32]; std::strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
        f << "[" << level << "] " << tbuf << " " << val.toString() << "\n";
    }
    return idx + 1;
}

// ─── LIST ─────────────────────────────────────────────────────────────────────
size_t FluentInterpreter::handleList(const std::vector<std::string>& tok, size_t idx) {
    if (tok.size() < 2) return idx + 1;
    std::string name = tok[1];
    if (name.size() > 9 && name.substr(0, 9) == "database_") {
        std::string db = name.substr(9);
        if (databases_.count(db)) {
            std::cout << "Database '" << db << "':\n";
            for (auto& e : databases_[db]) std::cout << "  - " << e << "\n";
            return idx + 1;
        }
    }
    FluentValue val = getVar(name);
    if (val.isTable()) {
        std::cout << name << ":\n";
        for (auto& item : val.asTable().items) std::cout << "  - " << item.toString() << "\n";
    } else {
        std::cout << name << ": " << val.toString() << "\n";
    }
    return idx + 1;
}
