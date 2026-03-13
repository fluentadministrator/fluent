#include "fluent.hpp"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <thread>

// ─── Constructor ────────────────────────────────────────────────────────────
FluentInterpreter::FluentInterpreter(const std::string& script_path)
    : script_path_(script_path) {
    loadScript();
}

// ─── Load Script ────────────────────────────────────────────────────────────
void FluentInterpreter::loadScript() {
    lines_.clear();
    std::ifstream f(script_path_);
    if (!f.is_open()) {
        std::cerr << " Cannot open: " << script_path_ << "\n";
        return;
    }
    std::string line;
    while (std::getline(f, line)) {
        lines_.push_back(line);
    }
}

void FluentInterpreter::reload() {
    loadScript();
    run();
}

// ─── String Helpers ─────────────────────────────────────────────────────────
std::string FluentInterpreter::trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

bool FluentInterpreter::startsWith(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() && s.substr(0, prefix.size()) == prefix;
}

std::string FluentInterpreter::stripQuotes(const std::string& s) {
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
        return s.substr(1, s.size() - 2);
    return s;
}

std::vector<std::string> FluentInterpreter::splitCommaArgs(const std::string& s) {
    std::vector<std::string> result;
    std::string current;
    bool inQuote = false;
    for (char c : s) {
        if (c == '"') inQuote = !inQuote;
        if (c == ',' && !inQuote) {
            result.push_back(trim(current));
            current.clear();
        } else {
            current += c;
        }
    }
    if (!current.empty()) result.push_back(trim(current));
    return result;
}

// ─── Tokenizer ──────────────────────────────────────────────────────────────
std::vector<std::string> FluentInterpreter::tokenize(const std::string& line) {
    std::vector<std::string> tokens;
    std::string cur;
    bool inQ = false;
    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (c == '"') {
            if (inQ) {
                cur += c;
                tokens.push_back(cur);
                cur.clear();
                inQ = false;
            } else {
                if (!cur.empty()) { tokens.push_back(cur); cur.clear(); }
                cur += c;
                inQ = true;
            }
        } else if (!inQ && (c == ' ' || c == '\t')) {
            if (!cur.empty()) { tokens.push_back(cur); cur.clear(); }
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) tokens.push_back(cur);
    return tokens;
}

// ─── Variable Access ────────────────────────────────────────────────────────
FluentValue& FluentInterpreter::getOrCreateVar(const std::string& name) {
    std::lock_guard<std::mutex> lock(vars_mutex_);
    if (globals_.count(name)) return globals_[name];
    return vars_[name];
}

FluentValue FluentInterpreter::getVar(const std::string& name) {
    std::lock_guard<std::mutex> lock(vars_mutex_);
    if (globals_.count(name)) return globals_[name];
    if (vars_.count(name)) return vars_[name];
    // hardware builtins
    if (name == "hardware_fluent")    return FluentValue(getHardwareInfo("all"));
    if (name == "hardwaregpu_fluent") return FluentValue(getHardwareInfo("gpu"));
    if (name == "hardwarecpu_fluent") return FluentValue(getHardwareInfo("cpu"));
    if (name == "hardwareram_fluent") return FluentValue(getHardwareInfo("ram"));
    if (name == "hardwarestorage_fluent") return FluentValue(getHardwareInfo("storage"));
    if (name == "hardwarevram_fluent") return FluentValue(getHardwareInfo("vram"));
    if (name == "hardwareos_fluent")  return FluentValue(getHardwareInfo("os"));
    return FluentValue{};
}

void FluentInterpreter::setVar(const std::string& name, FluentValue val) {
    std::lock_guard<std::mutex> lock(vars_mutex_);
    if (globals_.count(name)) { globals_[name] = val; return; }
    vars_[name] = val;
}

// ─── Value Resolution ────────────────────────────────────────────────────────
FluentValue FluentInterpreter::resolveValue(const std::string& token) {
    if (token.empty()) return FluentValue{};
    // Quoted string
    if (token.front() == '"' && token.back() == '"')
        return FluentValue(token.substr(1, token.size()-2));
    // Boolean
    if (token == "true")  return FluentValue(true);
    if (token == "false") return FluentValue(false);
    // Number
    try {
        size_t pos;
        double d = std::stod(token, &pos);
        if (pos == token.size()) return FluentValue(d);
    } catch (...) {}
    // Variable
    return getVar(token);
}

