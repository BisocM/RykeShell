#include "ryke_shell.h"
#include "commands.h"
#include "utils.h"

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <fstream>
#include <pwd.h>
#include <sstream>
#include <filesystem>
#include <atomic>
#include <unistd.h>
#include <sys/stat.h>

namespace {

std::string trim(const std::string& text) {
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }
    const auto last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

std::string getHomeDirectory() {
    if (const char* home = getenv("HOME")) {
        return home;
    }
    if (const passwd* pw = getpwuid(getuid())) {
        return pw->pw_dir;
    }
    return ".";
}

bool isWorldWritable(const std::string& path) {
    struct stat st {};
    if (stat(path.c_str(), &st) != 0) {
        return false;
    }
    return (st.st_mode & S_IWOTH) != 0;
}

} // namespace

namespace ryke {

static Shell* gShellInstance = nullptr;
static std::atomic<bool> gReapNeeded{false};

Shell::Shell(ShellConfig config)
    : config_(std::move(config)),
      history_(config_.historyLimit),
      aliases_(),
      promptTheme_(config_.defaultPromptColor, config_.defaultPromptColorName),
      terminal_(),
      autocomplete_(std::make_unique<AutocompleteEngine>()),
      parser_(std::make_unique<CommandParser>()),
      executor_(std::make_unique<CommandExecutor>(getpgrp(), STDIN_FILENO, &options_, [this](const std::string& msg) { notifyBackground(msg); })),
      registry_(std::make_unique<CommandRegistry>()),
      inputReader_(std::make_unique<InputReader>(terminal_, history_, *autocomplete_, [this]() { return buildPrompt(); })),
      promptTemplate_(config_.promptTemplate),
      shellPgid_(getpgrp()),
      running_(true),
      historyFile_(config_.historyFile.empty() ? defaultPath(".rykeshell_history") : config_.historyFile),
      aliasFile_(config_.aliasFile.empty() ? defaultPath(".rykeshell_aliases") : config_.aliasFile),
      configFile_(config_.configFile.empty() ? defaultPath(".rykeshell_config") : config_.configFile) {
    gShellInstance = this;
    setupSignalHandlers();
    registerBuiltinHandlers();
    loadState();
    const std::string rcPath = defaultPath(".rykeshellrc");
    if (std::filesystem::exists(rcPath)) {
        runScript(rcPath);
    }
}

Shell::~Shell() {
    if (running_) {
        saveState();
    }
    gShellInstance = nullptr;
}

int Shell::run() {
    displaySplashArt();

    while (running_) {
        if (gReapNeeded.exchange(false, std::memory_order_relaxed)) {
            executor_->reapBackground();
        }
        executor_->reapBackground();

        std::string rawInput = inputReader_->readLine();
        rawInput = trim(rawInput);
        if (rawInput.empty()) {
            continue;
        }

        if (!(options_.historyIgnoreSpace && !rawInput.empty() && rawInput.front() == ' ')) {
            if (!options_.historyIgnoreDups || history_.empty() || history_.entries().back().command != rawInput) {
                history_.add(rawInput);
            }
        }

        std::string expandedInput;
        try {
            expandedInput = expandInput(rawInput);
        } catch (const std::exception& ex) {
            std::cerr << ex.what() << '\n';
            continue;
        }
        const std::vector<Pipeline> pipelines = parser_->parse(expandedInput);
        if (pipelines.empty()) {
            continue;
        }

        const Pipeline& firstPipeline = pipelines.front();
        const bool isSingleBuiltin = pipelines.size() == 1 && firstPipeline.stages.size() == 1;
        if (isSingleBuiltin && registry_->tryHandle(firstPipeline.stages.front(), *this)) {
            continue;
        }

        const int status = executor_->execute(pipelines, rawInput);
        if (options_.errexit && status != 0) {
            requestExit(status);
        }
    }

    saveState();
    return exitStatus_;
}

int Shell::runScript(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        std::cerr << "Failed to open script: " << path << '\n';
        return 1;
    }

    std::string line;
    while (running_ && std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }

        std::string expandedInput;
        try {
            expandedInput = expandInput(line);
        } catch (const std::exception& ex) {
            std::cerr << ex.what() << '\n';
            continue;
        }
        std::vector<Pipeline> pipelines = parser_->parse(expandedInput);
        if (pipelines.empty()) {
            continue;
        }

