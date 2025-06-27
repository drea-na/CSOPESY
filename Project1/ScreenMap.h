#ifndef SCREENMAP_H
#define SCREENMAP_H

#include <map>
#include <string>
#include "Screen.h"

class ScreenMap {
private:
    static std::map<std::string, Screen> screens;

public:
    static std::map<std::string, Screen>& getScreens();
    static void addScreen(const std::string& name, const Screen& screen);  // declaration
    static void removeScreen(const std::string& name);
    static bool hasScreen(const std::string& name);
};

#endif // SCREENMAP_H
