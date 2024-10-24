#include "input.h"
#include "utils.h"
#include <iostream>
#include <termios.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <cstring>
#include <vector>
#include <algorithm>
#include <iomanip>
#include <cstdlib>

//Read a line of input from the user, with basic auto-completion
std::string readInputLine() {
    std::string input;
    termios origTermios{}, rawTermios{};
    tcgetattr(STDIN_FILENO, &origTermios);
    rawTermios = origTermios;

    rawTermios.c_lflag &= ~(ICANON | ECHO); //Disable canonical mode and echo
    rawTermios.c_cc[VMIN] = 1;
    rawTermios.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &rawTermios);

    char c;
    std::string currentWord;
    size_t cursorPos = 0;

    while (true) {
        ssize_t n = read(STDIN_FILENO, &c, 1);
        if (n == -1) {
            break;
        }

        if (c == '\n') {
            std::cout << std::endl;
            break;
        } else if (c == '\t') {
            //Auto-complete
            std::string prefix = currentWord;

            std::vector<std::string> matches;
            if (input.empty() || input.back() == ' ') {
                //If at the beginning or after a space, complete command names
                matches = getExecutableNames(prefix);
            } else {
                //Otherwise, complete filenames
                matches = getFilenames(prefix);
            }
            if (matches.empty()) {
                //No matches, do nothing
                continue;
            } else if (matches.size() == 1) {
                //Single match, auto-complete
                std::string completion = matches[0].substr(prefix.length());
                input += completion;
                currentWord += completion;
                std::cout << completion;
                cursorPos += completion.length();
            } else {
                //Multiple matches, display options
                std::cout << std::endl;
                //Determine column width
                size_t maxLen = 0;
                for (const auto& match : matches) {
                    if (match.length() > maxLen) {
                        maxLen = match.length();
                    }
                }
                size_t cols = 80 / (maxLen + 2);
                size_t count = 0;
                for (const auto& match : matches) {
                    std::cout << std::left << std::setw(maxLen + 2) << match;
                    if (++count % cols == 0) {
                        std::cout << std::endl;
                    }
                }
                if (count % cols != 0) {
                    std::cout << std::endl;
                }
                //Re-display prompt and input
                std::cout << getPrompt() << input;
                std::cout.flush();
            }
        } else if (c == 127 || c == '\b') {
            //Handle backspace
            if (!input.empty()) {
                input.pop_back();
                if (!currentWord.empty()) {
                    currentWord.pop_back();
                }
                //Move cursor back, overwrite character with space, move back again
                std::cout << "\b \b";
                cursorPos--;
            }
        } else if (isprint(c)) {
            input += c;
            if (isspace(c)) {
                currentWord.clear();
            } else {
                currentWord += c;
            }
            std::cout << c;
            cursorPos++;
        }
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &origTermios);
    return input;
}

// Get list of executable names starting with a prefix
std::vector<std::string> getExecutableNames(const std::string& prefix) {
    std::vector<std::string> executables;
    const char* pathEnv = getenv("PATH");
    if (!pathEnv) {
        return executables;
    }

    std::string pathEnvStr = pathEnv;
    std::istringstream iss(pathEnvStr);
    std::string dir;
    while (std::getline(iss, dir, ':')) {
        DIR* dp = opendir(dir.c_str());
        if (dp == nullptr) {
            continue;
        }
        struct dirent* entry;
        while ((entry = readdir(dp)) != nullptr) {
            if (entry->d_type == DT_REG || entry->d_type == DT_LNK ||
                entry->d_type == DT_UNKNOWN) {
                std::string name = entry->d_name;
                if (name.find(prefix) == 0) {
                    std::string filepath = dir + "/" + name;
                    if (access(filepath.c_str(), X_OK) == 0) {
                        executables.push_back(name);
                    }
                }
            }
        }
        closedir(dp);
    }
    std::ranges::sort(executables);
    executables.erase(std::ranges::unique(executables).begin(), executables.end());
    return executables;
}

// Get list of filenames starting with a prefix
std::vector<std::string> getFilenames(const std::string& prefix) {
    std::vector<std::string> filenames;
    DIR* dp = opendir(".");
    if (dp == nullptr) {
        return filenames;
    }
    struct dirent* entry;
    while ((entry = readdir(dp)) != nullptr) {
        if (std::string name = entry->d_name; name.find(prefix) == 0) {
            filenames.push_back(name);
        }
    }
    closedir(dp);
    std::ranges::sort(filenames);
    return filenames;
}