        for (auto& pipeline : pipelines) {
            for (auto& cmd : pipeline.stages) {
                if (cmd.heredocDelimiter && !cmd.heredocData) {
                    std::string heredoc;
                    std::string docLine;
                    while (std::getline(in, docLine)) {
                        if (cmd.heredocStripTabs) {
                            while (!docLine.empty() && docLine.front() == '\t') {
                                docLine.erase(docLine.begin());
                            }
                        }
                        if (docLine == *cmd.heredocDelimiter) {
                            break;
                        }
                        heredoc += docLine + '\n';
                    }
                    cmd.heredocData = heredoc;
                }
            }
        }

        const Pipeline& firstPipeline = pipelines.front();
        const bool isSingleBuiltin = pipelines.size() == 1 && firstPipeline.stages.size() == 1;
        if (isSingleBuiltin && registry_->tryHandle(firstPipeline.stages.front(), *this)) {
            continue;
        }

        const int status = executor_->execute(pipelines, line);
        if (options_.errexit && status != 0) {
            requestExit(status);
        }
    }

    saveState();
    return exitStatus_;
}

History& Shell::history() {
    return history_;
}

AliasStore& Shell::aliases() {
    return aliases_;
}

PromptTheme& Shell::promptTheme() {
    return promptTheme_;
}

CommandParser& Shell::parser() {
    return *parser_;
}

CommandExecutor& Shell::executor() {
    return *executor_;
}

InputReader& Shell::inputReader() {
    return *inputReader_;
}

CommandRegistry& Shell::registry() {
    return *registry_;
}

const ShellConfig& Shell::config() const {
    return config_;
}

ShellOptions& Shell::options() {
    return options_;
}

void Shell::requestExit(int status) {
    running_ = false;
    exitStatus_ = status;
}

std::string Shell::promptTemplate() const {
    return promptTemplate_;
}

void Shell::setPromptTemplate(std::string templ) {
    promptTemplate_ = std::move(templ);
}

std::string Shell::buildPrompt() const {
    char hostname[1024] = {0};
    gethostname(hostname, sizeof(hostname));

    char cwd[1024];
    if (!getcwd(cwd, sizeof(cwd))) {
        std::snprintf(cwd, sizeof(cwd), "?");
    }

    std::string user;
    if (const char* userEnv = getenv("USER")) {
        user = userEnv;
    } else if (const passwd* pw = getpwuid(getuid()); pw) {
        user = pw->pw_name;
    } else {
        user = "user";
    }

    const std::string reset = "\033[0m";
    const std::string cwdColor = "\033[1;34m";

    std::string prompt = promptTemplate_;
    auto replaceAll = [&](const std::string& key, const std::string& value) {
        std::size_t pos = 0;
        while ((pos = prompt.find(key, pos)) != std::string::npos) {
            prompt.replace(pos, key.length(), value);
            pos += value.length();
        }
    };

    replaceAll("{user}", user);
    replaceAll("{host}", hostname);
    replaceAll("{cwd}", cwd);
    replaceAll("{color}", promptTheme_.colorCode());
    replaceAll("{reset}", reset);
    replaceAll("{cwdcolor}", cwdColor);

    return prompt;
}

std::string Shell::expandInput(const std::string& input) const {
    const std::string expandedVars = expandVariables(input, &options_);

    std::istringstream iss(expandedVars);
    std::string firstToken;
    if (!(iss >> firstToken)) {
        return expandedVars;
    }

    if (const auto aliasValue = aliases_.resolve(firstToken)) {
        const auto pos = expandedVars.find(firstToken);
        const std::string remainder = pos != std::string::npos ? expandedVars.substr(pos + firstToken.size()) : "";
        return *aliasValue + remainder;
    }

    return expandedVars;
}

std::string Shell::resolveAlias(const std::string& token) const {
    if (const auto alias = aliases_.resolve(token)) {
        return *alias;
    }
    return token;
}

void Shell::setupSignalHandlers() {
    struct sigaction sa {};
    sa.sa_handler = sigintHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa, nullptr);

    struct sigaction stp {};
    stp.sa_handler = sigtstpHandler;
    sigemptyset(&stp.sa_mask);
    stp.sa_flags = SA_RESTART;
    sigaction(SIGTSTP, &stp, nullptr);

    struct sigaction sc {};
    sc.sa_handler = [](int) { gReapNeeded.store(true, std::memory_order_relaxed); };
    sigemptyset(&sc.sa_mask);
    sc.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &sc, nullptr);
}

void Shell::sigintHandler(int /*sig*/) {
    const char newline = '\n';
    write(STDOUT_FILENO, &newline, 1);
}

void Shell::sigtstpHandler(int /*sig*/) {
    const char newline = '\n';
    write(STDOUT_FILENO, &newline, 1);
    if (gShellInstance) {
        gShellInstance->executor().stopForeground();
    }
}

