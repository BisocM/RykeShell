#ifndef INPUT_H
#define INPUT_H

#include <string>
#include <vector>

//Function Declarations
std::string readInputLine();
std::vector<std::string> getExecutableNames(const std::string& prefix);
std::vector<std::string> getFilenames(const std::string& prefix);
int interactiveListSelection(const std::vector<std::string>& items, const std::string& prompt);
int readKey();

#endif