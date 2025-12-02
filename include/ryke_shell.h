#ifndef RYKE_SHELL_H
#define RYKE_SHELL_H

#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <ctime>
#include <sys/types.h>
#include <termios.h>
#include <vector>

namespace ryke {

enum class ChainCondition {
    None,
    And,
    Or
};

struct Command {
    std::vector<std::string> args;
    std::optional<std::string> inputFile;
    std::optional<std::string> outputFile;
    std::optional<std::string> appendFile;
    std::optional<std::string> stderrFile;
    std::optional<std::string> stderrAppendFile;
    bool mergeStderr{false}; // for |& or &>
    std::optional<std::string> heredocDelimiter;
    std::optional<std::string> heredocData;
    std::optional<std::string> hereString;
    bool heredocStripTabs{false};
    bool heredocExpand{true};
    struct FdRedirection {
        int fd{1};
        enum class Type { Truncate, Append, Dup } type{Type::Truncate};
        std::string target; // file path for Truncate/Append
        int dupFd{1};       // target fd for Dup
    };
    std::vector<FdRedirection> fdRedirections;
};

struct Pipeline {
    std::vector<Command> stages;
    ChainCondition condition{ChainCondition::None}; //Relation to the previous pipeline
    bool background{false};
};

class History {
public:
    explicit History(std::size_t limit);

    struct Entry {
        std::string command;
        std::time_t timestamp{};
    };

    void add(const std::string& entry);
    [[nodiscard]] bool empty() const;
    [[nodiscard]] std::size_t size() const;
    [[nodiscard]] const std::deque<Entry>& entries() const;
    [[nodiscard]] const Entry& at(std::size_t index) const;

private:
    std::size_t limit_;
    std::deque<Entry> data_;
};

class AliasStore {
public:
    void set(const std::string& name, const std::string& value);
    [[nodiscard]] std::optional<std::string> resolve(const std::string& name) const;
    [[nodiscard]] const std::map<std::string, std::string>& all() const;

private:
    std::map<std::string, std::string> aliases_;
};

class PromptTheme {
public:
    explicit PromptTheme(std::string defaultColor, std::string defaultName = "green");

    bool applyColor(const std::string& colorName);
    [[nodiscard]] const std::string& colorCode() const;
    [[nodiscard]] const std::string& colorName() const;

private:
    std::string color_;
    std::string colorName_;
};

struct ShellOptions {
    bool monitor{true};
    bool noclobber{false};
    bool errexit{false};
    bool nounset{false};
    bool xtrace{false};
    bool notify{true};
    bool historyIgnoreDups{true};
    bool historyIgnoreSpace{true};
    bool noglob{false};
};

class Terminal {
public:
    Terminal();
    ~Terminal();

    Terminal(const Terminal&) = delete;
    Terminal& operator=(const Terminal&) = delete;

    [[nodiscard]] const termios& original() const;
    void restore() const;

private:
    termios original_{};
};

class RawModeGuard {
public:
    RawModeGuard(const Terminal& terminal, bool echo = false, bool enableSignals = true);
    ~RawModeGuard();

    RawModeGuard(const RawModeGuard&) = delete;
    RawModeGuard& operator=(const RawModeGuard&) = delete;

private:
    const Terminal& terminal_;
};

class AutocompleteEngine {
public:
    AutocompleteEngine() = default;

    std::string inlineSuggestion(const std::string& line, std::size_t cursorPos) const;
    std::vector<std::string> completionCandidates(const std::string& line, std::size_t cursorPos) const;

private:
    static bool isCommandPosition(const std::string& line, std::size_t wordStart);
    static std::vector<std::string> getExecutableNames(const std::string& prefix);
    static std::vector<std::string> getFilenames(const std::string& prefix);
    static std::string toLowerCase(const std::string& str);
};

class CommandParser {
public:
    CommandParser() = default;
    [[nodiscard]] std::vector<Pipeline> parse(const std::string& input) const;

private:
    struct Token {
        std::string text;
        bool quoted{false};
    };
    [[nodiscard]] std::vector<Token> tokenize(const std::string& input) const;
    [[nodiscard]] std::vector<Token> expandBraces(const std::vector<Token>& tokens) const;
    [[nodiscard]] std::string unescape(const std::string& token) const;
};

struct Job {
    enum class Status {
        Running,
        Stopped,
        Done
    };

