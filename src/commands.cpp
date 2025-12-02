#include "commands.h"
#include "utils.h"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <dlfcn.h>
#include <iomanip>
#include <iostream>
#include <memory>
#include <pwd.h>
#include <ranges>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace ryke {

void CommandRegistry::registerCommand(const std::string& name, std::unique_ptr<BuiltinCommand> handler) {
    handlers_[name] = std::move(handler);
}

bool CommandRegistry::tryHandle(const Command& command, Shell& shell) const {
    if (command.args.empty()) {
        return false;
    }

    const std::string& name = command.args.front();
    if (const auto it = handlers_.find(name); it != handlers_.end()) {
        it->second->run(command, shell);
        return true;
    }
    return false;
}

namespace {

class ExitCommand : public BuiltinCommand {
public:
    void run(const Command& /*command*/, Shell& shell) override {
        shell.saveState();
        shell.requestExit(0);
    }
};

class CdCommand : public BuiltinCommand {
public:
    void run(const Command& command, Shell& /*shell*/) override {
        std::string target;
        if (command.args.size() == 1) {
            const char* home = getenv("HOME");
            if (!home) {
                if (auto* pw = getpwuid(getuid())) {
                    home = pw->pw_dir;
                }
            }
            target = home ? home : "/";
        } else {
            target = expandTilde(command.args[1]);
        }

        if (chdir(target.c_str()) != 0) {
            std::cerr << "cd: " << strerror(errno) << '\n';
        }
    }
};

class PwdCommand : public BuiltinCommand {
public:
    void run(const Command& /*command*/, Shell& /*shell*/) override {
        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd))) {
            std::cout << cwd << '\n';
        } else {
            std::cerr << "pwd: " << strerror(errno) << '\n';
        }
    }
};

class HistoryCommand : public BuiltinCommand {
public:
    void run(const Command& /*command*/, Shell& shell) override {
        if (shell.history().empty()) {
            std::cout << "No commands in history.\n";
            return;
        }

        const auto& entries = shell.history().entries();
        std::vector<std::string> items;
        items.reserve(entries.size());
        for (const auto& e : entries) {
            items.push_back(e.command);
        }
        const int selected = shell.inputReader().interactiveListSelection(items, "Command History");
        if (selected < 0 || selected >= static_cast<int>(items.size())) {
            return;
        }

        const std::string input = items[static_cast<std::size_t>(selected)];
        const std::string expanded = shell.expandInput(input);
        const std::vector<Pipeline> pipelines = shell.parser().parse(expanded);
        if (pipelines.empty()) {
            return;
        }

        const Pipeline& first = pipelines.front();
        const bool singleBuiltin = pipelines.size() == 1 && first.stages.size() == 1;
        if (singleBuiltin && shell.registry().tryHandle(first.stages.front(), shell)) {
            return;
        }

        shell.executor().execute(pipelines, input);
    }
};

class AliasCommand : public BuiltinCommand {
public:
    void run(const Command& command, Shell& shell) override {
        auto& aliasStore = shell.aliases();
        if (command.args.size() == 1) {
            for (const auto& [name, value] : aliasStore.all()) {
                std::cout << "alias " << name << "='" << value << "'\n";
            }
            return;
        }

        for (std::size_t i = 1; i < command.args.size(); ++i) {
            const std::string& arg = command.args[i];
            if (const auto eqPos = arg.find('='); eqPos != std::string::npos) {
                std::string name = arg.substr(0, eqPos);
                std::string value = arg.substr(eqPos + 1);
                if (!value.empty() && value.front() == '\'' && value.back() == '\'') {
                    value = value.substr(1, value.size() - 2);
                }
                aliasStore.set(name, value);
            }
        }
    }
};

class ThemeCommand : public BuiltinCommand {
public:
    void run(const Command& command, Shell& shell) override {
        if (command.args.size() < 2) {
            std::cout << "Usage: theme [color]\n";
            return;
        }

        const std::string& color = command.args[1];
        if (!shell.promptTheme().applyColor(color)) {
            std::cerr << "Unknown color: " << color << '\n';
        }
    }
};

