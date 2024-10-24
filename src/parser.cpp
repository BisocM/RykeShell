#include "ryke_shell.h"
#include <sstream>

std::vector<Command> parseCommandLine(const std::string& input) {
    std::vector<Command> commands;
    Command cmd;
    std::istringstream iss(input);
    std::string token;
    while (iss >> token) {
        if (token == "|") {
            commands.push_back(cmd);
            cmd = Command();
        } else if (token == "<") {
            if (iss >> token) {
                cmd.inputFile = token;
            }
        } else if (token == ">") {
            if (iss >> token) {
                cmd.outputFile = token;
            }
        } else if (token == ">>") {
            if (iss >> token) {
                cmd.appendFile = token;
            }
        } else if (token == "&") {
            cmd.background = true;
        } else {
            cmd.args.push_back(token);
        }
    }
    if (!cmd.args.empty() || !cmd.inputFile.empty() || !cmd.outputFile.empty() || !cmd.appendFile.empty() || cmd.background) {
        commands.push_back(cmd);
    }
    return commands;
}