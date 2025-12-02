#include "ryke_shell.h"
#include "utils.h"

#include <csignal>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <iostream>
#include <sstream>
#include <pwd.h>
#include <stdexcept>
#include <unistd.h>
#include <cctype>

namespace ryke {

History::History(std::size_t limit) : limit_(limit) {}

void History::add(const std::string& entry) {
    if (entry.empty()) {
        return;
    }

    data_.push_back(Entry{entry, std::time(nullptr)});
    if (data_.size() > limit_) {
        data_.pop_front();
    }
}

bool History::empty() const {
    return data_.empty();
}

std::size_t History::size() const {
    return data_.size();
}

const std::deque<History::Entry>& History::entries() const {
    return data_;
}

const History::Entry& History::at(std::size_t index) const {
    return data_.at(index);
}

void AliasStore::set(const std::string& name, const std::string& value) {
    aliases_[name] = value;
}

std::optional<std::string> AliasStore::resolve(const std::string& name) const {
    if (const auto it = aliases_.find(name); it != aliases_.end()) {
        return it->second;
    }
    return std::nullopt;
}

const std::map<std::string, std::string>& AliasStore::all() const {
    return aliases_;
}

PromptTheme::PromptTheme(std::string defaultColor, std::string defaultName)
    : color_(std::move(defaultColor)), colorName_(std::move(defaultName)) {}

bool PromptTheme::applyColor(const std::string& colorName) {
    static const std::map<std::string, std::string> colorMap = {
        {"red", "\033[1;31m"},
        {"green", "\033[1;32m"},
        {"yellow", "\033[1;33m"},
        {"blue", "\033[1;34m"},
        {"magenta", "\033[1;35m"},
        {"cyan", "\033[1;36m"}
    };

    if (const auto it = colorMap.find(colorName); it != colorMap.end()) {
        color_ = it->second;
        colorName_ = colorName;
        return true;
    }
    return false;
}

const std::string& PromptTheme::colorCode() const {
    return color_;
}

const std::string& PromptTheme::colorName() const {
    return colorName_;
}

Terminal::Terminal() {
    if (tcgetattr(STDIN_FILENO, &original_) == -1) {
        throw std::runtime_error("Failed to read terminal attributes");
    }
}

Terminal::~Terminal() {
    restore();
}

const termios& Terminal::original() const {
    return original_;
}

void Terminal::restore() const {
    tcsetattr(STDIN_FILENO, TCSANOW, &original_);
}

RawModeGuard::RawModeGuard(const Terminal& terminal, bool echo, bool enableSignals)
    : terminal_(terminal) {
    termios raw = terminal_.original();
    raw.c_lflag &= ~ICANON;
    if (!echo) {
        raw.c_lflag &= ~ECHO;
    }
    if (!enableSignals) {
        raw.c_lflag &= ~ISIG;
    }
    raw.c_iflag &= ~(IXON | ICRNL);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
}

RawModeGuard::~RawModeGuard() {
    terminal_.restore();
}

std::string expandTilde(const std::string& path) {
    if (path.empty() || path.front() != '~') {
        return path;
    }

    const auto slashPos = path.find('/');
    const std::string userPart = slashPos == std::string::npos ? path.substr(1) : path.substr(1, slashPos - 1);

    const char* home = nullptr;
    if (userPart.empty()) {
        home = getenv("HOME");
        if (!home) {
            if (const auto* pw = getpwuid(getuid())) {
                home = pw->pw_dir;
            }
        }
    } else {
        if (const auto* pw = getpwnam(userPart.c_str())) {
            home = pw->pw_dir;
        }
    }

    if (!home) {
        return path;
    }

    if (slashPos == std::string::npos) {
        return std::string(home);
    }
    return std::string(home) + path.substr(slashPos);
}

std::string expandVariables(const std::string& input, const ShellOptions* options) {
    std::string output;
    bool inSingle = false;
    bool inDouble = false;
    std::size_t i = 0;

    auto isWordBoundary = [](char c) {
        return std::isspace(static_cast<unsigned char>(c));
    };

    auto expandCommandSubst = [&](std::size_t start) -> std::pair<std::string, std::size_t> {
        int depth = 0;
        std::string cmd;
        std::size_t j = start;
        for (; j < input.size(); ++j) {
            if (input[j] == '(') {
                depth++;
                if (depth == 1) continue;
            } else if (input[j] == ')') {
                depth--;
                if (depth == 0) {
                    ++j;
                    break;
                }
            }
            if (depth >= 1) cmd.push_back(input[j]);
        }

        std::string result;
        if (!cmd.empty()) {
            FILE* fp = popen(cmd.c_str(), "r");
            if (fp) {
                char buf[256];
                while (fgets(buf, sizeof(buf), fp)) {
                    result += buf;
                }
                pclose(fp);
            }
            while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
                result.pop_back();
            }
        }
        return {result, j};
    };

