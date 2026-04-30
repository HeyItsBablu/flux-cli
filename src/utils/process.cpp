//src/utils/process.cpp
#include "process.hpp"
#include <iostream>
#include <cstdlib>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <unistd.h>
    #include <sys/wait.h>
#endif

int process_run(const std::string& command, const std::string& working_dir) {
#ifdef _WIN32
    // save current dir
    char prev[MAX_PATH];
    GetCurrentDirectoryA(MAX_PATH, prev);

    if (!working_dir.empty())
        SetCurrentDirectoryA(working_dir.c_str());

    int result = system(command.c_str());

    SetCurrentDirectoryA(prev);
    return result;

#else
    // save current dir
    char prev[4096];
    getcwd(prev, sizeof(prev));

    if (!working_dir.empty())
        chdir(working_dir.c_str());

    int result = system(command.c_str());

    chdir(prev);
    return result;
#endif
}