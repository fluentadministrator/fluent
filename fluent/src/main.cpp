#include "fluent.hpp"
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <filesystem>
#include <atomic>
#include <csignal>

namespace fs = std::filesystem;

static std::atomic<bool> g_running{true};

void signal_handler(int) { g_running = false; }

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

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    auto last_write = fs::last_write_time(script_path);

    auto run_interpreter = [&]() {
        try {
            FluentInterpreter interp(script_path);
            interp.run();
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << "\n";
        }
    };

    std::thread interp_thread(run_interpreter);

    // Hot-reload watcher
    std::thread watcher([&]() {
        while (g_running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            try {
                auto cur_write = fs::last_write_time(script_path);
                if (cur_write != last_write) {
                    last_write = cur_write;
                    if (interp_thread.joinable()) interp_thread.join();
                    interp_thread = std::thread(run_interpreter);
                }
            } catch (...) {}
        }
    });

    if (interp_thread.joinable()) interp_thread.join();
    g_running = false;
    if (watcher.joinable()) watcher.join();

    return 0;
}
