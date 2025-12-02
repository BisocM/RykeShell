#include "ryke_shell.h"

#include <cctype>
#include <functional>
#include <sstream>

namespace ryke {

namespace {

std::vector<std::string> splitFields(const std::string& token, const std::string& ifs = " \t\n") {
    std::vector<std::string> fields;
    std::string current;
    for (char c : token) {
        if (ifs.find(c) != std::string::npos) {
            if (!current.empty()) {
                fields.push_back(current);
                current.clear();
            }
        } else {
            current.push_back(c);
        }
    }
    if (!current.empty()) {
        fields.push_back(current);
    }
    if (fields.empty()) {
        fields.push_back("");
    }
    return fields;
}

} // namespace

std::vector<CommandParser::Token> CommandParser::tokenize(const std::string& input) const {
    std::vector<Token> tokens;
    std::string current;
    bool inSingleQuotes = false;
    bool inDoubleQuotes = false;
    bool escaping = false;
    bool tokenQuoted = false;

    auto flushCurrent = [&]() {
        if (!current.empty()) {
            tokens.push_back(Token{current, tokenQuoted});
            current.clear();
            tokenQuoted = false;
        }
    };

    for (std::size_t i = 0; i < input.size(); ++i) {
        const char c = input[i];

        if (escaping) {
            current.push_back(c);
            escaping = false;
            continue;
        }

        if (c == '\\') {
            escaping = true;
            continue;
        }

        if (c == '\'' && !inDoubleQuotes) {
            inSingleQuotes = !inSingleQuotes;
            tokenQuoted = true;
            continue;
        }
        if (c == '"' && !inSingleQuotes) {
            inDoubleQuotes = !inDoubleQuotes;
            tokenQuoted = true;
            continue;
        }

        if (!inSingleQuotes && !inDoubleQuotes) {
            if (std::isspace(static_cast<unsigned char>(c))) {
                flushCurrent();
                continue;
            }

            const auto twoChar = [&](char next, const std::string& tokenText) {
                if (i + 1 < input.size() && input[i + 1] == next) {
                    flushCurrent();
                    tokens.emplace_back(Token{tokenText, false});
                    ++i;
                    return true;
                }
                return false;
            };

            if (twoChar('&', "&&") || twoChar('|', "||") || twoChar('>', ">>")) {
                continue;
            }

            if (i + 1 < input.size() && (std::isdigit(static_cast<unsigned char>(c)) && input[i + 1] == '>')) {
                flushCurrent();
                std::string t;
                t.push_back(c);
                t.push_back('>');
                tokens.emplace_back(Token{t, false});
                ++i;
                continue;
            }

            if (i + 2 < input.size() && std::isdigit(static_cast<unsigned char>(c)) && input[i + 1] == '>' && input[i + 2] == '>') {
                flushCurrent();
                std::string t;
                t.push_back(c);
                t.append(">>");
                tokens.emplace_back(Token{t, false});
                i += 2;
                continue;
            }

            if (i + 1 < input.size() && c == '|' && input[i + 1] == '&') {
                flushCurrent();
                tokens.emplace_back(Token{"|&", false});
                ++i;
                continue;
            }
            if (i + 1 < input.size() && c == '&' && input[i + 1] == '>') {
                flushCurrent();
                tokens.emplace_back(Token{"&>", false});
                ++i;
                continue;
            }
            if (i + 1 < input.size() && c == '<' && input[i + 1] == '<') {
                if (i + 2 < input.size() && input[i + 2] == '<') {
                    flushCurrent();
                    tokens.emplace_back(Token{"<<<", false});
                    i += 2;
                    continue;
                }
                if (i + 2 < input.size() && input[i + 2] == '-') {
                    flushCurrent();
                    tokens.emplace_back(Token{"<<-", false});
                    i += 2;
                    continue;
                }
                flushCurrent();
                tokens.emplace_back(Token{"<<", false});
                ++i;
                continue;
            }
            if (c == '|' || c == '<' || c == '>' || c == '&') {
                flushCurrent();
                tokens.emplace_back(Token{std::string(1, c), false});
                continue;
            }
        }

        current.push_back(c);
    }

    flushCurrent();
    return tokens;
}

std::vector<Pipeline> CommandParser::parse(const std::string& input) const {
    const auto rawTokens = expandBraces(tokenize(input));
    std::vector<Pipeline> pipelines;

    Pipeline pipeline;
    Command command;
    ChainCondition pendingCondition = ChainCondition::None;

    auto flushCommand = [&]() {
        if (!command.args.empty() || command.inputFile || command.outputFile || command.appendFile ||
            command.stderrFile || command.stderrAppendFile || command.heredocDelimiter || command.hereString) {
            pipeline.stages.push_back(command);
        }
        command = Command{};
    };

    auto flushPipeline = [&]() {
        flushCommand();
        if (!pipeline.stages.empty()) {
            pipeline.condition = pendingCondition;
            pipelines.push_back(pipeline);
            pipeline = Pipeline{};
        }
        pendingCondition = ChainCondition::None;
    };

    for (std::size_t i = 0; i < rawTokens.size(); ++i) {
        const std::string& token = rawTokens[i].text;
        const bool tokenQuoted = rawTokens[i].quoted;

        if (token == "|") {
            flushCommand();
            continue;
        }

        if (token == "|&") {
            command.mergeStderr = true;
            flushCommand();
            continue;
        }

        if (token == "&&" || token == "||") {
            flushPipeline();
            pendingCondition = (token == "&&") ? ChainCondition::And : ChainCondition::Or;
            continue;
        }

        if (token == "<") {
            if (i + 1 < rawTokens.size()) {
                command.inputFile = rawTokens[++i].text;
            }
            continue;
        }

        if (token == ">") {
            if (i + 1 < rawTokens.size()) {
                command.outputFile = rawTokens[++i].text;
                command.appendFile.reset();
            }
            continue;
        }

        if (token == ">>") {
            if (i + 1 < rawTokens.size()) {
                command.appendFile = rawTokens[++i].text;
                command.outputFile.reset();
            }
            continue;
        }

        if (token == "2>") {
            if (i + 1 < rawTokens.size()) {
                command.stderrFile = rawTokens[++i].text;
                command.stderrAppendFile.reset();
            }
            continue;
        }

        if (token == "2>>") {
            if (i + 1 < rawTokens.size()) {
                command.stderrAppendFile = rawTokens[++i].text;
                command.stderrFile.reset();
            }
            continue;
        }

        if (token == "&>") {
            if (i + 1 < rawTokens.size()) {
                command.outputFile = rawTokens[++i].text;
                command.stderrFile = command.outputFile;
                command.appendFile.reset();
                command.stderrAppendFile.reset();
            }
            continue;
        }

        if (token == "<<") {
            if (i + 1 < rawTokens.size()) {
                command.heredocDelimiter = rawTokens[++i].text;
                command.heredocExpand = !rawTokens[i].quoted;
            }
            continue;
        }

        if (token == "<<-") {
            if (i + 1 < rawTokens.size()) {
                command.heredocDelimiter = rawTokens[++i].text;
                command.heredocStripTabs = true;
                command.heredocExpand = !rawTokens[i].quoted;
            }
            continue;
        }

        if (token == "<<<") {
            if (i + 1 < rawTokens.size()) {
                command.hereString = rawTokens[++i].text;
            }
            continue;
        }

        if (!token.empty() && std::isdigit(static_cast<unsigned char>(token[0])) && token.ends_with('>')) {
            int fd = token[0] - '0';
            if (token == "2>" || token == "1>" || token.size() == 2) {
                if (i + 1 < rawTokens.size()) {
                    command.fdRedirections.push_back(Command::FdRedirection{fd, Command::FdRedirection::Type::Truncate, rawTokens[++i].text, fd});
                }
                continue;
            }
        }

        if (token.size() >= 3 && std::isdigit(static_cast<unsigned char>(token[0])) && token[1] == '>' && token[2] == '>') {
            int fd = token[0] - '0';
            if (i + 1 < rawTokens.size()) {
                command.fdRedirections.push_back(Command::FdRedirection{fd, Command::FdRedirection::Type::Append, rawTokens[++i].text, fd});
            }
            continue;
        }

        if (token == "2>" || token == "1>") {
            int fd = token[0] - '0';
            if (i + 2 < rawTokens.size() && rawTokens[i + 1].text == "&" && !rawTokens[i + 2].text.empty() && std::isdigit(static_cast<unsigned char>(rawTokens[i + 2].text[0]))) {
                command.fdRedirections.push_back(Command::FdRedirection{fd, Command::FdRedirection::Type::Dup, "", rawTokens[i + 2].text[0] - '0'});
                i += 2;
                continue;
            }
        }

        if (token == ">" && i + 2 < rawTokens.size() && rawTokens[i + 1].text == "&" && !rawTokens[i + 2].text.empty() && std::isdigit(static_cast<unsigned char>(rawTokens[i + 2].text[0]))) {
            command.fdRedirections.push_back(Command::FdRedirection{1, Command::FdRedirection::Type::Dup, "", rawTokens[i + 2].text[0] - '0'});
            i += 2;
            continue;
        }

        if (token == "&") {
            pipeline.background = true;
            continue;
        }

        if (tokenQuoted) {
            command.args.push_back(token);
        } else {
        const char* ifsEnv = getenv("IFS");
        const std::string ifs = ifsEnv ? std::string(ifsEnv) : std::string(" \t\n");
        const auto fields = splitFields(token, ifs);
        command.args.insert(command.args.end(), fields.begin(), fields.end());
    }
    }

    flushPipeline();

    return pipelines;
}

std::vector<CommandParser::Token> CommandParser::expandBraces(const std::vector<Token>& tokens) const {
    std::vector<Token> result;
    for (const auto& tokenObj : tokens) {
        const auto& token = tokenObj.text;
        const auto lbrace = token.find('{');
        const auto rbrace = token.find('}');
        if (lbrace != std::string::npos && rbrace != std::string::npos && rbrace > lbrace) {
            const std::string before = token.substr(0, lbrace);
            const std::string inside = token.substr(lbrace + 1, rbrace - lbrace - 1);
            const std::string after = token.substr(rbrace + 1);

            const auto dots = inside.find("..");
            if (dots != std::string::npos) {
                try {
                    int start = std::stoi(inside.substr(0, dots));
                    int end = std::stoi(inside.substr(dots + 2));
                    const int step = (start <= end) ? 1 : -1;
                    for (int v = start; step > 0 ? v <= end : v >= end; v += step) {
                        result.push_back(Token{before + std::to_string(v) + after, tokenObj.quoted});
                    }
                    continue;
                } catch (...) {
                    // fallthrough
                }
            }

            std::stringstream ss(inside);
            std::string part;
            while (std::getline(ss, part, ',')) {
                result.push_back(Token{before + part + after, tokenObj.quoted});
            }
        } else {
            result.push_back(tokenObj);
        }
    }
    return result;
}

std::string CommandParser::unescape(const std::string& token) const {
    std::string out;
    bool escape = false;
    for (char c : token) {
        if (escape) {
            out.push_back(c);
            escape = false;
        } else if (c == '\\') {
            escape = true;
        } else {
            out.push_back(c);
        }
    }
    return out;
}

} // namespace ryke