// ─── Duration Parsing ────────────────────────────────────────────────────────
long long FluentInterpreter::parseDurationMs(const std::vector<std::string>& tok, size_t i) {
    if (i >= tok.size()) return 0;
    double amount = 0;
    try { amount = std::stod(tok[i]); } catch (...) {}
    std::string unit = (i+1 < tok.size()) ? tok[i+1] : "seconds";
    if (unit == "seconds" || unit == "second") return (long long)(amount * 1000);
    if (unit == "minutes" || unit == "minute") return (long long)(amount * 60000);
    if (unit == "hours"   || unit == "hour")   return (long long)(amount * 3600000LL);
    if (unit == "days"    || unit == "day")    return (long long)(amount * 86400000LL);
    if (unit == "weeks"   || unit == "week")   return (long long)(amount * 604800000LL);
    if (unit == "months"  || unit == "month")  return (long long)(amount * 2592000000LL);
    if (unit == "years"   || unit == "year")   return (long long)(amount * 31536000000LL);
    return (long long)(amount * 1000);
}

// ─── Math Helpers ────────────────────────────────────────────────────────────
bool FluentInterpreter::isPrime(long long n) {
    if (n < 2) return false;
    if (n == 2) return true;
    if (n % 2 == 0) return false;
    for (long long i = 3; i * i <= n; i += 2)
        if (n % i == 0) return false;
    return true;
}
bool FluentInterpreter::isEven(double n) { return (long long)n % 2 == 0; }
bool FluentInterpreter::isOdd(double n)  { return (long long)n % 2 != 0; }

// ─── Hardware Info ────────────────────────────────────────────────────────────
std::string FluentInterpreter::getHardwareInfo(const std::string& key) {
    auto run_cmd = [](const std::string& cmd) -> std::string {
        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) return "unknown";
        char buf[256];
        std::string result;
        while (fgets(buf, sizeof(buf), pipe)) result += buf;
        pclose(pipe);
        // trim
        while (!result.empty() && (result.back()=='\n'||result.back()=='\r'||result.back()==' '))
            result.pop_back();
        return result.empty() ? "unknown" : result;
    };

#ifdef _WIN32
    if (key == "os") return run_cmd("ver");
    if (key == "cpu") return run_cmd("wmic cpu get name /value 2>nul | findstr Name");
    if (key == "ram") return run_cmd("wmic OS get TotalVisibleMemorySize /value 2>nul | findstr Total");
    if (key == "storage") return run_cmd("wmic logicaldisk get size /value 2>nul");
    if (key == "gpu" || key == "vram") return run_cmd("wmic path win32_VideoController get Name /value 2>nul | findstr Name");
    if (key == "all") return "OS: " + getHardwareInfo("os") + " | CPU: " + getHardwareInfo("cpu");
#else
    if (key == "os") return run_cmd("uname -srm");
    if (key == "cpu") return run_cmd("grep -m1 'model name' /proc/cpuinfo 2>/dev/null | cut -d: -f2 | xargs");
    if (key == "ram") return run_cmd("free -h 2>/dev/null | awk '/^Mem:/{print $2}'");
    if (key == "storage") return run_cmd("df -h / 2>/dev/null | awk 'NR==2{print $2}'");
    if (key == "gpu") return run_cmd("lspci 2>/dev/null | grep -i 'vga\\|3d\\|display' | head -1 | cut -d: -f3 | xargs");
    if (key == "vram") return run_cmd("lspci -v 2>/dev/null | grep -i memory | grep -i 'prefetchable\\|non-prefetchable' | head -1 | awk '{print $NF}'");
    if (key == "all") return "OS: " + getHardwareInfo("os") + " | CPU: " + getHardwareInfo("cpu") + " | RAM: " + getHardwareInfo("ram");
#endif
    return "unknown";
}

// ─── Find Block (done) ───────────────────────────────────────────────────────
std::pair<size_t,size_t> FluentInterpreter::findBlock(size_t start) {
    // Find the matching 'done' for a block starting at 'start'
    // Returns {body_start, done_idx}
    int depth = 1;
    for (size_t i = start + 1; i < lines_.size(); ++i) {
        auto tok = tokenize(trim(lines_[i]));
        if (tok.empty()) continue;
        // anything that opens a nested block
        if (tok[0] == "if" || tok[0] == "loop" || tok[0] == "parallel" ||
            tok[0] == "when" || tok[0] == "check" || tok[0] == "repeat" ||
            tok[0] == "ask")
            depth++;
        if (tok[0] == "done") {
            depth--;
            if (depth == 0) return {start + 1, i};
        }
    }
    return {start + 1, lines_.size() - 1};
}

// ─── Network Check ──────────────────────────────────────────────────────────
static bool checkInternet() {
#ifdef _WIN32
    return system("ping -n 1 8.8.8.8 >nul 2>&1") == 0;
#else
    return system("ping -c1 -W1 8.8.8.8 >/dev/null 2>&1") == 0;
#endif
}
