#ifndef UTILS_H
#define UTILS_H

#include <string>

//Function Declarations
void displaySplashArt();
void setupSignalHandlers();
void sigintHandler(int sig);
std::string expandVariables(const std::string& input);
std::string getPrompt();

#endif