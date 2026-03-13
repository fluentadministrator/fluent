#include "fluent.hpp"
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <filesystem>
#include <atomic>
#include <csignal>

namespace fs = std::filesystem;

static std::atomic<bool> g_reload{false};
static std::atomic<bool> g_quit{false};

void signal_handler(int) { g_quit = true; }

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: fluent <script.fluent>\n";
        return 1;
    }

    std::string script_path = argv[1];

    if (!fs::exists(script_path)) {
        std::cerr << "Error: File not found: " << script_path << "\n";
        return 1;
    }

    std::signal(SIGINT,  signal_handler);
    std::signal(SIGTERM, signal_handler);

    // Run script
    try {
        FluentInterpreter interp(script_path);
        interp.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    // Done - exit immediately
    return 0;
}
