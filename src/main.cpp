//main.cpp
#include <iostream>
#include <string>
#include "commands/create.hpp"
#include "commands/run.hpp"

void print_help() {
    std::cout << "flux CLI\n\n";
    std::cout << "Usage:\n";
    std::cout << "  flux create <app_name>     Create a new app\n";
    std::cout << "  flux run <platform>        Run on platform (android/windows/linux)\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_help();
        return 0;
    }

    std::string command = argv[1];

    if (command == "create") {
        if (argc < 3) {
            std::cerr << "Usage: flux create <app_name>\n";
            return 1;
        }
        return cmd_create(argv[2]);
    }
    else if (command == "run") {
        if (argc < 3) {
            std::cerr << "Usage: flux run <platform>\n";
            return 1;
        }
        return cmd_run(argv[2]);
    }
    else {
        std::cerr << "Unknown command: " << command << "\n";
        print_help();
        return 1;
    }
}