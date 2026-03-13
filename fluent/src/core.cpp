#include "fluent.hpp"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <thread>
#include <cstdio>

// ─── Constructor ─────────────────────────────────────────────────────────────
FluentInterpreter::FluentInterpreter(const std::string& script_path)
    : script_path_(script_path) {
    loadScript();
}

void FluentInterpreter::loadScript() {
    lines_.clear();
    std::ifstream f(script_path_);
    if (!f.is_open()) { std::cerr << "Error: Cannot open: " << script_path_ << "\n"; return; }
    std::string line;
    while (std::getline(f, line)) lines_.push_back(line);
}

void FluentInterpreter::reload() { loadScript(); run(); }

// ─── String Helpers ──────────────────────────────────────────────────────────
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
        if (c == ',' && !inQuote) { result.push_back(trim(current)); current.clear(); }
        else current += c;
    }
    if (!current.empty()) result.push_back(trim(current));
    return result;
}

// ─── Tokenizer ───────────────────────────────────────────────────────────────
std::vector<std::string> FluentInterpreter::tokenize(const std::string& line) {
    std::vector<std::string> tokens;
    std::string cur;
    bool inQ = false;
    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (c == '"') {
            if (inQ) { cur += c; tokens.push_back(cur); cur.clear(); inQ = false; }
            else { if (!cur.empty()) { tokens.push_back(cur); cur.clear(); } cur += c; inQ = true; }
        } else if (!inQ && (c == ' ' || c == '\t')) {
            if (!cur.empty()) { tokens.push_back(cur); cur.clear(); }
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) tokens.push_back(cur);
    return tokens;
}

// ─── Variable Access ─────────────────────────────────────────────────────────
FluentValue& FluentInterpreter::getOrCreateVar(const std::string& name) {
    if (globals_.count(name)) return globals_[name];
    return vars_[name];
}

FluentValue FluentInterpreter::getVar(const std::string& name) {
    // Hardware builtins - checked first, no lock needed
    if (name == "hardware_fluent")        return FluentValue(getHardwareInfo("all"));
    if (name == "hardwaregpu_fluent")     return FluentValue(getHardwareInfo("gpu"));
    if (name == "hardwarecpu_fluent")     return FluentValue(getHardwareInfo("cpu"));
    if (name == "hardwareram_fluent")     return FluentValue(getHardwareInfo("ram"));
    if (name == "hardwarestorage_fluent") return FluentValue(getHardwareInfo("storage"));
    if (name == "hardwarevram_fluent")    return FluentValue(getHardwareInfo("vram"));
    if (name == "hardwareos_fluent")      return FluentValue(getHardwareInfo("os"));

    std::lock_guard<std::mutex> lock(vars_mutex_);
    if (globals_.count(name)) return globals_[name];
    if (vars_.count(name))    return vars_[name];
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
    if (token.front() == '"' && token.back() == '"')
        return FluentValue(token.substr(1, token.size() - 2));
    if (token == "true")  return FluentValue(true);
    if (token == "false") return FluentValue(false);
    try {
        size_t pos;
        double d = std::stod(token, &pos);
        if (pos == token.size()) return FluentValue(d);
    } catch (...) {}
    return getVar(token);
}

// ─── Duration Parsing ────────────────────────────────────────────────────────
long long FluentInterpreter::parseDurationMs(const std::vector<std::string>& tok, size_t i) {
    if (i >= tok.size()) return 0;
    double amount = 0;
    try { amount = std::stod(tok[i]); } catch (...) {}
    std::string unit = (i + 1 < tok.size()) ? tok[i + 1] : "seconds";
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
    if (n < 2) return false; if (n == 2) return true; if (n % 2 == 0) return false;
    for (long long i = 3; i * i <= n; i += 2) if (n % i == 0) return false;
    return true;
}
bool FluentInterpreter::isEven(double n) { return (long long)n % 2 == 0; }
bool FluentInterpreter::isOdd(double n)  { return (long long)n % 2 != 0; }

