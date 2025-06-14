#include <iostream>
#include <string>
#include <cstdlib>
#include <map>
#include <vector>
#include <mutex>
#include <thread>
#include <queue>
#include <fstream>
#include <chrono>
#include "Console.h"
#include "Scheduler.h"

using namespace std;

const string G = "\033[32m"; //G for green
const string Y = "\033[33m"; //Y for yellow
const string C = "\033[36m"; //C for cyan
const string Default = "\033[0m";

// Declare the screenMap variable
map<string, Console> screenMap;

queue<Console> readyQueue;
mutex queueMutex;
condition_variable cv;
bool schedulerRunning = true;

struct ProcessInfo {
    string name;
    string startTime;
    int coreID;
    int progress;
    int total;
    bool finished;
};

// global variables
vector<ProcessInfo> processList;
mutex processMutex;

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

void showProcessList() {
    lock_guard<mutex> lock(processMutex);

    cout << "\n-------------------------------" << endl;
    cout << "Running processes:\n";

    for (const auto& p : processList) {
        if (!p.finished) {
            cout << p.name << "\t(" << p.startTime << ")"
                << "  Core: " << p.coreID
                << "  " << p.progress << " / " << p.total << endl;
        }
    }

    cout << "\nFinished processes:\n";
    for (const auto& p : processList) {
        if (p.finished) {
            cout << p.name << "\t(" << p.startTime << ")"
                << "  Finished  " << p.total << " / " << p.total << endl;
        }
    }

    cout << "-------------------------------\n";
}

void cpuWorker(int coreID) {
    while (schedulerRunning) {
        Console currentProcess;

        {
            unique_lock<mutex> lock(queueMutex);
            cv.wait(lock, [] { return !readyQueue.empty() || !schedulerRunning; });

            if (!schedulerRunning && readyQueue.empty()) return;

            currentProcess = readyQueue.front();
            readyQueue.pop();
        }

        // Simulate executing 100 print commands
        for (int i = 1; i <= 100; ++i) {
            this_thread::sleep_for(chrono::milliseconds(10)); // simulate work

            // Log to file
            ofstream outFile(currentProcess.name + ".txt", ios::app);
            auto now = Console().getCurrentTimestamp();
            outFile << "(" << now << ") Core:" << coreID << " - Hello world from " << currentProcess.name << "!\n";
            outFile.close();

            // Update progress
            {
                lock_guard<mutex> lock(processMutex);
                for (auto& info : processList) {
                    if (info.name == currentProcess.name) {
                        info.coreID = coreID;
                        info.progress = i;
                        if (i == 100) info.finished = true;
                        break;
                    }
                }
            }
        }
    }
}


int main() {
    bool running = true;
    string str;

    printHeader();

    while (running) {
        getline(cin, str);

        if (str == "exit") {
            cout << "Shutting down... bye bye" << endl;
            schedulerRunning = false;
            cv.notify_all();
            running = false;
        }

        else if (str == "clear") {
            system("cls");
            printHeader();
        }
        else if (str == "initialize" || str == "screen" ||
            str == "scheduler-stop" || str == "report-util") {
            cout << str << " command recognized. Doing something." << endl;
            printEnter();
        }
        else if (str == "scheduler-test") {
            {
                lock_guard<mutex> lock(queueMutex);
                for (int i = 0; i < 10; ++i) {
                    string pname = "process0" + to_string(i + 1);
                    Console proc(pname);
                    readyQueue.push(proc);

                    ProcessInfo info;
                    info.name = pname;
                    info.startTime = proc.getCurrentTimestamp(); // make sure it's public
                    info.coreID = -1;
                    info.progress = 0;
                    info.total = 100;
                    info.finished = false;

                    lock_guard<mutex> lock2(processMutex);
                    processList.push_back(info);
                }
            }

            cv.notify_all();

            thread schedulerThread([] {
                vector<thread> cores;
                for (int i = 0; i < 4; ++i) {
                    cores.emplace_back(cpuWorker, i);
                }
                for (auto& t : cores) t.join();
                });

            schedulerThread.detach(); // runs independently

            cout << "Started scheduler with 10 processes.\n";
            printEnter();
        }
        else if (str == "screen -ls") {
            lock_guard<mutex> lock(processMutex);
            cout << "\n=== Process List ===" << endl;
            for (const auto& p : processList) {
                cout << "Process: " << p.name
                    << " | Core: " << (p.coreID == -1 ? "Waiting" : to_string(p.coreID))
                    << " | Progress: " << p.progress << "/" << p.total
                    << " | Status: " << (p.finished ? "Finished" : "Running") << endl;
            }
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
