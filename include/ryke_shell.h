#ifndef RYKE_SHELL_H
#define RYKE_SHELL_H

#include <string>
#include <vector>
#include <deque>
#include <map>
#include <functional>
#include <termios.h>

extern const size_t HISTORY_LIMIT;

extern std::deque<std::string> history;
extern std::map<std::string, std::string> aliases;
extern std::string promptColor;
extern termios shellTermios;
extern pid_t shellPGID;
extern int shellTerminal;
extern bool shellIsInteractive;

struct Command {
    std::vector<std::string> args;
    std::string inputFile;
    std::string outputFile;
    std::string appendFile;
    bool background = false;
};

using CommandFunction = std::function<void(const Command&)>;

void initShell();

void setupSignalHandlers();
void sigintHandler(int sig);
void sigtstpHandler(int sig);
void sigchldHandler(int sig);

void displaySplashArt();
std::string expandVariables(const std::string& input);
std::string getPrompt();
void resetTerminalSettings();

std::vector<Command> parseCommandLine(const std::string& input);
void executeCommands(const std::vector<Command>& commands);

std::string readInputLine();
std::vector<std::string> getExecutableNames(const std::string& prefix);
std::vector<std::string> getFilenames(const std::string& prefix);
int interactiveListSelection(const std::vector<std::string>& items, const std::string& prompt);
int readKey();

void registerCommands();
bool handleCommand(const Command& command);

#endif //RYKE_SHELL_H