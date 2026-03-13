#include "fluent.hpp"
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <ctime>
#include <vector>

namespace fs = std::filesystem;

// ─── CREATE FILE ─────────────────────────────────────────────────────────────
// create a file called "Errors" with the file type as txt then wait up to 5 seconds then delete
// create a file called "No more" with the file type as txt then wait up to 5 seconds then rename to "Yes more"
size_t FluentInterpreter::handleCreateFile(const std::vector<std::string>& tok, size_t idx) {
    // Find file name
    std::string filename, filetype;
    for (size_t j = 0; j < tok.size(); j++) {
        if (tok[j] == "called" && j+1 < tok.size()) filename = stripQuotes(tok[j+1]);
        if (tok[j] == "as" && j+1 < tok.size()) filetype = tok[j+1];
    }
    if (filename.empty()) return idx + 1;
    if (!filetype.empty()) filename += "." + filetype;

    // Create the file
    { std::ofstream f(filename); f << ""; }
    // Created file: " << filename << "\n";

    // Parse chain actions after "then"
    // Look for wait, delete, rename
    for (size_t j = 0; j < tok.size(); j++) {
        if (tok[j] == "then") {
            if (j+1 < tok.size() && tok[j+1] == "wait") {
                // wait up to N seconds
                long long ms = 0;
                for (size_t k = j+1; k < tok.size(); k++) {
                    if (tok[k] == "up" && k+2 < tok.size()) {
                        ms = parseDurationMs(tok, k+2);
                        break;
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(ms));
            }
            if (j+1 < tok.size() && tok[j+1] == "delete") {
                fs::remove(filename);
                // Deleted file: " << filename << "\n";
            }
            if (j+1 < tok.size() && tok[j+1] == "rename") {
                // find "to"
                for (size_t k = j+1; k < tok.size(); k++) {
                    if (tok[k] == "to" && k+1 < tok.size()) {
                        std::string newname = stripQuotes(tok[k+1]);
                        // add extension
                        std::string ext = fs::path(filename).extension().string();
                        if (!ext.empty() && newname.find('.') == std::string::npos)
                            newname += ext;
                        fs::rename(filename, newname);
                        // Renamed to: " << newname << "\n";
                        break;
                    }
                }
            }
        }
    }
    return idx + 1;
}

// ─── PARALLEL ────────────────────────────────────────────────────────────────
// parallel
//   say "Hi"
//   say "Hello"
// done
size_t FluentInterpreter::handleParallel(size_t idx) {
    auto [body_start, done_idx] = findBlock(idx);

    // Collect lines in block
    std::vector<std::string> block_lines;
    for (size_t i = body_start; i < done_idx; i++)
        block_lines.push_back(lines_[i]);

    // Each line is a separate thread
    std::vector<std::thread> threads;
    for (auto& line : block_lines) {
        if (trim(line).empty()) continue;
        threads.emplace_back([this, line]() {
            auto tok = tokenize(trim(line));
            if (tok.empty()) return;
            // We need to handle a single-line mini-executor
            // We'll use a mini interpreter on just this line
            std::vector<std::string> saved = lines_;
            lines_ = {line};
            executeLine(0);
            lines_ = saved;
        });
    }
    for (auto& t : threads) if (t.joinable()) t.join();

    return done_idx + 1;
}

// ─── KEEP ─────────────────────────────────────────────────────────────────────
// keep variable2 in the system up to 5 hours
// keep file called "LOL" in the same directory in the system up to 5 hours
// keep variable1 as "String" do not let it be changed for up to 5 hours
size_t FluentInterpreter::handleKeep(const std::vector<std::string>& tok, size_t idx) {
    // Persist variable or file for duration
    // For this implementation: we print a note and lock the variable if needed
    std::string name = (tok.size() > 1) ? tok[1] : "";

    // find duration
    long long ms = 0;
    for (size_t j = 0; j < tok.size(); j++) {
        if (tok[j] == "up" && j+2 < tok.size()) {
            ms = parseDurationMs(tok, j+2);
            break;
        }
    }

    // "do not let it be changed" - lock the variable
    bool locked = false;
    for (size_t j = 0; j < tok.size(); j++)
        if (tok[j] == "not" && j+1 < tok.size() && tok[j+1] == "let") locked = true;

    if (!name.empty() && name != "file") {
        FluentValue val = getVar(name);
        // Schedule a background thread to restore value if changed
        if (locked) {
            std::string saved_val = val.toString();
            std::thread([this, name, saved_val, ms]() {
                auto start = std::chrono::steady_clock::now();
                while (running_) {
                    auto now = std::chrono::steady_clock::now();
                    if (std::chrono::duration_cast<std::chrono::milliseconds>(now-start).count() >= ms) break;
                    // restore if changed
                    FluentValue cur = getVar(name);
                    if (cur.toString() != saved_val)
                        setVar(name, FluentValue(saved_val));
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            }).detach();
        }
    }

    if (name == "file") {
        // keep file called "X" - just note it
        std::string fname;
        for (size_t j = 0; j < tok.size(); j++)
            if (tok[j] == "called" && j+1 < tok.size()) fname = stripQuotes(tok[j+1]);
        // Keeping file: " << fname << " for " << ms << "ms\n";
        // Schedule deletion after duration
        std::thread([fname, ms]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(ms));
            fs::remove(fname);
        }).detach();
    }

    return idx + 1;
}

