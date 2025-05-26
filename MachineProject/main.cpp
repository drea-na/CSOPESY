#include <iostream>
#include <string>
#include <cstdlib>
#include <map>
#include "Console.h"

using namespace std;

const string G = "\033[32m"; //G for green
const string Y = "\033[33m"; //Y for yellow
const string C = "\033[36m"; //C for cyan
const string Default = "\033[0m";

// Declare the screenMap variable
map<string, Console> screenMap;

void printHeader() {
    cout << C << " _______  _______  _______  _______  _______  _______  __   __ " << Default << endl;
    cout << C << "|       ||       ||       ||       ||       ||       ||  | |  |" << Default << endl;
    cout << C << "|       ||  _____||   _   ||    _  ||    ___||  _____||  |_|  |" << Default << endl;
    cout << C << "|       || |_____ |  | |  ||   |_| ||   |___ | |_____ |       |" << Default << endl;
    cout << C << "|      _||_____  ||  |_|  ||    ___||    ___||_____  ||_     _|" << Default << endl;
    cout << C << "|     |_  _____| ||       ||   |    |   |___  _____| |  |   |  " << Default << endl;
    cout << C << "|_______||_______||_______||___|    |_______||_______|  |___|  " << Default << endl;

    cout << G << "\nHello, Welcome to CSOPESY commandline!" << Default << endl;
    cout << Y << "Type 'exit' to quit, 'clear' to clear the screen" << Default << endl;
    cout << "Enter a command: ";
}

void printEnter() {
    cout << "\nEnter a command: ";
}

int main() {
    bool running = true;
    string str;

    printHeader();

    while (running) {
        getline(cin, str);

        if (str == "exit") {
            cout << "Shutting down... bye bye" << endl;
            running = false;
        }
        else if (str == "clear") {
            system("cls");
            printHeader();
        }
        else if (str == "initialize" || str == "screen" || str == "scheduler-test" ||
            str == "scheduler-stop" || str == "report-util") {
            cout << str << " command recognized. Doing something." << endl;
            printEnter();
        }
        else if (str.substr(0, 10) == "screen -s ") {
            string name = str.substr(10);
            if (name.empty()) {
                cout << "Error: screen name cannot be empty." << endl;
            }
            else {
                if (screenMap.count(name)) {
                    cout << "Screen '" << name << "' already exists." << endl;
                }
                else {
                    screenMap[name] = Console(name);
                    cout << "Created screen: " << name << endl;
                }
            }
            printEnter();
        }
        else if (str.substr(0, 10) == "screen -r ") {
            string name = str.substr(10);
            if (name.empty()) {
                cout << "Error: screen name cannot be empty." << endl;
            }
            else if (screenMap.count(name)) {
                bool inScreen = true;
                while (inScreen) {
                    system("cls");
                    screenMap[name].displayScreen();

                    string input;
                    getline(cin, input);
                    if (input == "exit") {
                        inScreen = false;
                        system("cls");
                        printHeader();
                    }
                }
            }
            else {
                cout << "No screen named '" << name << "' found." << endl;
                printEnter();
            }
        }
        else {
            cout << "Unknown command." << endl;
            printEnter();
        }
    }

    return 0;
}
