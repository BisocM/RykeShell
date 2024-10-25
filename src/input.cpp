#include "input.h"
#include "autocomplete.h"
#include "ryke_shell.h"
#include <iostream>
#include <termios.h>
#include <unistd.h>
#include <cstring>
#include <vector>
#include <iomanip>
#include <sstream>
#include <cctype>

size_t commonPrefixLength(const std::string& s1, const std::string& s2) {
    const size_t len = std::min(s1.length(), s2.length());
    for (size_t i = 0; i < len; ++i) {
        if (std::tolower(static_cast<unsigned char>(s1[i])) != std::tolower(static_cast<unsigned char>(s2[i]))) {
            return i;
        }
    }
    return len;
}

std::string readInputLine() {
    std::string input;
    termios rawTermios = origTermios;

    //Disable canonical mode and echo
    rawTermios.c_lflag &= ~(ICANON | ECHO);
    rawTermios.c_cc[VMIN] = 1;
    rawTermios.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSANOW, &rawTermios) == -1) {
        perror("tcsetattr");
        return "";
    }

    char c;
    std::string currentWord;
    size_t cursorPos = 0;

    while (true) {
        ssize_t n = read(STDIN_FILENO, &c, 1);
        if (n == -1) {
            break;
        }

        if (c == '\n') {
            //Clear any existing suggestion when user presses Enter
            if (!currentWord.empty()) {
                std::string suggestion = getSuggestion(currentWord, input);
                if (!suggestion.empty()) {
                    size_t suggestionLength = suggestion.length() - currentWord.length();
                    //Move cursor to the end and clear the suggestion
                    std::cout << "\033[" << suggestionLength << "C"; //Move cursor forward
                    std::cout << "\033[K"; //Clear to end of line
                }
            }
            std::cout << std::endl;
            break;
        }

        if (c == 127 || c == '\b') {
            //Handle backspace
            if (!input.empty()) {
                input.pop_back();
                if (!currentWord.empty()) {
                    currentWord.pop_back();
                }
                //Remove character from screen
                std::cout << "\b \b";
                //Clear any existing suggestion
                std::cout << "\033[K";
                std::cout.flush();
                cursorPos--;
            }
        } else if (isprint(c)) {
            input += c;
            if (isspace(c)) {
                currentWord.clear();
                //Clear any existing suggestion
                std::cout << "\033[K";
            } else {
                currentWord += c;
            }
            std::cout << c;
            std::cout.flush();
            cursorPos++;

            //Show inline suggestion using correct casing
            if (std::string suggestion = getSuggestion(currentWord, input); !suggestion.empty()) {
                //Calculate the common prefix length
                if (const size_t prefixLen = commonPrefixLength(currentWord, suggestion); prefixLen < suggestion.length()) {
                    //Display the suggestion tail in grey
                    std::string suggestionTail = suggestion.substr(prefixLen);
                    std::cout << "\033[90m" << suggestionTail << "\033[0m";
                    //Move cursor back to after user's input
                    for (size_t i = 0; i < suggestionTail.length(); ++i) {
                        std::cout << "\b";
                    }
                    std::cout.flush();
                }
            } else {
                //No suggestion, clear to end of line
                std::cout << "\033[K";
            }
        } else if (c == '\t') {
            //Accept the suggestion if available
            if (std::string suggestion = getSuggestion(currentWord, input); !suggestion.empty()) {
                //Calculate the common prefix length
                size_t prefixLen = commonPrefixLength(currentWord, suggestion);

                //Erase the currentWord from input
                input.erase(input.length() - currentWord.length(), currentWord.length());
                input += suggestion;

                //Update currentWord
                currentWord = suggestion;

                //Clear the current line and reprint prompt and input
                std::cout << "\r\033[2K"; //Move to start of line and clear entire line
                std::cout << getPrompt() << input; //Reprint prompt and input
                std::cout.flush();

                cursorPos = input.length();
            } else {
                //No suggestion or multiple suggestions - handle accordingly.
                handleAutoComplete(input, currentWord, cursorPos);
            }
        }
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &origTermios);
    return input;
}

int interactiveListSelection(const std::vector<std::string>& items, const std::string& prompt) {
    if (items.empty()) {
        std::cout << "No items to select." << std::endl;
        return -1;
    }

    termios rawTermios = origTermios;

    rawTermios.c_lflag &= ~(ECHO | ICANON | ISIG);
    rawTermios.c_iflag &= ~(IXON | ICRNL);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &rawTermios);

    int selected = static_cast<int>(items.size()) - 1;
    const int numItems = static_cast<int>(items.size());

    //Hide cursor
    std::cout << "\033[?25l";
    std::cout << prompt << "\n";
    std::cout << "Navigate with arrow keys. Press Enter to select. Press 'q' or Esc to exit.\n";

    for (int i = 0; i < numItems; ++i) {
        if (i == selected) {
            std::cout << "\033[34m> " << items[i] << "\033[0m\n";
        } else {
            std::cout << "  " << items[i] << "\n";
        }
    }
    std::cout.flush();

    while (true) {
        if (const int key = readKey(); key == 'q' || key == '\x1b') {
            //Exit interactive mode
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &origTermios);
            //Show cursor
            std::cout << "\033[?25h";
            //Move cursor up to the top of the list
            std::cout << "\033[" << numItems + 3 << "A";
            //Clear the list lines
            for (int i = 0; i < numItems + 3; ++i) {
                std::cout << "\033[2K\033[B";
            }
            //Move cursor back up to the top
            std::cout << "\033[" << numItems + 3 << "A";
            std::cout.flush();
            return -1;
        } else {
            if (key == '\n' || key == '\r') {
                //User selected an item
                tcsetattr(STDIN_FILENO, TCSAFLUSH, &origTermios);
                //Show cursor
                std::cout << "\033[?25h";
                //Move cursor up to the top of the list
                std::cout << "\033[" << numItems + 3 << "A";
                //Clear the list lines
                for (int i = 0; i < numItems + 3; ++i) {
                    std::cout << "\033[2K\033[B";
                }
                //Move cursor back up to the top
                std::cout << "\033[" << numItems + 3 << "A";
                std::cout.flush();
                return selected;
            }
            if (key == 'k') {
                if (selected > 0) {
                    selected--;
                }
            } else if (key == 'j') {
                if (selected < numItems - 1) {
                    selected++;
                }
            }
        }

        //Move cursor back up to the top of the list
        std::cout << "\033[" << numItems << "A";

        //Redraw the list with the updated selection
        for (int i = 0; i < numItems; ++i) {
            //Clear the line
            std::cout << "\033[2K\r"; //Clear entire line and move to start
            if (i == selected) {
                std::cout << "\033[34m> " << items[i] << "\033[0m\n";
            } else {
                std::cout << "  " << items[i] << "\n";
            }
        }
        std::cout.flush();
    }
}

int readKey() {
    char c;
    while (true) {
        ssize_t n = read(STDIN_FILENO, &c, 1);
        if (n == -1) {
            return -1;
        }

        if (c == '\x1b') {
            char seq[2];
            if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
            if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

            if (seq[0] == '[') {
                if (seq[1] == 'A') return 'k'; //Up arrow
                if (seq[1] == 'B') return 'j'; //Down arrow
                if (seq[1] == 'C') return 'l'; //Right arrow
                if (seq[1] == 'D') return 'h'; //Left arrow
            }
        } else {
            return c;
        }
    }
}