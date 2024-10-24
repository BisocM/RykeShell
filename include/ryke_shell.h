#ifndef RYKE_SHELL_H
#define RYKE_SHELL_H

#include <string>
#include <vector>
#include <deque>
#include <map>
#include <functional>

//Constants
extern const size_t HISTORY_LIMIT;

//Global Variables
extern std::deque<std::string> history;
extern std::map<std::string, std::string> aliases;
extern std::string promptColor;

//Struct Definitions
struct Command {
    std::vector<std::string> args;
    std::string inputFile;
    std::string outputFile;
    std::string appendFile;
    bool background = false;
};

//Command Handler Function Type
using CommandFunction = std::function<void(const Command&)>;

//Function Declarations (from utils.cpp)
void displaySplashArt();
void setupSignalHandlers();
void sigintHandler(int sig);
std::string expandVariables(const std::string& input);
std::string getPrompt();

//Function Declarations (from parser.cpp)
std::vector<Command> parseCommandLine(const std::string& input);

//Function Declarations (from executor.cpp)
void executeCommands(const std::vector<Command>& commands);

//Function Declarations (from input.cpp)
std::string readInputLine();
std::vector<std::string> getExecutableNames(const std::string& prefix);
std::vector<std::string> getFilenames(const std::string& prefix);

//Function Declarations (from commands.cpp)
void registerCommands();
bool handleCommand(const Command& command);

//Function Declarations (from input.cpp for interactive list)
int interactiveListSelection(const std::vector<std::string>& items, const std::string& prompt);

#endif