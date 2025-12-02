#include "input.h"
#include "autocomplete.h"
#include "ryke_shell.h"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <unistd.h>
#include <vector>

namespace {

enum KeyCode {
    ArrowLeft = 1000,
    ArrowRight,
    ArrowUp,
    ArrowDown,
    DeleteKey,
    HomeKey,
    EndKey
};

struct WordInfo {
    std::size_t start{0};
    std::size_t length{0};
    std::string text;
};

WordInfo currentWord(const std::string& line, std::size_t cursor) {
    WordInfo info{};
    if (line.empty()) {
        return info;
    }

    std::size_t start = cursor;
    while (start > 0 && !std::isspace(static_cast<unsigned char>(line[start - 1]))) {
        --start;
    }

    std::size_t end = cursor;
    while (end < line.size() && !std::isspace(static_cast<unsigned char>(line[end]))) {
        ++end;
    }

    info.start = start;
    info.length = end - start;
    info.text = line.substr(start, info.length);
    return info;
}

} // namespace

namespace ryke {

InputReader::InputReader(Terminal& terminal, History& history, const AutocompleteEngine& autocomplete,
                         std::function<std::string()> promptProvider)
    : terminal_(terminal),
      history_(history),
      autocomplete_(autocomplete),
      promptProvider_(std::move(promptProvider)) {}

std::string InputReader::readLine() {
    RawModeGuard guard(terminal_, false, false);

    std::string line;
    std::size_t cursor = 0;
    int historyIndex = static_cast<int>(history_.size());
    bool searching = false;
    std::string searchQuery;
    std::string searchResult;

    while (true) {
        const int key = readKey();
        if (key == -1) {
            break;
        }

        if (key == '\n') {
            if (searching) {
                line = searchResult;
                cursor = line.size();
                searching = false;
                std::cout << "\r\033[K";
                continue;
            }
            std::cout << '\n';
            break;
        }

        if (key == '\x03') { //Ctrl-C
            std::cout << "^C\n";
            line.clear();
            cursor = 0;
            break;
        }

        if (key == 0x12) { // Ctrl+R reverse search
            searching = true;
            searchQuery.clear();
            searchResult.clear();
            std::cout << "\r\033[K(reverse-i-search)`': ";
            continue;
        }

        if (searching) {
            if (key == '\b' || key == 127) {
                if (!searchQuery.empty()) {
                    searchQuery.pop_back();
                }
            } else if (key == '\x1b') { // ESC
                searching = false;
            } else if (std::isprint(key)) {
                searchQuery.push_back(static_cast<char>(key));
            }

            searchResult.clear();
            for (int idx = static_cast<int>(history_.size()) - 1; idx >= 0; --idx) {
                const auto& entry = history_.at(static_cast<std::size_t>(idx)).command;
                if (entry.find(searchQuery) != std::string::npos) {
                    searchResult = entry;
                    break;
                }
            }

            std::cout << "\r\033[K(reverse-i-search)`" << searchQuery << "': " << searchResult;
            std::cout.flush();
            continue;
        }

        if (key == 127 || key == '\b') { // Backspace
            historyIndex = static_cast<int>(history_.size());
            if (cursor > 0) {
                line.erase(cursor - 1, 1);
                --cursor;
            }
        } else if (key == DeleteKey) {
            historyIndex = static_cast<int>(history_.size());
            if (cursor < line.size()) {
                line.erase(cursor, 1);
            }
        } else if (key == 0x01) { // Ctrl-A
            cursor = 0;
        } else if (key == 0x05) { // Ctrl-E
            cursor = line.size();
        } else if (key == 0x17) { // Ctrl-W
            if (cursor > 0) {
                std::size_t start = cursor;
                while (start > 0 && std::isspace(static_cast<unsigned char>(line[start - 1]))) {
                    --start;
                }
                while (start > 0 && !std::isspace(static_cast<unsigned char>(line[start - 1]))) {
                    --start;
                }
                line.erase(start, cursor - start);
                cursor = start;
            }
        } else if (key == ArrowLeft) {
            if (cursor > 0) {
                --cursor;
            }
        } else if (key == ArrowRight) {
            if (cursor < line.size()) {
                ++cursor;
            }
        } else if (key == HomeKey) {
            cursor = 0;
        } else if (key == EndKey) {
            cursor = line.size();
        } else if (key == ArrowUp) {
            if (!history_.empty() && historyIndex > 0) {
                --historyIndex;
                line = history_.at(static_cast<std::size_t>(historyIndex)).command;
                cursor = line.size();
            }
        } else if (key == ArrowDown) {
            if (!history_.empty()) {
                if (historyIndex < static_cast<int>(history_.size()) - 1) {
                    ++historyIndex;
                    line = history_.at(static_cast<std::size_t>(historyIndex)).command;
                } else {
                    historyIndex = static_cast<int>(history_.size());
                    line.clear();
                }
                cursor = line.size();
            }
        } else if (key == '\t') { // Tab completion
            const auto word = currentWord(line, cursor);
            auto matches = autocomplete_.completionCandidates(line, cursor);
            if (matches.empty()) {
                // nothing
            } else if (matches.size() == 1) {
                const std::string& match = matches.front();
                line.replace(word.start, word.length, match);
                cursor = word.start + match.size();
            } else {
                std::cout << '\n';

                std::size_t maxLen = 0;
                for (const auto& m : matches) {
                    maxLen = std::max(maxLen, m.size());
                }
                const std::size_t cols = std::max<std::size_t>(1, 80 / (maxLen + 2));
                std::size_t count = 0;
                for (const auto& match : matches) {
                    std::cout << std::left << std::setw(static_cast<int>(maxLen + 2)) << match;
                    if (++count % cols == 0) {
                        std::cout << '\n';
                    }
                }
                if (count % cols != 0) {
                    std::cout << '\n';
                }
            }
        } else if (std::isprint(key)) {
            historyIndex = static_cast<int>(history_.size());
            line.insert(cursor, 1, static_cast<char>(key));
            ++cursor;
        }

        const auto word = currentWord(line, cursor);
        const std::string suggestion = autocomplete_.inlineSuggestion(line, cursor);
        const std::string prompt = promptProvider_();
        const std::size_t promptLen = visibleLength(prompt);

        std::cout << "\r\033[K" << prompt << line;
        if (!suggestion.empty() && suggestion.size() > word.text.size()) {
            const std::string tail = suggestion.substr(word.text.size());
            std::cout << "\0337"; // save cursor
            const std::size_t suggestionPos = promptLen + cursor;
            std::cout << "\033[" << suggestionPos + 1 << "G";
            std::cout << "\033[90m" << tail << "\033[0m";
            std::cout << "\0338"; // restore cursor
        }

        const std::size_t cursorPos = promptLen + cursor;
        std::cout << "\r\033[" << cursorPos + 1 << "G";
        std::cout.flush();
    }

    return line;
}

int InputReader::interactiveListSelection(const std::vector<std::string>& items, const std::string& prompt) {
    if (items.empty()) {
        std::cout << "No items to select.\n";
        return -1;
    }

    RawModeGuard guard(terminal_, false, false);

    int selected = static_cast<int>(items.size()) - 1;
    const int numItems = static_cast<int>(items.size());

    std::cout << "\033[?25l";
    std::cout << prompt << "\n";
    std::cout << "Navigate with arrow keys. Press Enter to select. Press 'q' or Esc to exit.\n";

    auto renderList = [&](int currentSelection) {
        for (int i = 0; i < numItems; ++i) {
            if (i == currentSelection) {
                std::cout << "\033[34m> " << items[i] << "\033[0m\n";
            } else {
                std::cout << "  " << items[i] << "\n";
            }
        }
        std::cout.flush();
    };

    renderList(selected);

    while (true) {
        const int key = readKey();
        if (key == 'q' || key == '\x1b') {
            std::cout << "\033[?25h";
            std::cout << "\033[" << numItems + 3 << "A";
            for (int i = 0; i < numItems + 3; ++i) {
                std::cout << "\033[2K\033[B";
            }
            std::cout << "\033[" << numItems + 3 << "A";
            std::cout.flush();
            return -1;
        }
        if (key == '\n' || key == '\r') {
            std::cout << "\033[?25h";
            std::cout << "\033[" << numItems + 3 << "A";
            for (int i = 0; i < numItems + 3; ++i) {
                std::cout << "\033[2K\033[B";
            }
            std::cout << "\033[" << numItems + 3 << "A";
            std::cout.flush();
            return selected;
        }
        if (key == ArrowUp || key == 'k') {
            if (selected > 0) {
                --selected;
            }
        } else if (key == ArrowDown || key == 'j') {
            if (selected < numItems - 1) {
                ++selected;
            }
        }

        std::cout << "\033[" << numItems << "A";
        renderList(selected);
    }
}

std::size_t InputReader::visibleLength(const std::string& text) {
    std::size_t length = 0;
    bool inEscape = false;
    for (const char c : text) {
        if (inEscape) {
            if (c == 'm') {
                inEscape = false;
            }
        } else {
            if (c == '\033') {
                inEscape = true;
            } else {
                ++length;
            }
        }
    }
    return length;
}

int InputReader::readKey() {
    int nread;
    char c;
    char seq[3];

    while ((nread = ::read(STDIN_FILENO, &c, 1)) != -1) {
        if (c == '\x1b') {
            if (::read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
            if (::read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

            if (seq[0] == '[') {
                switch (seq[1]) {
                    case 'A': return ArrowUp;
                    case 'B': return ArrowDown;
                    case 'C': return ArrowRight;
                    case 'D': return ArrowLeft;
                    case '3': {
                        if (::read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
                        if (seq[2] == '~') return DeleteKey;
                        break;
                    }
                    case 'H': return HomeKey;
                    case 'F': return EndKey;
                    default: break;
                }
            }
        } else {
            return c;
        }
    }
    return -1;
}

} // namespace ryke
