#include "fluent.hpp"
#include <fstream>
#include <sstream>
#include <iostream>

// ─── Simple JSON helpers (no external deps) ──────────────────────────────────

static std::string valueToJson(const FluentValue& v, int indent = 0) {
    std::string pad(indent * 2, ' ');
    if (v.isNull())   return "null";
    if (v.isBool())   return v.asBool() ? "true" : "false";
    if (v.isNumber()) {
        double n = v.asNumber();
        if (n == (long long)n) return std::to_string((long long)n);
        std::ostringstream oss; oss << n; return oss.str();
    }
    if (v.isObfuscated()) {
        // escape quotes
        std::string s = v.asObfuscated().mask;
        std::string out = "\"";
        for (char c : s) { if (c=='"') out += "\\\""; else out += c; }
        return out + "\"";
    }
    if (v.isText()) {
        std::string out = "\"";
        for (char c : v.asText()) {
            if (c == '"')  out += "\\\"";
            else if (c == '\\') out += "\\\\";
            else if (c == '\n') out += "\\n";
            else if (c == '\r') out += "\\r";
            else if (c == '\t') out += "\\t";
            else out += c;
        }
        return out + "\"";
    }
    if (v.isTable()) {
        std::string out = "[\n";
        const auto& items = v.asTable().items;
        for (size_t i = 0; i < items.size(); i++) {
            out += pad + "  " + valueToJson(items[i], indent+1);
            if (i+1 < items.size()) out += ",";
            out += "\n";
        }
        return out + pad + "]";
    }
    return "null";
}

static std::string jsonToString(const std::string& json) {
    // Strip surrounding quotes and unescape
    std::string s = json;
    // trim whitespace
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a != std::string::npos) s = s.substr(a);
    size_t b = s.find_last_not_of(" \t\r\n");
    if (b != std::string::npos) s = s.substr(0, b+1);
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
        s = s.substr(1, s.size()-2);
        std::string out;
        for (size_t i = 0; i < s.size(); i++) {
            if (s[i] == '\\' && i+1 < s.size()) {
                char n = s[++i];
                if (n == '"')  out += '"';
                else if (n == '\\') out += '\\';
                else if (n == 'n')  out += '\n';
                else if (n == 'r')  out += '\r';
                else if (n == 't')  out += '\t';
                else { out += '\\'; out += n; }
            } else out += s[i];
        }
        return out;
    }
    return s;
}

// Parse a JSON array into a FluentTable (flat, one level)
static FluentValue parseJsonValue(const std::string& json) {
    std::string s = json;
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a != std::string::npos) s = s.substr(a);
    size_t b = s.find_last_not_of(" \t\r\n");
    if (b != std::string::npos) s = s.substr(0, b+1);

    if (s == "null")  return FluentValue{};
    if (s == "true")  return FluentValue(true);
    if (s == "false") return FluentValue(false);
    if (!s.empty() && s.front() == '"') return FluentValue(jsonToString(s));
    if (!s.empty() && s.front() == '[') {
        // parse array
        FluentTable tbl;
        std::string inner = s.substr(1, s.size()-2);
        // split by commas (naive, works for flat arrays)
        std::string cur;
        int depth = 0;
        bool inStr = false;
        for (char c : inner) {
            if (c == '"' && (cur.empty() || cur.back() != '\\')) inStr = !inStr;
            if (!inStr && (c == '[' || c == '{')) depth++;
            if (!inStr && (c == ']' || c == '}')) depth--;
            if (!inStr && c == ',' && depth == 0) {
                tbl.items.push_back(parseJsonValue(cur));
                cur.clear();
            } else cur += c;
        }
        if (!cur.empty()) {
            size_t x = cur.find_first_not_of(" \t\r\n");
            if (x != std::string::npos) tbl.items.push_back(parseJsonValue(cur));
        }
        return FluentValue(tbl);
    }
    // Try number
    try {
        size_t pos;
        double d = std::stod(s, &pos);
        if (pos == s.size()) return FluentValue(d);
    } catch (...) {}
    // Treat as plain text
    return FluentValue(s);
}