//Interactive list selection function
int interactiveListSelection(const std::vector<std::string>& items, const std::string& prompt) {
    if (items.empty()) {
        std::cout << "No items to select." << std::endl;
        return -1;
    }

    termios origTermios{}, rawTermios{};
    tcgetattr(STDIN_FILENO, &origTermios);
    rawTermios = origTermios;

    //Enable raw mode
    rawTermios.c_lflag &= ~(ECHO | ICANON | ISIG);
    rawTermios.c_iflag &= ~(IXON | ICRNL);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &rawTermios);

    int selected = static_cast<int>(items.size()) - 1; //Start from the most recent item
    int numItems = static_cast<int>(items.size());

    //Hide cursor
    std::cout << "\033[?25l";
    //Print the initial prompt and instructions
    std::cout << prompt << "\n";
    std::cout << "Navigate with arrow keys. Press Enter to select. Press 'q' or Esc to exit.\n";

    //Display the initial list of items
    for (int i = 0; i < numItems; ++i) {
        if (i == selected) {
            //Highlight the selected item
            std::cout << "\033[34m"; //Dark blue text
            std::cout << "> " << items[i] << "\033[0m"; //Reset formatting
        } else {
            std::cout << "  " << items[i];
        }
        std::cout << "\n";
    }

    //Flush output
    std::cout.flush();

    while (true) {
        //Read user input
        int key = readKey();
        if (key == 'q' || key == '\x1b') {
            //Exit interactive mode
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &origTermios);
            //Show cursor
            std::cout << "\033[?25h";
            //Move cursor up to the top of the list
            std::cout << "\033[" << numItems << "A";
            //Clear the list lines
            for (int i = 0; i < numItems; ++i) {
                std::cout << "\033[2K\033[B";
            }
            //Move cursor back up to the position after the instructions
            std::cout << "\033[" << numItems << "A";
            //Optionally clear instructions and prompt
            std::cout << "\033[2K";
            std::cout << "\033[1A\033[2K";
            std::cout << "\033[1A\033[2K";

            //Move cursor to the bottom
            std::cout << "\033[" << (numItems + 3) << "B";

            std::cout.flush();
            return -1;
        } else if (key == '\n' || key == '\r') {
            //User selected an item
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &origTermios);
            //Show cursor
            std::cout << "\033[?25h";
            //Move cursor up to the top of the list
            std::cout << "\033[" << numItems << "A";
            //Clear the list lines
            for (int i = 0; i < numItems; ++i) {
                std::cout << "\033[2K\033[B";
            }
            //Move cursor back up to the position after the instructions
            std::cout << "\033[" << numItems << "A";
            //Optionally clear instructions and prompt
            std::cout << "\033[2K";
            std::cout << "\033[1A\033[2K";
            std::cout << "\033[1A\033[2K";

            //Move cursor to the bottom
            std::cout << "\033[" << (numItems + 3) << "B";

            std::cout.flush();
            return selected;
        } else if (key == 'k') { //Up arrow
            if (selected > 0) {
                selected--;
            }
        } else if (key == 'j') { //Down arrow
            if (selected < numItems - 1) {
                selected++;
            }
        }

        //Move cursor back up to the top of the list
        std::cout << "\033[" << numItems << "A";

        //Redraw the list with the updated selection
        for (int i = 0; i < numItems; ++i) {
            //Clear the line
            std::cout << "\033[2K\r"; //Clear entire line and move to start
            if (i == selected) {
                //Highlight the selected item
                std::cout << "\033[34m"; //Dark blue text
                std::cout << "> " << items[i] << "\033[0m";
            } else {
                std::cout << "  " << items[i];
            }
            std::cout << "\n";
        }

        //Flush output
        std::cout.flush();
    }
}

//Read a key press and handle escape sequences
int readKey() {
    char c;
    while (true) {
        ssize_t n = read(STDIN_FILENO, &c, 1);
        if (n == -1) return -1;

        if (c == '\x1b') { //Escape sequence
            char seq[3];
            if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
            if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

            if (seq[0] == '[') {
                if (seq[1] == 'A') return 'k'; //Up arrow
                else if (seq[1] == 'B') return 'j'; //Down arrow
                else if (seq[1] == 'C') return 'l'; //Right arrow
                else if (seq[1] == 'D') return 'h'; //Left arrow
            }
            continue;
        } else {
            return c;
        }
    }
}