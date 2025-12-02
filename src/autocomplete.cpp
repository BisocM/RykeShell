#include "autocomplete.h"
#include "ryke_shell.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <dirent.h>
#include <ranges>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace {

struct WordInfo {
    std::string text;
    std::size_t start{0};
};

WordInfo findWord(const std::string& line, std::size_t cursorPos) {
    WordInfo info{};
    if (line.empty()) {
        return info;
    }

    std::size_t start = cursorPos;
    while (start > 0 && !std::isspace(static_cast<unsigned char>(line[start - 1]))) {
        --start;
    }
    std::size_t end = cursorPos;
    while (end < line.size() && !std::isspace(static_cast<unsigned char>(line[end]))) {
        ++end;
    }

    info.start = start;
    info.text = line.substr(start, end - start);
    return info;
}

bool startsWithCaseInsensitive(const std::string& value, const std::string& prefix) {
    if (prefix.empty()) {
        return true;
    }
    if (value.size() < prefix.size()) {
        return false;
    }
    for (std::size_t i = 0; i < prefix.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(value[i])) != std::tolower(static_cast<unsigned char>(prefix[i]))) {
            return false;
        }
    }
    return true;
}

} // namespace

namespace ryke {

std::string AutocompleteEngine::inlineSuggestion(const std::string& line, std::size_t cursorPos) const {
    const auto matches = completionCandidates(line, cursorPos);
    if (matches.size() == 1) {
        return matches.front();
    }
    return "";
}

std::vector<std::string> AutocompleteEngine::completionCandidates(const std::string& line, std::size_t cursorPos) const {
    const WordInfo word = findWord(line, cursorPos);
    if (word.text.empty()) {
        return {};
    }

    const bool treatAsPath = word.text.find('/') != std::string::npos;
    if (treatAsPath) {
        return getFilenames(word.text);
    }

    if (isCommandPosition(line, word.start)) {
        return getExecutableNames(word.text);
    }

    return getFilenames(word.text);
}

bool AutocompleteEngine::isCommandPosition(const std::string& line, std::size_t wordStart) {
    if (wordStart == 0) {
        return true;
    }

    // Walk backwards to find the last non-space character before the word
    std::size_t pos = wordStart;
    while (pos > 0 && std::isspace(static_cast<unsigned char>(line[pos - 1]))) {
        --pos;
    }
    if (pos == 0) {
        return true;
    }
    const char previous = line[pos - 1];
    return previous == '|' || previous == '&';
}

std::vector<std::string> AutocompleteEngine::getExecutableNames(const std::string& prefix) {
    std::vector<std::string> executables;
    static const std::vector<std::string> builtins = {
        "cd","pwd","history","alias","prompt","theme","ls","export","jobs","fg","bg","set","source","plugin","exit","help"
    };
    for (const auto& b : builtins) {
        if (startsWithCaseInsensitive(b, prefix)) {
            executables.push_back(b);
        }
    }
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
            const std::string name = entry->d_name;
            if (!startsWithCaseInsensitive(name, prefix)) {
                continue;
            }

            const std::string filepath = dir + "/" + name;
            if (access(filepath.c_str(), X_OK) == 0) {
                executables.push_back(name);
            }
        }
        closedir(dp);
    }

    // Preserve PATH order but deduplicate case-insensitively
    std::vector<std::string> unique;
    for (const auto& ex : executables) {
        const auto lower = toLowerCase(ex);
        const bool exists = std::ranges::any_of(unique, [&](const std::string& v) { return toLowerCase(v) == lower; });
        if (!exists) {
            unique.push_back(ex);
        }
    }
    executables = std::move(unique);

    return executables;
}

std::vector<std::string> AutocompleteEngine::getFilenames(const std::string& prefix) {
    std::vector<std::string> filenames;

    std::string dir = "./";
    std::string filePrefix = prefix;
    const auto slashPos = prefix.find_last_of('/');
    const bool hasSlash = slashPos != std::string::npos;

    if (hasSlash) {
        dir = prefix.substr(0, slashPos + 1);
        filePrefix = prefix.substr(slashPos + 1);
    }

    DIR* dp = opendir(dir.c_str());
    if (!dp) {
        return filenames;
    }

    dirent* entry;
    while ((entry = readdir(dp)) != nullptr) {
        const std::string name = entry->d_name;
        if (!startsWithCaseInsensitive(name, filePrefix)) {
            continue;
        }

        const std::string fullPath = dir + name;
        struct stat st {};
        if (stat(fullPath.c_str(), &st) != 0) {
            continue;
        }

        std::string candidate = hasSlash ? fullPath : name;
        if (S_ISDIR(st.st_mode)) {
            candidate += '/';
        }
        filenames.push_back(candidate);
    }
    closedir(dp);

    std::ranges::sort(filenames, [](const std::string& a, const std::string& b) {
        const bool aDir = !a.empty() && a.back() == '/';
        const bool bDir = !b.empty() && b.back() == '/';
        if (aDir != bDir) {
            return aDir > bDir;
        }
        return toLowerCase(a) < toLowerCase(b);
    });
    const auto newEnd = std::ranges::unique(filenames, [](const std::string& a, const std::string& b) {
        return toLowerCase(a) == toLowerCase(b);
    });
    filenames.erase(newEnd.begin(), newEnd.end());

    return filenames;
}

std::string AutocompleteEngine::toLowerCase(const std::string& str) {
    std::string lower;
    lower.reserve(str.size());
    for (const char c : str) {
        lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return lower;
}

} // namespace ryke
