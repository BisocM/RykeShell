#include "commands.h"
#include "input.h"
#include <iostream>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <sstream>

std::map<std::string, CommandFunction> commandHandlers;

void registerCommands() {
    commandHandlers["exit"] = cmdExit;
    commandHandlers["cd"] = cmdCd;
    commandHandlers["pwd"] = cmdPwd;
    commandHandlers["history"] = cmdHistory;
    commandHandlers["alias"] = cmdAlias;
    commandHandlers["theme"] = cmdTheme;
}

bool handleCommand(const Command& command) {
    if (command.args.empty()) {
        return false;
    }

    if (const std::string cmdName = command.args[0]; commandHandlers.contains(cmdName)) {
        commandHandlers[cmdName](command);
        return true;
    }
    return false;
}

void cmdExit(const Command& command) {
    exit(0);
}

void cmdCd(const Command& command) {
    const std::string dir = (command.args.size() > 1) ? command.args[1] : getenv("HOME");
    if (chdir(dir.c_str()) != 0) {
        std::cerr << "cd: " << strerror(errno) << '\n';
    }
}

void cmdPwd(const Command& command) {
    if (char cwd[1024]; getcwd(cwd, sizeof(cwd)) != nullptr) {
        std::cout << cwd << std::endl;
    } else {
        std::cerr << "pwd: " << strerror(errno) << '\n';
    }
}

void cmdHistory(const Command& command) {
    if (history.empty()) {
        std::cout << "No commands in history." << std::endl;
        return;
    }
    
    const std::vector historyItems(history.begin(), history.end());

    //Check if user made a selection
    if (const int selected = interactiveListSelection(historyItems, "Command History"); selected >= 0 && selected < static_cast<int>(historyItems.size())) {
        //Execute the selected command
        std::string input = historyItems[selected];

        //Expand variables
        input = expandVariables(input);

        //Expand aliases
        std::string expandedInput = input;
        std::istringstream iss(input);
        std::string firstToken;
        iss >> firstToken;
        if (aliases.contains(firstToken)) {
            expandedInput = aliases[firstToken] + input.substr(firstToken.length());
        }

        //Parse and execute commands
        if (const std::vector<Command> commands = parseCommandLine(expandedInput); !commands.empty()) {
            if (!handleCommand(commands[0])) {
                executeCommands(commands);
            }
        }
    }
}

void cmdAlias(const Command& command) {
    if (command.args.size() == 1) {
        //List all aliases
        for (const auto&[fst, snd] : aliases) {
            std::cout << "alias " << fst << "='" << snd << "'\n";
        }
    } else {
        //Set an alias
        for (size_t k = 1; k < command.args.size(); ++k) {
            std::string aliasStr = command.args[k];
            if (size_t eqPos = aliasStr.find('='); eqPos != std::string::npos) {
                std::string key = aliasStr.substr(0, eqPos);
                std::string value = aliasStr.substr(eqPos + 1);
                //Remove quotes
                if (!value.empty() && value.front() == '\'' && value.back() == '\'') {
                    value = value.substr(1, value.length() - 2);
                }
                aliases[key] = value;
            }
        }
    }
}

void cmdTheme(const Command& command) {
    if (command.args.size() > 1) {
        if (const std::string color = command.args[1]; color == "red") {
            promptColor = "\033[1;31m";
        } else if (color == "green") {
            promptColor = "\033[1;32m";
        } else if (color == "yellow") {
            promptColor = "\033[1;33m";
        } else if (color == "blue") {
            promptColor = "\033[1;34m";
        } else if (color == "magenta") {
            promptColor = "\033[1;35m";
        } else if (color == "cyan") {
            promptColor = "\033[1;36m";
        } else {
            std::cerr << "Unknown color: " << color << std::endl;
        }
    } else {
        std::cout << "Usage: theme [color]\n";
    }
}