void Shell::registerBuiltinHandlers() {
    registerBuiltinCommands(*registry_);
}

std::string Shell::defaultPath(const std::string& filename) const {
    std::string base = getHomeDirectory();
    if (!base.empty() && base.back() != '/') {
        base += '/';
    }
    return base + filename;
}

void Shell::saveState() {
    // Ensure directory exists
    const auto ensureDir = [](const std::string& path) {
        const auto pos = path.find_last_of('/');
        if (pos == std::string::npos) return;
        const std::string dir = path.substr(0, pos);
        mkdir(dir.c_str(), 0755);
    };

    ensureDir(historyFile_);
    ensureDir(aliasFile_);
    ensureDir(configFile_);

    std::ofstream historyOut(historyFile_);
    for (const auto& entry : history_.entries()) {
        historyOut << entry.command << '\n';
    }

    std::ofstream aliasOut(aliasFile_);
    for (const auto& [name, value] : aliases_.all()) {
        aliasOut << name << '=' << value << '\n';
    }

    std::ofstream configOut(configFile_);
    configOut << "prompt_color=" << promptTheme_.colorName() << '\n';
    configOut << "prompt_template=" << promptTemplate_ << '\n';
    configOut << "option=monitor:" << (options_.monitor ? 1 : 0) << '\n';
    configOut << "option=noclobber:" << (options_.noclobber ? 1 : 0) << '\n';
    configOut << "option=errexit:" << (options_.errexit ? 1 : 0) << '\n';
    configOut << "option=nounset:" << (options_.nounset ? 1 : 0) << '\n';
    configOut << "option=xtrace:" << (options_.xtrace ? 1 : 0) << '\n';
    configOut << "option=notify:" << (options_.notify ? 1 : 0) << '\n';
    configOut << "option=history-ignore-dups:" << (options_.historyIgnoreDups ? 1 : 0) << '\n';
    configOut << "option=history-ignore-space:" << (options_.historyIgnoreSpace ? 1 : 0) << '\n';
    configOut << "option=noglob:" << (options_.noglob ? 1 : 0) << '\n';
}

void Shell::loadState() {
    if (isWorldWritable(historyFile_)) {
        std::cerr << "Warning: history file is world-writable: " << historyFile_ << '\n';
    }
    if (std::ifstream historyIn(historyFile_); historyIn) {
        std::string line;
        while (std::getline(historyIn, line)) {
            history_.add(line);
        }
    }

    if (isWorldWritable(aliasFile_)) {
        std::cerr << "Warning: alias file is world-writable: " << aliasFile_ << '\n';
    }
    if (std::ifstream aliasIn(aliasFile_); aliasIn) {
        std::string line;
        while (std::getline(aliasIn, line)) {
            const auto pos = line.find('=');
            if (pos == std::string::npos) continue;
            const std::string name = line.substr(0, pos);
            const std::string value = line.substr(pos + 1);
            aliases_.set(name, value);
        }
    }

    if (isWorldWritable(configFile_)) {
        std::cerr << "Warning: config file is world-writable: " << configFile_ << '\n';
    }
    if (std::ifstream configIn(configFile_); configIn) {
        std::string line;
        while (std::getline(configIn, line)) {
            const auto pos = line.find('=');
            if (pos == std::string::npos) continue;
            const std::string key = line.substr(0, pos);
            const std::string value = line.substr(pos + 1);
            if (key == "prompt_color") {
                promptTheme_.applyColor(value);
            } else if (key == "prompt_template") {
                promptTemplate_ = value;
            } else if (key == "option") {
                const auto colon = value.find(':');
                if (colon != std::string::npos) {
                    const std::string optName = value.substr(0, colon);
                    const bool enabled = value.substr(colon + 1) == "1";
                    applyOption(optName, enabled);
                }
            }
        }
    }
}

void Shell::applyOption(const std::string& name, bool enabled) {
    if (name == "monitor") options_.monitor = enabled;
    else if (name == "noclobber") options_.noclobber = enabled;
    else if (name == "errexit") options_.errexit = enabled;
    else if (name == "nounset") options_.nounset = enabled;
    else if (name == "xtrace") options_.xtrace = enabled;
    else if (name == "notify") options_.notify = enabled;
    else if (name == "history-ignore-dups") options_.historyIgnoreDups = enabled;
    else if (name == "history-ignore-space") options_.historyIgnoreSpace = enabled;
    else if (name == "noglob") options_.noglob = enabled;
}
void Shell::notifyBackground(const std::string& message) const {
    std::cout << message << '\n';
}

} // namespace ryke