// ─── SCHEDULE ─────────────────────────────────────────────────────────────────
// schedule at 10:00PM and when reached say "Its 10:00PM sleep"
size_t FluentInterpreter::handleSchedule(const std::vector<std::string>& tok, size_t idx) {
    std::string time_str;
    for (size_t j = 0; j < tok.size(); j++)
        if (tok[j] == "at" && j+1 < tok.size()) { time_str = tok[j+1]; break; }

    // Parse time (basic)
    int hour = 0, minute = 0;
    bool pm = false;
    if (!time_str.empty()) {
        auto colon = time_str.find(':');
        if (colon != std::string::npos) {
            try { hour = std::stoi(time_str.substr(0, colon)); } catch (...) {}
            std::string rest = time_str.substr(colon+1);
            if (rest.size() >= 2) {
                try { minute = std::stoi(rest.substr(0, 2)); } catch (...) {}
                if (rest.find("PM") != std::string::npos || rest.find("pm") != std::string::npos) pm = true;
            }
        }
    }
    if (pm && hour != 12) hour += 12;

    // Find the say/action after "when reached"
    std::vector<std::string> action_tok;
    for (size_t j = 0; j < tok.size(); j++)
        if (tok[j] == "reached" && j+1 < tok.size()) {
            for (size_t k = j+1; k < tok.size(); k++) action_tok.push_back(tok[k]);
            break;
        }

    // Schedule in background
    std::thread([this, hour, minute, action_tok]() {
        while (running_) {
            auto now = std::chrono::system_clock::now();
            std::time_t t = std::chrono::system_clock::to_time_t(now);
            std::tm* ltm = std::localtime(&t);
            if (ltm->tm_hour == hour && ltm->tm_min == minute) {
                if (!action_tok.empty() && action_tok[0] == "say") {
                    std::string msg;
                    for (size_t j = 1; j < action_tok.size(); j++) {
                        if (j > 1) msg += " ";
                        msg += stripQuotes(action_tok[j]);
                    }
                    std::cout << msg << "\n";
                }
                break;
            }
            std::this_thread::sleep_for(std::chrono::seconds(30));
        }
    }).detach();

    return idx + 1;
}

// ─── OPEN APPLICATION ────────────────────────────────────────────────────────
// open application called "cmd.exe" on path C:/
// open application called "cmd.exe"
size_t FluentInterpreter::handleOpen(const std::vector<std::string>& tok, size_t idx) {
    std::string appname, path;
    for (size_t j = 0; j < tok.size(); j++) {
        if (tok[j] == "called" && j+1 < tok.size()) appname = stripQuotes(tok[j+1]);
        if (tok[j] == "path" && j+1 < tok.size()) path = tok[j+1];
    }
    if (appname.empty()) return idx + 1;

    std::string cmd;
    if (!path.empty()) {
#ifdef _WIN32
        cmd = "start \"\" \"" + path + appname + "\"";
#else
        cmd = path + appname + " &";
#endif
    } else {
#ifdef _WIN32
        cmd = "start \"\" \"" + appname + "\"";
#else
        cmd = appname + " &";
#endif
    }
    std::system(cmd.c_str());
    return idx + 1;
}