// ─── Hardware Info (cached, fast) ────────────────────────────────────────────
std::string FluentInterpreter::getHardwareInfo(const std::string& key) {
    // Cache so we only run commands once per interpreter instance
    static std::unordered_map<std::string, std::string> cache;
    auto it = cache.find(key);
    if (it != cache.end()) return it->second;

    auto run_cmd = [](const std::string& cmd) -> std::string {
#ifdef _WIN32
        FILE* pipe = _popen(cmd.c_str(), "r");
#else
        FILE* pipe = popen(cmd.c_str(), "r");
#endif
        if (!pipe) return "";
        char buf[512]; std::string result;
        while (fgets(buf, sizeof(buf), pipe)) result += buf;
#ifdef _WIN32
        _pclose(pipe);
#else
        pclose(pipe);
#endif
        // strip all leading/trailing whitespace and control chars
        size_t s = result.find_first_not_of(" \t\r\n");
        if (s == std::string::npos) return "";
        size_t e = result.find_last_not_of(" \t\r\n");
        return result.substr(s, e - s + 1);
    };

    std::string result;
#ifdef _WIN32
    // wmic outputs UTF-16 on some systems but /format:value gives clean key=value lines
    // Use cmd /c to avoid issues with special chars
    if (key == "os")      result = run_cmd("cmd /c \"wmic os get Caption /format:value 2>nul\"");
    if (key == "cpu")     result = run_cmd("cmd /c \"wmic cpu get Name /format:value 2>nul\"");
    if (key == "ram")     result = run_cmd("cmd /c \"wmic os get TotalVisibleMemorySize /format:value 2>nul\"");
    if (key == "storage") result = run_cmd("cmd /c \"wmic logicaldisk where DeviceID='C:' get Size /format:value 2>nul\"");
    if (key == "gpu")     result = run_cmd("cmd /c \"wmic path win32_VideoController get Name /format:value 2>nul\"");
    if (key == "vram")    result = run_cmd("cmd /c \"wmic path win32_VideoController get AdapterRAM /format:value 2>nul\"");
    if (key == "all")     { cache["all"] = getHardwareInfo("os") + " | " + getHardwareInfo("cpu"); return cache["all"]; }

    // wmic /format:value gives "Caption=Windows 11 Pro\r\n\r\n" - extract value after =
    auto eq = result.find('=');
    if (eq != std::string::npos) {
        result = result.substr(eq + 1);
        // trim again
        size_t s = result.find_first_not_of(" \t\r\n");
        if (s == std::string::npos) result = "";
        else { size_t e = result.find_last_not_of(" \t\r\n"); result = result.substr(s, e-s+1); }
    }
    // For RAM, convert KB to GB
    if (key == "ram" && !result.empty()) {
        try {
            long long kb = std::stoll(result);
            double gb = kb / 1024.0 / 1024.0;
            std::ostringstream oss; oss << std::fixed;
            oss.precision(1); oss << gb << " GB";
            result = oss.str();
        } catch (...) {}
    }
    // For VRAM, convert bytes to MB
    if (key == "vram" && !result.empty()) {
        try {
            long long b = std::stoll(result);
            result = std::to_string(b / 1024 / 1024) + " MB";
        } catch (...) {}
    }
#else
    if (key == "os")      result = run_cmd("uname -srm 2>/dev/null");
    if (key == "cpu")     result = run_cmd("grep -m1 'model name' /proc/cpuinfo 2>/dev/null | cut -d: -f2 | xargs");
    if (key == "ram")     result = run_cmd("free -h 2>/dev/null | awk '/^Mem:/{print $2}'");
    if (key == "storage") result = run_cmd("df -h / 2>/dev/null | awk 'NR==2{print $2}'");
    if (key == "gpu")     result = run_cmd("lspci 2>/dev/null | grep -i 'vga\\|3d\\|display' | head -1 | cut -d: -f3 | xargs");
    if (key == "vram")    result = run_cmd("lspci -v 2>/dev/null | grep -i 'prefetchable' | head -1 | awk '{print $NF}'");
    if (key == "all")     { cache["all"] = getHardwareInfo("os") + " | " + getHardwareInfo("cpu") + " | " + getHardwareInfo("ram"); return cache["all"]; }
#endif

    if (result.empty()) result = "unknown";
    cache[key] = result;
    return result;
}


// ─── Find Block ──────────────────────────────────────────────────────────────
std::pair<size_t,size_t> FluentInterpreter::findBlock(size_t start) {
    int depth = 1;
    for (size_t i = start + 1; i < lines_.size(); ++i) {
        auto tok = tokenize(trim(lines_[i]));
        if (tok.empty()) continue;
        if (tok[0]=="if"||tok[0]=="loop"||tok[0]=="parallel"||
            tok[0]=="when"||tok[0]=="check"||tok[0]=="repeat"||tok[0]=="ask") depth++;
        if (tok[0] == "done") { depth--; if (depth == 0) return {start + 1, i}; }
    }
    return {start + 1, lines_.size() > 0 ? lines_.size() - 1 : 0};
}
