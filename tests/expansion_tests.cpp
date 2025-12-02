#include "utils.h"
#include "ryke_shell.h"

#include <cassert>
#include <cstdlib>
#include <functional>
#include <string>

void addTest(std::string name, std::function<void()> func);

using namespace ryke;

namespace {

void test_variable_expansion() {
    setenv("RYKE_TEST_VAR", "value", 1);
    const std::string expanded = expandVariables("echo $RYKE_TEST_VAR", nullptr);
    assert(expanded == "echo value");
}

void test_default_expansion() {
    unsetenv("RYKE_TEST_MISSING");
    const std::string expanded = expandVariables("echo ${RYKE_TEST_MISSING:-fallback}", nullptr);
    assert(expanded == "echo fallback");
}

void test_quote_rules() {
    setenv("RYKE_TEST_QUOTE", "yes", 1);
    const std::string single = expandVariables("echo '$RYKE_TEST_QUOTE'", nullptr);
    assert(single == "echo '$RYKE_TEST_QUOTE'");
    const std::string dbl = expandVariables("echo \"$RYKE_TEST_QUOTE\"", nullptr);
    assert(dbl == "echo \"yes\"");
}

void test_tilde_rules() {
    setenv("HOME", "/tmp/rykehome", 1);
    const std::string expanded = expandVariables("~/work", nullptr);
    assert(expanded == "/tmp/rykehome/work");
    const std::string quoted = expandVariables("'~'/work", nullptr);
    assert(quoted == "'~'/work");
}

void test_command_substitution() {
    const std::string expanded = expandVariables("val=$(printf hi)", nullptr);
    assert(expanded == "val=hi");
}

void test_arithmetic_substitution() {
    const std::string expanded = expandVariables("echo $((2+3))", nullptr);
    assert(expanded == "echo 5");
}

void test_nounset_option() {
    ShellOptions opts;
    opts.nounset = true;
    bool threw = false;
    try {
        expandVariables("echo $UNDEFINED_VAR", &opts);
    } catch (...) {
        threw = true;
    }
    assert(threw);
}

} // namespace

void register_expansion_tests() {
    addTest("expand variables", test_variable_expansion);
    addTest("expand default", test_default_expansion);
    addTest("expand quotes", test_quote_rules);
    addTest("expand tilde", test_tilde_rules);
    addTest("expand command subst", test_command_substitution);
    addTest("expand arithmetic", test_arithmetic_substitution);
    addTest("expand nounset throws", test_nounset_option);
}
