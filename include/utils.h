#ifndef UTILS_H
#define UTILS_H

#include <string>

void resetTerminalSettings();
void displaySplashArt();
void setupSignalHandlers();
void sigintHandler(int sig);

std::string expandTilde(const std::string& path);
std::string expandVariables(const std::string& input);
std::string getPrompt();

#endif