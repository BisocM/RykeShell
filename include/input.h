#ifndef INPUT_H
#define INPUT_H

#include <string>
#include <vector>

std::string readInputLine();
int interactiveListSelection(const std::vector<std::string>& items, const std::string& prompt);
int readKey();

#endif