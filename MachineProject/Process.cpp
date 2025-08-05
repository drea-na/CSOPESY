#include "Process.h"
#include <sstream>
#include <algorithm>

void Process::parseSingleInstruction(const std::string& instr) {
    std::stringstream ss(instr);
    std::string op;
    ss >> op;
    
    Instruction instruction;
    
    if (op == "DECLARE") {
        instruction.type = InstrType::DECLARE;
        std::string varName, value;
        ss >> varName >> value;
        instruction.args = {varName, value};
    } else if (op == "ADD") {
        instruction.type = InstrType::ADD;
        std::string var1, var2, var3;
        ss >> var1 >> var2 >> var3;
        instruction.args = {var1, var2, var3};
    } else if (op == "SUBTRACT") {
        instruction.type = InstrType::SUBTRACT;
        std::string var1, var2, var3;
        ss >> var1 >> var2 >> var3;
        instruction.args = {var1, var2, var3};
    } else if (op == "PRINT") {
        instruction.type = InstrType::PRINT;
        // For PRINT, capture everything after PRINT as the message
        std::string message;
        std::getline(ss, message);
        // Trim leading whitespace
        message.erase(0, message.find_first_not_of(" \t"));
        
        // Remove outer parentheses if present
        if (message.length() >= 2 && message[0] == '(' && message[message.length()-1] == ')') {
            message = message.substr(1, message.length() - 2);
        }
        
        // Remove outer quotes if present
        if (message.length() >= 2 && message[0] == '"' && message[message.length()-1] == '"') {
            message = message.substr(1, message.length() - 2);
        }
        
        instruction.args = {message};
    } else if (op == "SLEEP") {
        instruction.type = InstrType::SLEEP;
        std::string ticks;
        ss >> ticks;
        instruction.args = {ticks};
    } else if (op == "READ") {
        instruction.type = InstrType::READ;
        std::string varName, address;
        ss >> varName >> address;
        instruction.args = {varName, address};
    } else if (op == "WRITE") {
        instruction.type = InstrType::WRITE;
        std::string address, value;
        ss >> address >> value;
        instruction.args = {address, value};
    }
    
    instructions.push_back(instruction);
} 