class PromptCommand : public BuiltinCommand {
public:
    void run(const Command& command, Shell& shell) override {
        if (command.args.size() == 1) {
            std::cout << "Current template: " << shell.promptTemplate() << "\n";
            std::cout << "Placeholders: {user}, {host}, {cwd}, {color}, {cwdcolor}, {reset}\n";
            return;
        }

        if (command.args.size() >= 2) {
            std::ostringstream oss;
            for (std::size_t i = 1; i < command.args.size(); ++i) {
                if (i > 1) oss << ' ';
                oss << command.args[i];
            }
            shell.setPromptTemplate(oss.str());
        }
    }
};

class ExportCommand : public BuiltinCommand {
public:
    void run(const Command& command, Shell& /*shell*/) override {
        if (command.args.size() < 2) {
            std::cerr << "No variable provided. Use: export VAR=value\n";
            return;
        }

        const std::string& assignment = command.args[1];
        if (const auto eqPos = assignment.find('='); eqPos != std::string::npos) {
            const std::string var = assignment.substr(0, eqPos);
            const std::string value = assignment.substr(eqPos + 1);
            if (setenv(var.c_str(), value.c_str(), 1) != 0) {
                std::cerr << "Failed to set environment variable " << var << '\n';
            }
        } else {
            std::cerr << "Invalid format. Use VAR=value\n";
        }
    }
};

