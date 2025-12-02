#include "ryke_shell.h"

#include <cassert>
#include <functional>
#include <string>
#include <vector>

void addTest(std::string name, std::function<void()> func);

using namespace ryke;

namespace {

void test_basic_parsing() {
    CommandParser parser;
    const std::vector<Pipeline> pipelines = parser.parse(R"(echo "hello world" && ls | grep cpp > out &)");
    assert(pipelines.size() == 2);

    const Pipeline& first = pipelines[0];
    assert(first.condition == ChainCondition::None);
    assert(first.stages.size() == 1);
    assert(first.stages[0].args.size() == 2);
    assert(first.stages[0].args[1] == "hello world");
    assert(!first.background);

    const Pipeline& second = pipelines[1];
    assert(second.condition == ChainCondition::And);
    assert(second.background);
    assert(second.stages.size() == 2);
    assert(second.stages[0].args[0] == "ls");
    assert(second.stages[1].args[0] == "grep");
    assert(second.stages[1].outputFile.has_value());
    assert(*second.stages[1].outputFile == "out");
}

void test_append_and_or() {
    CommandParser parser;
    const std::vector<Pipeline> pipelines = parser.parse("cat < in.txt || echo fail >> log.txt");
    assert(pipelines.size() == 2);
    const Pipeline& first = pipelines[0];
    assert(first.stages[0].inputFile.has_value());
    assert(*first.stages[0].inputFile == "in.txt");
    const Pipeline& second = pipelines[1];
    assert(second.condition == ChainCondition::Or);
    assert(second.stages[0].appendFile.has_value());
    assert(*second.stages[0].appendFile == "log.txt");
}

void test_background_only() {
    CommandParser parser;
    const std::vector<Pipeline> pipelines = parser.parse("sleep 1 &");
    assert(pipelines.size() == 1);
    assert(pipelines[0].background);
}

} // namespace

void register_parser_tests() {
    addTest("parser basic", test_basic_parsing);
    addTest("parser append/or", test_append_and_or);
    addTest("parser background", test_background_only);
}
