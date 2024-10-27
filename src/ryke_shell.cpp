#include "ryke_shell.h"
#include "utils.h"
#include "input.h"
#include "job_control.h"
#include <iostream>
#include <sstream>
#include <unistd.h>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <glob.h>

std::deque<std::string> history;
const size_t HISTORY_LIMIT = 100;
std::map<std::string, std::string> aliases;
std::string promptColor = "\033[1;32m";

//Job control variables
pid_t shellPGID;
termios shellTermios;
int shellTerminal;
bool shellIsInteractive = false;


int main() {
    if (tcgetattr(STDIN_FILENO, &shellTermios) == -1) {
        perror("tcgetattr");
        return 1;
    }

    initShell();
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

void initShell() {
    shellTerminal = STDIN_FILENO;
    shellIsInteractive = isatty(shellTerminal);

    if (shellIsInteractive) {
        //Loop until shell is in foreground
        while (tcgetpgrp(shellTerminal) != (shellPGID = getpgrp())) {
            kill(-shellPGID, SIGTTIN);
        }

        //Ignore interactive and job-control signals
        signal(SIGINT, SIG_IGN);
        signal(SIGQUIT, SIG_IGN);
        signal(SIGTSTP, SIG_IGN);
        signal(SIGTTIN, SIG_IGN);
        signal(SIGTTOU, SIG_IGN);
        signal(SIGCHLD, SIG_DFL);

        //Put shell in its own process group
        shellPGID = getpid();
        if (setpgid(0, shellPGID) < 0 && errno != EPERM) {
            perror("Couldn't put the shell in its own process group");
            exit(1);
        }

        //Grab control of the terminal
        tcsetpgrp(shellTerminal, shellPGID);

        //Save default terminal attributes
        if (tcgetattr(shellTerminal, &shellTermios) == -1) {
            perror("tcgetattr");
            exit(1);
        }
    }
}