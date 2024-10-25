#ifndef AUTOCOMPLETE_H
#define AUTOCOMPLETE_H

#include <string>
#include <vector>

void handleAutoComplete(std::string& input, std::string& currentWord, size_t& cursorPos);
std::string getSuggestion(const std::string& currentWord, const std::string& input, size_t cursorPos);
std::vector<std::string> getExecutableNames(const std::string& prefix);
std::vector<std::string> getFilenames(const std::string& prefix);

#endif