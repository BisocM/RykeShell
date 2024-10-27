#include "commands.h"
#include "ryke_shell.h"
#include <iostream>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <dirent.h>
#include <iomanip>
#include <sstream>
#include <sys/stat.h>
#include <pwd.h>
#include <vector>
#include <algorithm>

#include "job_control.h"

std::map<std::string, CommandFunction> commandHandlers;

void registerCommands() {
    commandHandlers["exit"] = cmdExit;
    commandHandlers["cd"] = cmdCd;
    commandHandlers["pwd"] = cmdPwd;
    commandHandlers["history"] = cmdHistory;
    commandHandlers["alias"] = cmdAlias;
    commandHandlers["theme"] = cmdTheme;
    commandHandlers["ls"] = cmdLs;
    commandHandlers["export"] = cmdExport;
    commandHandlers["jobs"] = cmdJobs;
    commandHandlers["fg"] = cmdFg;
    commandHandlers["bg"] = cmdBg;
}

void cmdJobs(const Command& /*command*/) {
    listJobs();
}

void cmdFg(const Command& command) {
    Job* job = nullptr;

    if (command.args.size() == 1) {
        //No job ID specified, use the last job
        if (!jobList.empty()) {
            job = &jobList.back();
        } else {
            std::cerr << "fg: no current job\n";
            return;
        }
    } else {
        std::string arg = command.args[1];
        if (arg[0] == '%') {
            arg = arg.substr(1);
        }
        int jobID;
        try {
            jobID = std::stoi(arg);
        } catch (const std::invalid_argument&) {
            std::cerr << "fg: invalid job id\n";
            return;
        } catch (const std::out_of_range&) {
            std::cerr << "fg: job id out of range\n";
            return;
        }

        job = findJobByID(jobID);
        if (!job) {
            std::cerr << "fg: no such job\n";
            return;
        }
    }

    bringJobToForeground(job, true);
}

void cmdBg(const Command& command) {
    Job* job = nullptr;

    if (command.args.size() == 1) {
        //No job ID specified, use the last job
        if (!jobList.empty()) {
            job = &jobList.back();
        } else {
            std::cerr << "bg: no current job\n";
            return;
        }
    } else {
        std::string arg = command.args[1];
        if (arg[0] == '%') {
            arg = arg.substr(1);
        }
        int jobID;
        try {
            jobID = std::stoi(arg);
        } catch (const std::invalid_argument&) {
            std::cerr << "bg: invalid job id\n";
            return;
        } catch (const std::out_of_range&) {
            std::cerr << "bg: job id out of range\n";
            return;
        }

        job = findJobByID(jobID);
        if (!job) {
            std::cerr << "bg: no such job\n";
            return;
        }
    }

    continueJobInBackground(job, true);
}

bool handleCommand(const Command& command) {
    if (command.args.empty()) {
        return false;
    }

    const std::string cmdName = command.args[0];
    if (const auto it = commandHandlers.find(cmdName); it != commandHandlers.end()) {
        it->second(command);
        return true;
    }
    return false;
}

void cmdExit(const Command& /*command*/) {
    resetTerminalSettings();
    exit(0);
}

void cmdCd(const Command& command) {
    const std::string& dir = (command.args.size() > 1) ? command.args[1] : getenv("HOME");
    if (chdir(dir.c_str()) != 0) {
        std::cerr << "cd: " << strerror(errno) << '\n';
    }
}

void cmdPwd(const Command& /*command*/) {
    if (char cwd[1024]; getcwd(cwd, sizeof(cwd))) {
        std::cout << cwd << std::endl;
    } else {
        std::cerr << "pwd: " << strerror(errno) << '\n';
    }
}

void cmdHistory(const Command& /*command*/) {
    if (history.empty()) {
        std::cout << "No commands in history." << std::endl;
        return;
    }

    const std::vector<std::string> historyItems(history.begin(), history.end());
    if (const int selected = interactiveListSelection(historyItems, "Command History"); selected >= 0 && selected < static_cast<int>(historyItems.size())) {
        std::string input = historyItems[selected];

        input = expandVariables(input);

        std::string expandedInput = input;
        std::istringstream iss(input);
        std::string firstToken;
        iss >> firstToken;
        if (aliases.contains(firstToken)) {
            expandedInput = aliases[firstToken] + input.substr(firstToken.length());
        }

        if (const std::vector<Command> commands = parseCommandLine(expandedInput); !commands.empty()) {
            if (!handleCommand(commands[0])) {
                executeCommands(commands);
            }
        }
    }
}

void cmdAlias(const Command& command) {
    if (command.args.size() == 1) {
        for (const auto& [key, value] : aliases) {
            std::cout << "alias " << key << "='" << value << "'\n";
        }
    } else {
        for (size_t k = 1; k < command.args.size(); ++k) {
            std::string aliasStr = command.args[k];
            if (const size_t eqPos = aliasStr.find('='); eqPos != std::string::npos) {
                std::string key = aliasStr.substr(0, eqPos);
                std::string value = aliasStr.substr(eqPos + 1);
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
        const std::string& color = command.args[1];
        if (color == "red") {
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

void cmdExport(const Command &command) {
    if (command.args.size() > 1) {
        std::string assignment = command.args[1];
        if (const size_t eqPos = assignment.find('='); eqPos != std::string::npos) {
            const std::string var = assignment.substr(0, eqPos);
            const std::string value = assignment.substr(eqPos + 1);

            //Set the environment variable and check if it succeeded
            if (setenv(var.c_str(), value.c_str(), 1) == 0) {
                std::cout << "Environment variable " << var << " set to " << value << std::endl;
            } else {
                std::cerr << "Failed to set environment variable " << var << std::endl;
            }
        } else {
            std::cerr << "Invalid format. Use VAR=value" << std::endl;
        }
    } else {
        std::cerr << "No variable provided. Use: export VAR=value" << std::endl;
    }
}

void cmdLs(const Command& command) {
    std::string directory = ".";
    if (command.args.size() > 1) {
        directory = command.args[1];
    }

    DIR* dir = opendir(directory.c_str());
    if (!dir) {
        std::cerr << "ls: cannot access '" << directory << "': " << strerror(errno) << std::endl;
        return;
    }

    dirent* entry;
    std::vector<std::string> filenames;

    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_name[0] != '.') {
            filenames.emplace_back(entry->d_name);
        }
    }
    closedir(dir);

    std::ranges::sort(filenames);

    size_t maxLen = 0;
    for (const auto& name : filenames) {
        if (name.length() > maxLen) {
            maxLen = name.length();
        }
    }

    int count = 0;
    for (const auto& name : filenames) {
        std::string filepath = directory + "/" + name;
        struct stat fileStat{};
        if (stat(filepath.c_str(), &fileStat) == -1) {
            perror("stat");
            continue;
        }

        if (S_ISDIR(fileStat.st_mode)) {
            std::cout << "\033[1;34m" << std::setw(maxLen + 2) << std::left << name << "\033[0m";
        } else if (fileStat.st_mode & S_IXUSR) {
            std::cout << "\033[1;32m" << std::setw(maxLen + 2) << std::left << name << "\033[0m";
        } else {
            std::cout << std::setw(maxLen + 2) << std::left << name;
        }

        if (++count % 8 == 0) {
            std::cout << std::endl;
        }
    }
    if (count % 8 != 0) {
        std::cout << std::endl;
    }
}