// ─── WRITE ───────────────────────────────────────────────────────────────────
// write "Hello" to file "notes.txt"
// write variable1 to file "notes.txt"
// write json variable1 to file "data.json"
size_t FluentInterpreter::handleWrite(const std::vector<std::string>& tok, size_t idx) {
    if (tok.size() < 4) return idx + 1;

    bool is_json = (tok[1] == "json");
    size_t val_idx = is_json ? 2 : 1;

    // find "to file"
    std::string filename;
    size_t fi = val_idx;
    for (; fi < tok.size(); fi++)
        if (tok[fi] == "to" && fi+2 < tok.size() && tok[fi+1] == "file") {
            filename = stripQuotes(tok[fi+2]);
            break;
        }
    if (filename.empty()) return idx + 1;

    // collect value tokens between val_idx and "to"
    std::string valStr;
    for (size_t j = val_idx; j < fi; j++) { if (j > val_idx) valStr += " "; valStr += tok[j]; }
    FluentValue val = resolveValue(trim(valStr));

    std::ofstream f(filename, std::ios::trunc);
    if (!f.is_open()) { std::cerr << "Error: Cannot write to " << filename << "\n"; return idx+1; }

    if (is_json) {
        f << valueToJson(val, 0) << "\n";
    } else {
        f << val.toString() << "\n";
    }
    return idx + 1;
}

// ─── APPEND ──────────────────────────────────────────────────────────────────
// append "Hello" to file "notes.txt"
// append variable1 to file "notes.txt"
size_t FluentInterpreter::handleAppend(const std::vector<std::string>& tok, size_t idx) {
    if (tok.size() < 4) return idx + 1;

    std::string filename;
    size_t fi = 1;
    for (; fi < tok.size(); fi++)
        if (tok[fi] == "to" && fi+2 < tok.size() && tok[fi+1] == "file") {
            filename = stripQuotes(tok[fi+2]);
            break;
        }
    if (filename.empty()) return idx + 1;

    std::string valStr;
    for (size_t j = 1; j < fi; j++) { if (j > 1) valStr += " "; valStr += tok[j]; }
    FluentValue val = resolveValue(trim(valStr));

    std::ofstream f(filename, std::ios::app);
    if (!f.is_open()) { std::cerr << "Error: Cannot append to " << filename << "\n"; return idx+1; }
    f << val.toString() << "\n";
    return idx + 1;
}

// ─── READ ────────────────────────────────────────────────────────────────────
// read file "notes.txt" into content
// read json file "data.json" into content
size_t FluentInterpreter::handleRead(const std::vector<std::string>& tok, size_t idx) {
    if (tok.size() < 4) return idx + 1;

    bool is_json = (tok[1] == "json");
    size_t file_kw = is_json ? 2 : 1; // index of "file"

    if (tok[file_kw] != "file" || file_kw+1 >= tok.size()) return idx + 1;

    std::string filename = stripQuotes(tok[file_kw+1]);

    // find "into varname"
    std::string varname;
    for (size_t j = file_kw+2; j < tok.size(); j++)
        if (tok[j] == "into" && j+1 < tok.size()) { varname = tok[j+1]; break; }
    if (varname.empty()) return idx + 1;

    std::ifstream f(filename);
    if (!f.is_open()) { std::cerr << "Error: Cannot read " << filename << "\n"; return idx+1; }

    std::string raw((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    // trim trailing newline
    while (!raw.empty() && (raw.back()=='\n'||raw.back()=='\r')) raw.pop_back();

    if (is_json) {
        setVar(varname, parseJsonValue(raw));
    } else {
        setVar(varname, FluentValue(raw));
    }
    return idx + 1;
}
