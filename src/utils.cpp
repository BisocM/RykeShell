#include "utils.h"
#include "ryke_shell.h"
#include <iostream>
#include <csignal>
#include <cstring>
#include <unistd.h>
#include <pwd.h>
#include <sstream>
#include <cstdlib>
#include <termios.h>
#include <sys/wait.h>

#include "job_control.h"

void resetTerminalSettings() {
    tcsetattr(STDIN_FILENO, TCSANOW, &shellTermios);
}

void displaySplashArt() {
    std::cout << "\033[1;34m"
              << " __________          __              _________.__             .__   .__   \n"
              << " \\______   \\ ___.__.|  | __  ____   /   _____/|  |__    ____  |  |  |  |  \n"
              << "  |       _/<   |  ||  |/ /_/ __ \\  \\_____  \\ |  |  \\ _/ __ \\ |  |  |  |  \n"
              << "  |    |   \\ \\___  ||    < \\  ___/  /        \\|   Y  \\\\  ___/ |  |__|  |__\n"
              << "  |____|_  / / ____||__|_ \\ \\___  >/_______  /|___|  / \\___  >|____/|____/\n"
              << "         \\/  \\/          \\/     \\/         \\/      \\/      \\/             \n"
              << "\033[0m" << std::endl;
}


void setupSignalHandlers() {
    struct sigaction sa{};
    sa.sa_handler = sigintHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa, nullptr);

    struct sigaction sa_chld{};
    sa_chld.sa_handler = sigchldHandler;
    sigemptyset(&sa_chld.sa_mask);
    sa_chld.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa_chld, nullptr);

    struct sigaction sa_tstp{};
    sa_tstp.sa_handler = sigtstpHandler;
    sigemptyset(&sa_tstp.sa_mask);
    sa_tstp.sa_flags = SA_RESTART;
    sigaction(SIGTSTP, &sa_tstp, nullptr);
}

void sigintHandler(int /*sig*/) {
    //Send SIGINT to the foreground process group
    if (shellIsInteractive) {
        kill(-shellPGID, SIGINT);
    }
    std::cout << std::endl;
}

void sigtstpHandler(int /*sig*/) {
    //Send SIGTSTP to the foreground process group
    if (shellIsInteractive) {
        kill(-shellPGID, SIGTSTP);
    }
}

void sigchldHandler(int /*sig*/) {
    const int saved_errno = errno;
    pid_t pid;
    int status;

    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
        pid_t pgid = getpgid(pid);
        Job* job = findJob(pgid);
        if (!job) continue;

        if (WIFEXITED(status) || WIFSIGNALED(status)) {
            //Process terminated
            removeJob(pgid);
        } else if (WIFSTOPPED(status)) {
            //Process stopped
            updateJobStatus(pgid, false, true);
            std::cout << "\n[" << job->id << "] Stopped    " << job->command << std::endl;
            std::cout << getPrompt();
            std::cout.flush();
        } else if (WIFCONTINUED(status)) {
            //Process continued
            updateJobStatus(pgid, true, false);
        }
    }
    errno = saved_errno;
}

std::string expandVariables(const std::string& input) {
    std::string output;
    size_t pos = 0;

    while (pos < input.length()) {
        if (input[pos] == '$') {
            if (pos + 1 < input.length() && input[pos + 1] == '{') {
                //Handle ${VAR}
                if (const size_t endBrace = input.find('}', pos + 2); endBrace != std::string::npos) {
                    std::string varExpr = input.substr(pos + 2, endBrace - pos - 2);
                    size_t colonDash = varExpr.find(":-");
                    std::string varName, defaultValue;

                    if (colonDash != std::string::npos) {
                        varName = varExpr.substr(0, colonDash);
                        defaultValue = varExpr.substr(colonDash + 2);
                    } else {
                        varName = varExpr;
                    }

                    if (const char* varValue = getenv(varName.c_str())) {
                        output += varValue;
                    } else {
                        output += defaultValue;
                    }
                    pos = endBrace + 1;
                    continue;
                }
            } else {
                //Handle $VAR
                size_t end = pos + 1;
                while (end < input.length() && (std::isalnum(input[end]) || input[end] == '_')) {
                    ++end;
                }
                std::string varName = input.substr(pos + 1, end - pos - 1);
                if (const char* varValue = getenv(varName.c_str())) {
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
    if (!getcwd(cwd, sizeof(cwd))) {
        std::strcpy(cwd, "?");
    }

    const char* userEnv = getenv("USER");
    std::string user;
    if (userEnv) {
        user = userEnv;
    } else {
        const passwd* pw = getpwuid(getuid());
        user = pw ? pw->pw_name : "user";
    }

    //Build the prompt string with colors
    std::string prompt = promptColor;
    prompt += user + "@" + hostname;
    prompt += "\033[0m:"; //Reset color
    prompt += "\033[1;34m" + std::string(cwd);
    prompt += "\033[0m$ ";

    return prompt;
}