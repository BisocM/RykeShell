// utils.cpp
#include "utils.h"
#include "ryke_shell.h"
#include <iostream>
#include <csignal>
#include <cstring>
#include <unistd.h>
#include <pwd.h>
#include <sstream>
#include <cctype>

void displaySplashArt() {
    std::cout << "\033[1;34m" //Bold Blue Text
              << "__________         __            _________.__           .__  .__   \n"
              << "\\______   \\___.__.|  | __ ____  /   _____/|  |__   ____ |  | |  |  \n"
              << " |       _<   |  ||  |/ // __ \\ \\_____  \\ |  |  \\_/ __ \\|  | |  |  \n"
              << " |    |   \\\\___  ||    <\\  ___/ /        \\|   Y  \\  ___/|  |_|  |__\n"
              << " |____|_  // ____||__|_ \\\\___  >_______  /|___|  /\\___  >____/____/\n"
              << "        \\/ \\/          \\/    \\/        \\/      \\/     \\/           \n"
              << "\033[0m"      //Reset formatting
              << std::endl;
}

void setupSignalHandlers() {
    struct sigaction sa;
    sa.sa_handler = sigintHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART; //Restart interrupted system calls
    sigaction(SIGINT, &sa, nullptr);
}

void sigintHandler(int sig) {
    //Ignore SIGINT (Ctrl+C)
    std::cout << std::endl;
}

std::string expandVariables(const std::string& input) {
    std::string output;
    size_t pos = 0;
    while (pos < input.length()) {
        if (input[pos] == '$') {
            if (pos + 1 < input.length() && input[pos + 1] == '{') {
                //${VAR}
                size_t endBrace = input.find('}', pos + 2);
                if (endBrace != std::string::npos) {
                    std::string varExpr = input.substr(pos + 2, endBrace - pos - 2);
                    //Check for default value
                    size_t colonDash = varExpr.find(":-");
                    std::string varName, defaultValue;
                    if (colonDash != std::string::npos) {
                        varName = varExpr.substr(0, colonDash);
                        defaultValue = varExpr.substr(colonDash + 2);
                    } else {
                        varName = varExpr;
                    }
                    const char* varValue = getenv(varName.c_str());
                    if (varValue != nullptr) {
                        output += varValue;
                    } else if (!defaultValue.empty()) {
                        output += defaultValue;
                    }
                    pos = endBrace + 1;
                    continue;
                }
            } else {
                //$VAR
                size_t end = pos + 1;
                while (end < input.length() && (isalnum(input[end]) || input[end] == '_')) {
                    ++end;
                }
                std::string varName = input.substr(pos + 1, end - pos - 1);
                const char* varValue = getenv(varName.c_str());
                if (varValue != nullptr) {
                    output += varValue;
                }
                pos = end;
                continue;
            }
        }
        output += input[pos++];
    }
    return output;
}

std::string getPrompt() {
    char hostname[1024] = {0};
    gethostname(hostname, sizeof(hostname));

    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) == nullptr) {
        std::strcpy(cwd, "?");
    }

    const char* userEnv = getenv("USER");
    std::string user;
    if (userEnv) {
        user = userEnv;
    } else {
        struct passwd* pw = getpwuid(getuid());
        user = pw ? pw->pw_name : "user";
    }

    //Build the prompt string with colors
    std::string prompt = promptColor; //Use selected color for user@hostname
    prompt += user;
    prompt += "@";
    prompt += hostname;
    prompt += "\033[0m:"; //Reset color
    prompt += "\033[1;34m"; //Bold Blue for directory
    prompt += cwd;
    prompt += "\033[0m$ "; //Reset color and add $

    return prompt;
}