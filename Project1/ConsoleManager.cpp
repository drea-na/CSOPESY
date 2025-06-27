#include "ConsoleManager.h"
#include <iostream>

using namespace std;
using namespace ConsoleColors;

void ConsoleManager::drawHeader() {
    cout << C << " _______  _______  _______  _______  _______  _______  __   __ " << Default << endl;
    cout << C << "|       ||       ||       ||       ||       ||       ||  | |  |" << Default << endl;
    cout << C << "|       ||  _____||   _   ||    _  ||    ___||  _____||  |_|  |" << Default << endl;
    cout << C << "|       || |_____ |  | |  ||   |_| ||   |___ | |_____ |       |" << Default << endl;
    cout << C << "|      _||_____  ||  |_|  ||    ___||    ___||_____  ||_     _|" << Default << endl;
    cout << C << "|     |_  _____| ||       ||   |    |   |___  _____| |  |   |  " << Default << endl;
    cout << C << "|_______||_______||_______||___|    |_______||_______|  |___|  " << Default << endl;

    cout << G << "\nHello, Welcome to CSOPESY commandline!" << Default << endl;
    cout << Y << "Type 'exit' to quit, 'clear' to clear the screen" << Default << endl;
}

void ConsoleManager::prompt() {
    cout << "\nroot:\\> ";
}
