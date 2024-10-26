#include "autocomplete.h"
#include "ryke_shell.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <unistd.h>
#include <dirent.h>
#include <cstring>
#include <cctype>
#include <sys/stat.h>

#include "input.h"

//Helper function to convert a string to lowercase
std::string toLowerCase(const std::string& str) {
    std::string lowerStr;
    lowerStr.reserve(str.length());
    for (const char c : str) {
        lowerStr += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return lowerStr;
}

//Helper function to check if string starts with prefix (case-insensitive)
bool startsWithCaseInsensitive(const std::string& str, const std::string& prefix) {
    const std::string lowerStr = toLowerCase(str);
    const std::string lowerPrefix = toLowerCase(prefix);
    return lowerStr.find(lowerPrefix) == 0;
}

void handleAutoComplete(std::string& input, std::string& currentWord, size_t& cursorPos) {
    //Determine the prefix to use for auto-completion
    size_t wordStart = cursorPos;
    while (wordStart > 0 && !isspace(input[wordStart - 1])) {
        wordStart--;
    }
    currentWord = input.substr(wordStart, cursorPos - wordStart);
    const std::string prefix = currentWord;
    std::vector<std::string> matches;

    //If current word contains '/', treat it as a filename (with path)
    if (currentWord.find('/') != std::string::npos) {
        matches = getFilenames(prefix);
    } else if (wordStart == 0) {
        matches = getExecutableNames(prefix);
    } else {
        matches = getFilenames(prefix);
    }

    if (matches.empty()) {
        //Do nothing
    } else if (matches.size() == 1) {
        const std::string& match = matches[0]; //Use reference to maintain correct casing

        //Replace currentWord with match in input
        input.erase(wordStart, cursorPos - wordStart);
        input.insert(wordStart, match);

        //Update cursorPos
        cursorPos = wordStart + match.length();

        //Update currentWord
        currentWord = match;
    } else {
        //Multiple matches, display options
        std::cout << std::endl;

        //Determine column width
        size_t maxLen = 0;
        for (const auto& match : matches) {
            maxLen = std::max(maxLen, match.length());
        }

        const size_t cols = 80 / (maxLen + 2);
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
        //Move cursor to the current position
        const size_t promptDisplayLen = getDisplayLength(getPrompt());
        const size_t cursorMove = promptDisplayLen + cursorPos;
        std::cout << "\r\033[" << cursorMove + 1 << "G";
        std::cout.flush();
    }
}

std::string getSuggestion(const std::string& currentWord, const std::string& input, size_t cursorPos) {
    if (currentWord.empty()) {
        return "";
    }

    std::vector<std::string> matches;

    //If current word contains '/', treat it as a filename (with path)
    if (currentWord.find('/') != std::string::npos) {
        matches = getFilenames(currentWord);
    } else if (cursorPos - currentWord.length() == 0) {
        matches = getExecutableNames(currentWord);
    } else {
        matches = getFilenames(currentWord);
    }

    if (matches.size() == 1) {
        return matches[0]; //Return the full suggestion with correct casing
    }
    return "";
}

std::vector<std::string> getExecutableNames(const std::string& prefix) {
    std::vector<std::string> executables;
    const char* pathEnv = getenv("PATH");
    if (!pathEnv) {
        return executables;
    }

    std::istringstream iss(pathEnv);
    std::string dir;
    while (std::getline(iss, dir, ':')) {
        DIR* dp = opendir(dir.c_str());
        if (!dp) {
            continue;
        }
        dirent* entry;
        while ((entry = readdir(dp)) != nullptr) {
            std::string name = entry->d_name;
            if (startsWithCaseInsensitive(name, prefix)) {
                std::string filepath = dir + "/" + name;
                if (access(filepath.c_str(), X_OK) == 0) {
                    executables.push_back(name); //Store the executable name with original casing
                }
            }
        }
        closedir(dp);
    }

    //Remove duplicates (case-insensitive) and sort
    std::ranges::sort(executables, [](const std::string& a, const std::string& b) {
        return toLowerCase(a) < toLowerCase(b);
    });
    executables.erase(std::ranges::unique(executables, [](const std::string& a, const std::string& b) {
        return toLowerCase(a) == toLowerCase(b);
    }).begin(), executables.end());

    return executables;
}

std::vector<std::string> getFilenames(const std::string& prefix) {
    std::vector<std::string> filenames;

    std::string dir, filePrefix;

    //Check if prefix contains '/'

    if (const size_t slashPos = prefix.find_last_of("/"); slashPos != std::string::npos) {
        //Prefix contains '/', split into directory and file prefix
        dir = prefix.substr(0, slashPos + 1); //Includes the '/'
        filePrefix = prefix.substr(slashPos + 1);
    } else {
        //No '/', use current directory
        dir = "./";
        filePrefix = prefix;
    }

    //Now open the directory 'dir'
    DIR* dp = opendir(dir.c_str());
    if (!dp) {
        return filenames;
    }

    dirent* entry;
    while ((entry = readdir(dp)) != nullptr) {
        if (std::string name = entry->d_name; startsWithCaseInsensitive(name, filePrefix)) {
            std::string fullPath = dir + name;
            struct stat statbuf{};
            if (stat(fullPath.c_str(), &statbuf) == 0 && S_ISDIR(statbuf.st_mode)) {
                //Append '/' to directories
                filenames.push_back(fullPath + "/");
            } else {
                filenames.push_back(fullPath);
            }
        }
    }
    closedir(dp);

    //Sort filenames case-insensitively but keep original casing
    std::ranges::sort(filenames, [](const std::string& a, const std::string& b) {
        return toLowerCase(a) < toLowerCase(b);
    });

    return filenames;
}