class LsCommand : public BuiltinCommand {
public:
    void run(const Command& command, Shell& /*shell*/) override {
        std::string directory = ".";
        if (command.args.size() > 1) {
            directory = command.args[1];
        }

        DIR* dir = opendir(directory.c_str());
        if (!dir) {
            std::cerr << "ls: cannot access '" << directory << "': " << strerror(errno) << '\n';
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

        std::size_t maxLen = 0;
        for (const auto& name : filenames) {
            maxLen = std::max(maxLen, name.length());
        }

        int count = 0;
        for (const auto& name : filenames) {
            std::string filepath = directory + "/" + name;
            struct stat fileStat {};
            if (stat(filepath.c_str(), &fileStat) == -1) {
                perror("stat");
                continue;
            }

            if (S_ISDIR(fileStat.st_mode)) {
                std::cout << "\033[1;34m" << std::setw(static_cast<int>(maxLen + 2)) << std::left << name << "\033[0m";
            } else if (fileStat.st_mode & S_IXUSR) {
                std::cout << "\033[1;32m" << std::setw(static_cast<int>(maxLen + 2)) << std::left << name << "\033[0m";
            } else {
                std::cout << std::setw(static_cast<int>(maxLen + 2)) << std::left << name;
            }

            if (++count % 8 == 0) {
                std::cout << '\n';
            }
        }
        if (count % 8 != 0) {
            std::cout << '\n';
        }
    }
};

class JobsCommand : public BuiltinCommand {
public:
    void run(const Command& command, Shell& shell) override {
        bool verbose = false;
        if (command.args.size() > 1 && command.args[1] == "-l") {
            verbose = true;
        }
        shell.executor().listJobs(std::cout, verbose);
    }
};

class FgCommand : public BuiltinCommand {
public:
    void run(const Command& command, Shell& shell) override {
        int jobId = -1;
        if (command.args.size() > 1) {
            try {
                jobId = std::stoi(command.args[1]);
            } catch (...) {
                std::cerr << "fg: invalid job id\n";
                return;
            }
        }
        if (!shell.executor().foregroundJob(jobId)) {
            std::cerr << "fg: no such job\n";
        }
    }
};

class BgCommand : public BuiltinCommand {
public:
    void run(const Command& command, Shell& shell) override {
        int jobId = -1;
        if (command.args.size() > 1) {
            try {
                jobId = std::stoi(command.args[1]);
            } catch (...) {
                std::cerr << "bg: invalid job id\n";
                return;
            }
        }
        if (!shell.executor().backgroundJob(jobId)) {
            std::cerr << "bg: no such job\n";
        }
    }
};

class HelpCommand : public BuiltinCommand {
public:
    void run(const Command& /*command*/, Shell& /*shell*/) override {
        std::cout << "Built-ins: cd, pwd, history, alias, prompt, theme, set, ls, export, "
                     "jobs, fg, bg, source, plugin, exit, help\n";
    }
};

class SetCommand : public BuiltinCommand {
public:
    void run(const Command& command, Shell& shell) override {
        auto toggle = [&](const std::string& name, bool enable) {
            shell.applyOption(name, enable);
        };

        if (command.args.size() == 1) {
            std::cout << "Options: "
                      << "monitor=" << shell.options().monitor << " "
                      << "noclobber=" << shell.options().noclobber << " "
                      << "errexit=" << shell.options().errexit << " "
                      << "nounset=" << shell.options().nounset << " "
                      << "xtrace=" << shell.options().xtrace << " "
                      << "notify=" << shell.options().notify << " "
                      << "history-ignore-dups=" << shell.options().historyIgnoreDups << " "
                      << "history-ignore-space=" << shell.options().historyIgnoreSpace << " "
                      << "noglob=" << shell.options().noglob
                      << '\n';
            return;
        }

        for (std::size_t i = 1; i < command.args.size(); ++i) {
            const std::string& arg = command.args[i];
            if (arg == "-e") toggle("errexit", true);
            else if (arg == "+e") toggle("errexit", false);
            else if (arg == "-u") toggle("nounset", true);
            else if (arg == "+u") toggle("nounset", false);
            else if (arg == "-x") toggle("xtrace", true);
            else if (arg == "+x") toggle("xtrace", false);
            else if (arg == "-C") toggle("noclobber", true);
            else if (arg == "+C") toggle("noclobber", false);
            else if (arg == "-m") toggle("monitor", true);
            else if (arg == "+m") toggle("monitor", false);
            else if (arg == "-o") {
                if (i + 1 < command.args.size()) {
                    toggle(command.args[++i], true);
                }
            } else if (arg == "+o") {
                if (i + 1 < command.args.size()) {
                    toggle(command.args[++i], false);
                }
            } else if (arg == "-f") {
                toggle("noglob", true);
            } else if (arg == "+f") {
                toggle("noglob", false);
            }
        }
    }
};

class SourceCommand : public BuiltinCommand {
public:
    void run(const Command& command, Shell& shell) override {
        if (command.args.size() < 2) {
            std::cerr << "source: filename required\n";
            return;
        }
        shell.runScript(command.args[1]);
    }
};

class PluginCommand : public BuiltinCommand {
public:
    void run(const Command& command, Shell& shell) override {
        if (command.args.size() < 3 || command.args[1] != "load") {
            std::cerr << "plugin: usage: plugin load <path>\n";
            return;
        }
        const std::string& path = command.args[2];
        void* handle = dlopen(path.c_str(), RTLD_LAZY);
        if (!handle) {
            std::cerr << "plugin: " << dlerror() << '\n';
            return;
        }
        using RegisterFn = void(*)(Shell&);
        dlerror();
        auto* fn = reinterpret_cast<RegisterFn>(dlsym(handle, "register_plugin"));
        if (const char* err = dlerror()) {
            std::cerr << "plugin: " << err << '\n';
            dlclose(handle);
            return;
        }
        fn(shell);
    }
};

} // namespace

void registerBuiltinCommands(CommandRegistry& registry) {
    registry.registerCommand("exit", std::make_unique<ExitCommand>());
    registry.registerCommand("cd", std::make_unique<CdCommand>());
    registry.registerCommand("pwd", std::make_unique<PwdCommand>());
    registry.registerCommand("history", std::make_unique<HistoryCommand>());
    registry.registerCommand("alias", std::make_unique<AliasCommand>());
    registry.registerCommand("theme", std::make_unique<ThemeCommand>());
    registry.registerCommand("prompt", std::make_unique<PromptCommand>());
    registry.registerCommand("ls", std::make_unique<LsCommand>());
    registry.registerCommand("export", std::make_unique<ExportCommand>());
    registry.registerCommand("jobs", std::make_unique<JobsCommand>());
    registry.registerCommand("fg", std::make_unique<FgCommand>());
    registry.registerCommand("bg", std::make_unique<BgCommand>());
    registry.registerCommand("set", std::make_unique<SetCommand>());
    registry.registerCommand("source", std::make_unique<SourceCommand>());
    registry.registerCommand("plugin", std::make_unique<PluginCommand>());
    registry.registerCommand("help", std::make_unique<HelpCommand>());
}

} // namespace ryke
