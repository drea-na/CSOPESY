#pragma once
#ifndef CONSOLEMANAGER_H
#define CONSOLEMANAGER_H

#include <string>

namespace ConsoleColors {
    const std::string G = "\033[32m";
    const std::string Y = "\033[33m";
    const std::string C = "\033[36m";
    const std::string Default = "\033[0m";
}

class ConsoleManager {
public:
    static void drawHeader();
    static void prompt();
};

#endif