    int id{};
    pid_t pgid{};
    std::string command;
    Status status{Status::Running};
    int exitCode{0};
};

class CommandExecutor {
public:
    CommandExecutor(pid_t shellPgid, int terminalFd, const ShellOptions* options,
                    std::function<void(const std::string&)> notifier);

    int execute(const std::vector<Pipeline>& pipelines, const std::string& commandLine);
    void reapBackground();
    void listJobs(std::ostream& os, bool verbose = false);
    bool foregroundJob(int jobId);
    bool backgroundJob(int jobId);
    void stopForeground();

private:
    int executePipeline(const Pipeline& pipeline, const std::string& commandLine);
    void adoptTerminal(pid_t pgid);
    void restoreTerminal();
    Job* findJob(int jobId);
    Job* lastJob();
    void pruneDone();

    pid_t shellPgid_{};
    int terminalFd_{};
    const ShellOptions* options_{};
    std::function<void(const std::string&)> notify_;
    pid_t currentFgPgid_{0};
    std::vector<Job> jobs_;
    int nextJobId_{1};
};

class Shell;

class BuiltinCommand {
public:
    virtual ~BuiltinCommand() = default;
    virtual void run(const Command& command, Shell& shell) = 0;
};

class CommandRegistry {
public:
    void registerCommand(const std::string& name, std::unique_ptr<BuiltinCommand> handler);
    bool tryHandle(const Command& command, Shell& shell) const;

private:
    std::map<std::string, std::unique_ptr<BuiltinCommand>> handlers_;
};

class InputReader {
public:
    InputReader(Terminal& terminal, History& history, const AutocompleteEngine& autocomplete,
                std::function<std::string()> promptProvider);

    std::string readLine();
    int interactiveListSelection(const std::vector<std::string>& items, const std::string& prompt);

private:
    Terminal& terminal_;
    History& history_;
    const AutocompleteEngine& autocomplete_;
    std::function<std::string()> promptProvider_;

    static std::size_t visibleLength(const std::string& text);
    static int readKey();
};

struct ShellConfig {
    std::size_t historyLimit{100};
    std::string defaultPromptColor{"\033[1;32m"};
    std::string defaultPromptColorName{"green"};
    std::string promptTemplate{"{color}{user}@{host}{reset}:{cwdcolor}{cwd}{reset}$ "};
    std::string historyFile;
    std::string aliasFile;
    std::string configFile;
};

class Shell {
public:
    explicit Shell(ShellConfig config = {});
    ~Shell();
    int run();
    int runScript(const std::string& path);

    History& history();
    AliasStore& aliases();
    PromptTheme& promptTheme();
    CommandParser& parser();
    CommandExecutor& executor();
    InputReader& inputReader();
    CommandRegistry& registry();
    const ShellConfig& config() const;
    ShellOptions& options();
    void requestExit(int status = 0);
    std::string promptTemplate() const;
    void setPromptTemplate(std::string templ);

    std::string buildPrompt() const;
    std::string expandInput(const std::string& input) const;
    std::string resolveAlias(const std::string& token) const;
    void saveState();
    void loadState();
    void applyOption(const std::string& name, bool enabled);
    void notifyBackground(const std::string& message) const;

private:
    ShellConfig config_;
    History history_;
    AliasStore aliases_;
    PromptTheme promptTheme_;
    Terminal terminal_;
    std::unique_ptr<AutocompleteEngine> autocomplete_;
    std::unique_ptr<CommandParser> parser_;
    std::unique_ptr<CommandExecutor> executor_;
    std::unique_ptr<CommandRegistry> registry_;
    std::unique_ptr<InputReader> inputReader_;
    std::string promptTemplate_;
    pid_t shellPgid_{};
    bool running_{true};
    int exitStatus_{0};
    std::string historyFile_;
    std::string aliasFile_;
    std::string configFile_;
    ShellOptions options_;

    void setupSignalHandlers();
    static void sigintHandler(int sig);
    static void sigtstpHandler(int sig);
    void registerBuiltinHandlers();
    std::string defaultPath(const std::string& filename) const;
};

} // namespace ryke

#endif //RYKE_SHELL_H
