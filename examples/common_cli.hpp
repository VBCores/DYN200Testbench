#pragma once

#include <chrono>
#include <csignal>
#include <iostream>
#include <string>
#include <vector>

inline bool g_stop = false;

inline void installSignalHandlers() {
    std::signal(SIGINT, [](int) { g_stop = true; });
    std::signal(SIGTERM, [](int) { g_stop = true; });
}

inline std::string optionValue(int argc, char** argv, const std::string& name, const std::string& fallback) {
    for (int i = 1; i + 1 < argc; ++i) {
        if (argv[i] == name) {
            return argv[i + 1];
        }
    }
    return fallback;
}

inline int optionInt(int argc, char** argv, const std::string& name, int fallback) {
    return std::stoi(optionValue(argc, argv, name, std::to_string(fallback)));
}

inline double optionDouble(int argc, char** argv, const std::string& name, double fallback) {
    return std::stod(optionValue(argc, argv, name, std::to_string(fallback)));
}

inline bool hasFlag(int argc, char** argv, const std::string& name) {
    for (int i = 1; i < argc; ++i) {
        if (argv[i] == name) {
            return true;
        }
    }
    return false;
}

inline bool wantsHelp(int argc, char** argv) {
    return hasFlag(argc, argv, "--help") || hasFlag(argc, argv, "-h");
}

inline std::vector<std::string> positional(int argc, char** argv) {
    std::vector<std::string> out;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a.rfind("--", 0) == 0) {
            ++i;
            continue;
        }
        out.push_back(a);
    }
    return out;
}
