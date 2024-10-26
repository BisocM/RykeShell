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

#define ARROW_LEFT  1000
#define ARROW_RIGHT 1001
#define ARROW_UP    1002
#define ARROW_DOWN  1003
#define DEL_KEY     1004
#define HOME_KEY    1005
#define END_KEY     1006

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

    size_t cursorPos = 0;
    std::string currentWord;
    bool displaySuggestion = true;

    while (true) {
        const int c = readKey();
        if (c == -1) {
            break;
        }

        if (c == '\n') {
            std::cout << std::endl;
            break;
        }

        if (c == '\x03') { //Ctrl-C
            input.clear();
            cursorPos = 0;
            std::cout << "^C\n" << getPrompt();
            std::cout.flush();
            displaySuggestion = false;
        }

        else if (c == 127 || c == '\b') {
            //Handle backspace
            if (cursorPos > 0) {
                input.erase(cursorPos - 1, 1);
                cursorPos--;
                displaySuggestion = true; //Recompute suggestion
            }
        }

        else if (c == DEL_KEY) {
            //Handle delete key
            if (cursorPos < input.length()) {
                input.erase(cursorPos, 1);
                displaySuggestion = true; //Recompute suggestion
            }
        }

        else if (c == ARROW_LEFT) {
            if (cursorPos > 0) {
                cursorPos--;
                displaySuggestion = true; //Recompute suggestion
            }
        }

        else if (c == ARROW_RIGHT) {
            if (cursorPos < input.length()) {
                cursorPos++;
                displaySuggestion = true; //Recompute suggestion
            }
        }

        else if (isprint(c)) {
            input.insert(cursorPos, 1, c);
            cursorPos++;
            displaySuggestion = true;
        }

        else if (c == '\t') {
            //Handle autocomplete
            handleAutoComplete(input, currentWord, cursorPos);
            displaySuggestion = false; //Suggestion has been handled
            //Do not skip to next iteration; allow line to be redrawn
        }

        //Now, recompute currentWord and display suggestion if appropriate
        size_t wordStart = cursorPos;
        while (wordStart > 0 && !isspace(input[wordStart - 1])) {
            wordStart--;
        }
        currentWord = input.substr(wordStart, cursorPos - wordStart);

        //Get suggestion
        std::string suggestion;
        if (displaySuggestion && !currentWord.empty()) {
            suggestion = getSuggestion(currentWord, input, cursorPos);
        }

        //Redraw the line
        //Move cursor to the beginning of input line
        std::cout << "\r";
        //Clear the line
        std::cout << "\033[K";
        //Print the prompt
        std::cout << getPrompt();
        //Print the input
        std::cout << input;

        //If there's a suggestion, display suggestion tail in grey
        if (!suggestion.empty() && suggestion.length() > currentWord.length()) {
            std::string suggestionTail = suggestion.substr(currentWord.length());
            //Save cursor position
            std::cout << "\0337";
            //Move cursor to the position where suggestion tail starts
            const size_t promptDisplayLen = getDisplayLength(getPrompt());
            size_t suggestionPos = promptDisplayLen + cursorPos;
            std::cout << "\033[" << suggestionPos + 1 << "G";
            //Display the suggestion tail in grey
            std::cout << "\033[90m" << suggestionTail << "\033[0m";
            //Restore cursor position
            std::cout << "\0338";
        }

        //Move cursor back to the correct position
        const size_t promptDisplayLen = getDisplayLength(getPrompt());
        const size_t cursorMove = promptDisplayLen + cursorPos;
        std::cout << "\r\033[" << cursorMove + 1 << "G";
        std::cout.flush();
        displaySuggestion = true; //Reset for next iteration
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &origTermios);
    return input;
}

size_t getDisplayLength(const std::string& str) {
    size_t length = 0;
    bool inEscape = false;
    for (size_t i = 0; i < str.length(); ++i) {
        if (inEscape) {
            if (str[i] == 'm') {
                inEscape = false;
            }
        } else {
            if (str[i] == '\033') {
                inEscape = true;
            } else {
                length++;
            }
        }
    }
    return length;
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
        int key = readKey();
        if (key == 'q' || key == '\x1b') {
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
        } else if (key == '\n' || key == '\r') {
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
        } else if (key == ARROW_UP || key == 'k') {
            if (selected > 0) {
                selected--;
            }
        } else if (key == ARROW_DOWN || key == 'j') {
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
                std::cout << "\033[34m> " << items[i] << "\033[0m\n";
            } else {
                std::cout << "  " << items[i] << "\n";
            }
        }
        std::cout.flush();
    }
}

int readKey() {
    int nread;
    char c;
    char seq[3];

    while ((nread = read(STDIN_FILENO, &c, 1)) != -1) {
        if (c == '\x1b') {
            if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
            if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

            if (seq[0] == '[') {
                switch (seq[1]) {
                    case 'A': return ARROW_UP;
                    case 'B': return ARROW_DOWN;
                    case 'C': return ARROW_RIGHT;
                    case 'D': return ARROW_LEFT;
                    case '3': {
                        if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
                        if (seq[2] == '~') return DEL_KEY;
                        break;
                    }
                    case 'H': return HOME_KEY;
                    case 'F': return END_KEY;
                    default: ;
                }
            }
        } else {
            return c;
        }
    }
    return -1;
}