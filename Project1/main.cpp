#include "CommandHandler.h"
#include "Scheduler.h"
#include "Screen.h"

#include <iostream>
#include <map>

int main() {
    std::map<std::string, Screen> screens;
    Scheduler* scheduler = nullptr;

    CommandHandler::initialize(screens, scheduler);
    CommandHandler::handle();

    if (scheduler) {
        scheduler->stop();  // <-- Gracefully stop threads
        delete scheduler;
    }
    return 0;
}