    auto expandArithmetic = [&](std::size_t start) -> std::pair<std::string, std::size_t> {
        int depth = 0;
        std::string expr;
        std::size_t j = start;
        for (; j < input.size(); ++j) {
            if (input[j] == '(') {
                depth++;
                if (depth == 1) continue;
            } else if (input[j] == ')') {
                depth--;
                if (depth == 0) {
                    ++j;
                    break;
                }
            }
            if (depth >= 1) expr.push_back(input[j]);
        }
        auto eval = [](const std::string& e) -> long {
            long total = 0;
            char op = '+';
            std::size_t idx = 0;
            while (idx < e.size()) {
                while (idx < e.size() && std::isspace(static_cast<unsigned char>(e[idx]))) ++idx;
                bool negative = false;
                if (idx < e.size() && (e[idx] == '+' || e[idx] == '-')) {
                    negative = (e[idx] == '-');
                    ++idx;
                }
                long val = 0;
                while (idx < e.size() && std::isdigit(static_cast<unsigned char>(e[idx]))) {
                    val = val * 10 + (e[idx] - '0');
                    ++idx;
                }
                if (negative) val = -val;

                switch (op) {
                    case '+': total += val; break;
                    case '-': total -= val; break;
                    case '*': total *= val; break;
                    case '/': total = val != 0 ? total / val : total; break;
                    default: break;
                }

                while (idx < e.size() && std::isspace(static_cast<unsigned char>(e[idx]))) ++idx;
                if (idx < e.size()) {
                    op = e[idx];
                    ++idx;
                }
            }
            return total;
        };

        long value = eval(expr);
        return {std::to_string(value), j};
    };

    while (i < input.size()) {
        const char c = input[i];

        if (c == '\\' && !inSingle) {
            if (i + 1 < input.size()) {
                output.push_back(input[i + 1]);
                i += 2;
                continue;
            }
        }

        if (c == '\'' && !inDouble) {
            inSingle = !inSingle;
            output.push_back(c);
            ++i;
            continue;
        }
        if (c == '"' && !inSingle) {
            inDouble = !inDouble;
            output.push_back(c);
            ++i;
            continue;
        }

        const bool atWordStart = (i == 0 || isWordBoundary(input[i - 1]));
        if (c == '~' && !inSingle && !inDouble && atWordStart) {
            std::size_t end = i + 1;
            while (end < input.size() && !isWordBoundary(input[end])) {
                ++end;
            }
            output += expandTilde(input.substr(i, end - i));
            i = end;
            continue;
        }

        if (c == '$' && !inSingle) {
            if (i + 1 < input.size() && input[i + 1] == '(' && i + 2 < input.size() && input[i + 2] == '(') {
                const auto [value, nextIdx] = expandArithmetic(i + 2);
                output += value;
                i = nextIdx + 1;
                continue;
            }
            if (i + 1 < input.size() && input[i + 1] == '(') {
                const auto [value, nextIdx] = expandCommandSubst(i + 1);
                output += value;
                i = nextIdx;
                continue;
            }
            if (i + 1 < input.size() && input[i + 1] == '{') {
                if (const std::size_t endBrace = input.find('}', i + 2); endBrace != std::string::npos) {
                    const std::string varExpr = input.substr(i + 2, endBrace - i - 2);
                    const std::size_t colonDash = varExpr.find(":-");
                    const std::string varName = colonDash == std::string::npos ? varExpr : varExpr.substr(0, colonDash);
                    const std::string defaultValue = colonDash == std::string::npos ? "" : varExpr.substr(colonDash + 2);

                    if (const char* varValue = getenv(varName.c_str())) {
                        output += varValue;
                    } else if (options && options->nounset) {
                        throw std::runtime_error("unset variable: " + varName);
                    } else {
                        output += defaultValue;
                    }
                    i = endBrace + 1;
                    continue;
                }
            } else {
                std::size_t end = i + 1;
                while (end < input.size() && (std::isalnum(static_cast<unsigned char>(input[end])) || input[end] == '_')) {
                    ++end;
                }
                const std::string varName = input.substr(i + 1, end - i - 1);
                if (const char* varValue = getenv(varName.c_str())) {
                    output += varValue;
                } else if (options && options->nounset) {
                    throw std::runtime_error("unset variable: " + varName);
                }
                i = end;
                continue;
            }
        }

        output.push_back(c);
        ++i;
    }

    return output;
}

void displaySplashArt() {
    std::cout << "\033[1;34m"
              << " __________          __              _________.__             .__   .__   \n"
              << " \\______   \\ ___.__.|  | __  ____   /   _____/|  |__    ____  |  |  |  |  \n"
              << "  |       _/<   |  ||  |/ /_/ __ \\  \\_____  \\ |  |  \\ _/ __ \\ |  |  |  |  \n"
              << "  |    |   \\ \\___  ||    < \\  ___/  /        \\|   Y  \\\\  ___/ |  |__|  |__\n"
              << "  |____|_  / / ____||__|_ \\ \\___  >/_______  /|___|  / \\___  >|____/|____/\n"
              << "         \\/  \\/          \\/     \\/         \\/      \\/      \\/             \n"
              << "\033[0m" << std::endl;
}

} // namespace ryke
