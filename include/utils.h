#ifndef UTILS_H
#define UTILS_H

#include <string>

namespace ryke {

void displaySplashArt();
std::string expandTilde(const std::string& path);
class ShellOptions;

std::string expandVariables(const std::string& input, const ShellOptions* options = nullptr);

} // namespace ryke

#endif //UTILS_H