// ─── KILL APPLICATION ────────────────────────────────────────────────────────
// kill application called "cmd.exe"
size_t FluentInterpreter::handleKill(const std::vector<std::string>& tok, size_t idx) {
    std::string appname;
    for (size_t j = 0; j < tok.size(); j++)
        if (tok[j] == "called" && j+1 < tok.size()) appname = stripQuotes(tok[j+1]);
    if (appname.empty()) return idx + 1;

#ifdef _WIN32
    std::system(("taskkill /IM " + appname + " /F >nul 2>&1").c_str());
#else
    std::system(("pkill -f " + appname + " 2>/dev/null").c_str());
#endif
    return idx + 1;
}

// ─── SYSTEM ──────────────────────────────────────────────────────────────────
// system shutdown / system restart
size_t FluentInterpreter::handleSystem(const std::vector<std::string>& tok, size_t idx) {
    if (tok.size() < 2) return idx + 1;
    if (tok[1] == "shutdown") {
#ifdef _WIN32
        std::system("shutdown /s /t 0");
#else
        std::system("shutdown -h now");
#endif
    } else if (tok[1] == "restart") {
#ifdef _WIN32
        std::system("shutdown /r /t 0");
#else
        std::system("reboot");
#endif
    }
    return idx + 1;
}

// ─── GUI ─────────────────────────────────────────────────────────────────────
// put button on gui1 and be called "Click for free robux" and the position be X, Y, Z ...
// put text on gui1 and text be "Welcome" and the position be X, Y, Z ...
// put bar on gui1 and position be X, Y, Z and color be colorname
size_t FluentInterpreter::handleGUI(const std::vector<std::string>& tok, size_t idx) {
    GUIElement el;
    if (tok.size() < 3) return idx + 1;
    el.type = tok[1]; // button, text, bar
    el.gui_id = tok[3]; // gui1

    for (size_t j = 0; j < tok.size(); j++) {
        if (tok[j] == "called" && j+1 < tok.size()) el.label = stripQuotes(tok[j+1]);
        if (tok[j] == "be" && j+1 < tok.size() && tok[j-1] != "not" && j >= 1) {
            // text be "..."
            if (j >= 1 && tok[j-1] == "text") el.label = stripQuotes(tok[j+1]);
        }
        if (tok[j] == "position" && j+2 < tok.size() && tok[j+1] == "be") {
            try {
                el.x = (int)std::stod(tok[j+2]);
                if (j+3 < tok.size()) el.y = (int)std::stod(tok[j+3]);
                if (j+4 < tok.size()) el.z = (int)std::stod(tok[j+4]);
            } catch (...) {}
        }
        if (tok[j] == "color" && j+2 < tok.size() && tok[j+1] == "be") {
            el.fg_color = tok[j+2];
        }
        if (tok[j] == "background" && j+2 < tok.size() && tok[j+1] == "be") {
            el.bg_color = tok[j+2];
        }
    }

    gui_elements_.push_back(el);

    renderGUI(el.gui_id);
    return idx + 1;
}

void FluentInterpreter::renderGUI(const std::string& gui_id) {
    std::cout << "\n+--- " << gui_id << " ----------------------------+\n";
    for (auto& el : gui_elements_) {
        if (el.gui_id != gui_id) continue;
        if (el.type == "button") {
            std::cout << "|  [" << el.label << "] x=" << el.x << " y=" << el.y;
            if (!el.bg_color.empty()) std::cout << " bg=" << el.bg_color;
            std::cout << "\n";
        } else if (el.type == "text") {
            std::cout << "|  " << el.label << " x=" << el.x << " y=" << el.y << "\n";
        } else if (el.type == "bar") {
            std::cout << "|  [############] x=" << el.x << " y=" << el.y;
            if (!el.fg_color.empty()) std::cout << " color=" << el.fg_color;
            std::cout << "\n";
        }
    }
    std::cout << "+--------------------------------------------+\n\n";
}
