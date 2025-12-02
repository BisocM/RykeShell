#include <functional>
#include <iostream>
#include <string>
#include <vector>

struct TestCase {
    std::string name;
    std::function<void()> func;
};

std::vector<TestCase>& testRegistry() {
    static std::vector<TestCase> tests;
    return tests;
}

void addTest(std::string name, std::function<void()> func) {
    testRegistry().push_back(TestCase{std::move(name), std::move(func)});
}

void register_parser_tests();
void register_executor_tests();
void register_expansion_tests();

int main() {
    std::cerr << "[TESTS] starting\n";
    register_parser_tests();
    register_executor_tests();
    register_expansion_tests();

    int failures = 0;
    for (const auto& test : testRegistry()) {
        std::cout << "[RUN ] " << test.name << std::endl;
        try {
            test.func();
            std::cout << "[PASS] " << test.name << '\n';
        } catch (const std::exception& ex) {
            std::cout << "[FAIL] " << test.name << " - " << ex.what() << '\n';
            ++failures;
        } catch (...) {
            std::cout << "[FAIL] " << test.name << " - unknown exception\n";
            ++failures;
        }
    }

    std::cout << "Ran " << testRegistry().size() << " tests, failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
