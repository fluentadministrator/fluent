#include "fluent.hpp"

// ─── Main Run ───────────────────────────────────────────────────────────────
void FluentInterpreter::run() {
    running_ = true;
    size_t i = 0;
    while (i < lines_.size() && running_) {
        i = executeLine(i);
    }
}

// ─── Execute Single Line ─────────────────────────────────────────────────────
size_t FluentInterpreter::executeLine(size_t idx) {
    if (idx >= lines_.size()) return lines_.size();

    std::string raw = trim(lines_[idx]);

    // Skip empty lines and section headers [...]
    if (raw.empty()) return idx + 1;
    if (raw.front() == '[' && raw.back() == ']') return idx + 1;

    // Comments  :...:
    if (raw.front() == ':' && raw.back() == ':') return idx + 1;

    auto tok = tokenize(raw);
    if (tok.empty()) return idx + 1;

    // ── Dispatch ────────────────────────────────────────────────────────────
    const std::string& cmd = tok[0];

    if (cmd == "let")       return handleLet(tok, idx);
    if (cmd == "say")       return handleSay(tok, idx);
    if (cmd == "paragraph") return handleParagraph(tok, idx);
    if (cmd == "if")        return handleIf(idx);
    if (cmd == "loop")      return handleLoop(idx);
    if (cmd == "repeat")    return handleRepeat(idx);
    if (cmd == "wait")      return handleWait(tok, idx);
    if (cmd == "change")    return handleChange(tok, idx);
    if (cmd == "add")       return handleAdd(tok, idx);
    if (cmd == "remove")    return handleRemove(tok, idx);
    if (cmd == "subtract")  return handleSubtract(tok, idx);
    if (cmd == "divide")    return handleDivide(tok, idx);
    if (cmd == "multiply")  return handleMultiply(tok, idx);
    if (cmd == "obfuscate") return handleObfuscate(tok, idx);
    if (cmd == "set")       return handleSet(tok, idx);
    if (cmd == "ask")       return handleAsk(idx);
    if (cmd == "parallel")  return handleParallel(idx);
    if (cmd == "when")      return handleWhen(idx);
    if (cmd == "keep")      return handleKeep(tok, idx);
    if (cmd == "schedule")  return handleSchedule(tok, idx);
    if (cmd == "create")    return handleCreateFile(tok, idx);
    if (cmd == "open")      return handleOpen(tok, idx);
    if (cmd == "kill")      return handleKill(tok, idx);
    if (cmd == "system")    return handleSystem(tok, idx);
    if (cmd == "import")    return handleImport(tok, idx);
    if (cmd == "make")      return handleMake(tok, idx);
    if (cmd == "check")     return handleCheck(idx);
    if (cmd == "list")      return handleList(tok, idx);
    if (cmd == "put")       return handleGUI(tok, idx);
    if (cmd == "log")       return handleLog(tok, idx);
    if (cmd == "done")      return idx + 1; // stray done
    if (cmd == "otherwise") return idx + 1; // stray otherwise

    // Unknown
    std::cerr << "[FLUENT] Unknown command: " << raw << "\n";
    return idx + 1;
}

// ─── Execute Block Range ─────────────────────────────────────────────────────
size_t FluentInterpreter::executeBlock(size_t start, size_t end) {
    size_t i = start;
    while (i < end && running_) {
        i = executeLine(i);
    }
    return end;
}

// ─── valueToString ───────────────────────────────────────────────────────────
std::string FluentInterpreter::valueToString(const FluentValue& v) {
    return v.toString();
}
