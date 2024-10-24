#include "ryke_shell.h"
#include <iostream>
#include <sstream>

std::deque<std::string> history;
const size_t HISTORY_LIMIT = 100;
std::map<std::string, std::string> aliases;
std::string promptColor = "\033[1;32m";

int main() {
    displaySplashArt();
    setupSignalHandlers();
    registerCommands();

    while (true) {
        //Display prompt
        std::cout << getPrompt();
        std::cout.flush();

        //Read input
        std::string input = readInputLine();
        if (input.empty()) {
            continue;
        }

        //Expand variables
        input = expandVariables(input);

        //Store command in history
        history.push_back(input);
        if (history.size() > HISTORY_LIMIT) {
            history.pop_front();
        }

        //Expand aliases
        std::string expandedInput = input;
        std::istringstream iss(input);
        std::string firstToken;
        iss >> firstToken;
        if (aliases.count(firstToken)) {
            expandedInput = aliases[firstToken] + input.substr(firstToken.length());
        }

        //Parse commands
        std::vector<Command> commands = parseCommandLine(expandedInput);
        if (commands.empty()) {
            continue;
        }

        //Handle built-in commands
        if (handleCommand(commands[0])) {
            continue; //Command was handled
        }

        //Execute external commands
        executeCommands(commands);
    }
    return 0;
}