#pragma once
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <functional>
#include <variant>
#include <memory>
#include <optional>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <random>
#include <regex>
#include <condition_variable>
#include <stdexcept>
#include <cmath>
#include <set>

namespace fs = std::filesystem;

// ─── Value Types ─────────────────────────────────────────────────────────────
struct FluentNull {};
struct FluentTable {
    std::vector<struct FluentValue> items;
};
struct FluentObfuscated {
    std::string original;
    std::string mask;
};

struct FluentValue {
    enum class Type { Null, Number, Text, Bool, Table, Obfuscated, Random };
    std::variant<FluentNull, double, std::string, bool, FluentTable, FluentObfuscated> data;
    bool is_random = false;
    bool is_global = false;
    bool is_private = false;
    std::string private_condition;

    FluentValue() : data(FluentNull{}) {}
    explicit FluentValue(double v) : data(v) {}
    explicit FluentValue(const std::string& v) : data(v) {}
    explicit FluentValue(bool v) : data(v) {}
    explicit FluentValue(FluentTable v) : data(v) {}
    explicit FluentValue(FluentObfuscated v) : data(v) {}

    bool isNull()       const { return std::holds_alternative<FluentNull>(data); }
    bool isNumber()     const { return std::holds_alternative<double>(data); }
    bool isText()       const { return std::holds_alternative<std::string>(data); }
    bool isBool()       const { return std::holds_alternative<bool>(data); }
    bool isTable()      const { return std::holds_alternative<FluentTable>(data); }
    bool isObfuscated() const { return std::holds_alternative<FluentObfuscated>(data); }

    double      asNumber()     const { return std::get<double>(data); }
    std::string asText()       const { return std::get<std::string>(data); }
    bool        asBool()       const { return std::get<bool>(data); }
    FluentTable& asTable()     { return std::get<FluentTable>(data); }
    const FluentTable& asTable() const { return std::get<FluentTable>(data); }
    FluentObfuscated& asObfuscated() { return std::get<FluentObfuscated>(data); }
    const FluentObfuscated& asObfuscated() const { return std::get<FluentObfuscated>(data); }

    std::string toString() const;
    std::string typeName() const;
};

// ─── Interpreter ─────────────────────────────────────────────────────────────
class FluentInterpreter {
public:
    explicit FluentInterpreter(const std::string& script_path);
    void run();
    void reload();

private:
    std::string script_path_;
    std::vector<std::string> lines_;
    std::unordered_map<std::string, FluentValue> vars_;
    std::unordered_map<std::string, FluentValue> globals_;
    std::unordered_map<std::string, std::vector<std::string>> databases_;
    std::unordered_map<std::string, std::vector<std::string>> modules_;
    std::unordered_map<std::string, std::function<void(std::vector<std::string>)>> watchers_;
    std::mutex vars_mutex_;
    std::atomic<bool> running_{true};

    // ── parsing helpers
    void loadScript();
    std::vector<std::string> tokenize(const std::string& line);
    std::string trim(const std::string& s);
    std::string stripQuotes(const std::string& s);
    bool startsWith(const std::string& s, const std::string& prefix);
    std::vector<std::string> splitCommaArgs(const std::string& s);

    // ── value resolution
    FluentValue resolveValue(const std::string& token);
    FluentValue resolveExpr(const std::vector<std::string>& tokens, size_t& i);
    std::string valueToString(const FluentValue& v);

    // ── execution
    size_t executeLine(size_t idx);
    size_t executeBlock(size_t start, size_t end);
    std::pair<size_t,size_t> findBlock(size_t start); // returns end of 'done'

    // ── feature handlers
    size_t handleLet(const std::vector<std::string>& tok, size_t idx);
    size_t handleIf(size_t idx);
    size_t handleLoop(size_t idx);
    size_t handleRepeat(size_t idx);
    size_t handleWait(const std::vector<std::string>& tok, size_t idx);
    size_t handleChange(const std::vector<std::string>& tok, size_t idx);
    size_t handleSay(const std::vector<std::string>& tok, size_t idx);
    size_t handleParagraph(const std::vector<std::string>& tok, size_t idx);
    size_t handleAsk(size_t idx);
    size_t handleParallel(size_t idx);
    size_t handleWhen(size_t idx);
    size_t handleKeep(const std::vector<std::string>& tok, size_t idx);
    size_t handleSchedule(const std::vector<std::string>& tok, size_t idx);
    size_t handleCreateFile(const std::vector<std::string>& tok, size_t idx);
    size_t handleOpen(const std::vector<std::string>& tok, size_t idx);
    size_t handleKill(const std::vector<std::string>& tok, size_t idx);
    size_t handleSystem(const std::vector<std::string>& tok, size_t idx);
    size_t handleImport(const std::vector<std::string>& tok, size_t idx);
    size_t handleMake(const std::vector<std::string>& tok, size_t idx);
    size_t handleAdd(const std::vector<std::string>& tok, size_t idx);
    size_t handleRemove(const std::vector<std::string>& tok, size_t idx);
    size_t handleSubtract(const std::vector<std::string>& tok, size_t idx);
    size_t handleDivide(const std::vector<std::string>& tok, size_t idx);
    size_t handleMultiply(const std::vector<std::string>& tok, size_t idx);
    size_t handleObfuscate(const std::vector<std::string>& tok, size_t idx);
    size_t handleSet(const std::vector<std::string>& tok, size_t idx);
    size_t handleCheck(size_t idx);
    size_t handleList(const std::vector<std::string>& tok, size_t idx);
    size_t handleGUI(const std::vector<std::string>& tok, size_t idx);
    size_t handleLog(const std::vector<std::string>& tok, size_t idx);

    // ── condition evaluation
    bool evalCondition(const std::vector<std::string>& tok, size_t start, size_t end);
    bool evalVarCondition(const std::string& varname, const std::vector<std::string>& cond_tokens);

    // ── hardware
    std::string getHardwareInfo(const std::string& key);

    // ── math helpers
    bool isPrime(long long n);
    bool isEven(double n);
    bool isOdd(double n);

    // ── duration parsing
    long long parseDurationMs(const std::vector<std::string>& tok, size_t i);

    // ── GUI state
    struct GUIElement {
        std::string type; // button, text, bar
        std::string gui_id;
        std::string label;
        int x=0, y=0, z=0;
        std::string fg_color, bg_color;
    };
    std::vector<GUIElement> gui_elements_;
    void renderGUI(const std::string& gui_id);

    FluentValue& getOrCreateVar(const std::string& name);
    FluentValue getVar(const std::string& name);
    void setVar(const std::string& name, FluentValue val);
};
