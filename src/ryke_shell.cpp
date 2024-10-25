#include "ryke_shell.h"
#include "utils.h"
#include "input.h"
#include <iostream>
#include <sstream>
#include <unistd.h>

std::deque<std::string> history;
const size_t HISTORY_LIMIT = 100;
std::map<std::string, std::string> aliases;
std::string promptColor = "\033[1;32m";

termios origTermios;

int main() {
    if (tcgetattr(STDIN_FILENO, &origTermios) == -1) {
        perror("tcgetattr");
        return 1;
    }

    displaySplashArt();
    setupSignalHandlers();
    registerCommands();

    while (true) {
        std::cout << getPrompt();
        std::cout.flush();

        std::string input = readInputLine();
        if (input.empty()) {
            continue;
        }

        input = expandVariables(input);

        history.push_back(input);
        if (history.size() > HISTORY_LIMIT) {
            history.pop_front();
        }

        std::string expandedInput = input;
        std::istringstream iss(input);
        std::string firstToken;
        iss >> firstToken;
        if (aliases.contains(firstToken)) {
            expandedInput = aliases[firstToken] + input.substr(firstToken.length());
        }

        std::vector<Command> commands = parseCommandLine(expandedInput);
        if (commands.empty()) {
            continue;
        }

        if (handleCommand(commands[0])) {
            continue;
        }

        executeCommands(commands);
    }
}