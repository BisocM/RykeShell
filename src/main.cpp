#include "ryke_shell.h"

#include <exception>
#include <iostream>

int main(int argc, char** argv) {
    try {
        ryke::Shell shell{};
        if (argc > 1) {
            return shell.runScript(argv[1]);
        }
        return shell.run();
    } catch (const std::exception& ex) {
        std::cerr << "Fatal error: " << ex.what() << '\n';
        return 1;
    }
}
