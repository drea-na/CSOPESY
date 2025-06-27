#include "ScreenMap.h"

std::map<std::string, Screen> ScreenMap::screens;

std::map<std::string, Screen>& ScreenMap::getScreens() {
    return screens;
}

void ScreenMap::addScreen(const std::string& name, const Screen& screen) {
    screens[name] = screen;
}

void ScreenMap::removeScreen(const std::string& name) {
    screens.erase(name);
}

bool ScreenMap::hasScreen(const std::string& name) {
    return screens.find(name) != screens.end();
}
