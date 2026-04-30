//src/commands/run.cpp
#include "run.hpp"
#include "../utils/process.hpp"
#include "../utils/fs.hpp"
#include "../utils/config.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

#ifdef _WIN32
static std::string find_vsdevcmd() {
    std::vector<std::string> paths = {
        "C:\\Program Files\\Microsoft Visual Studio\\18\\Community\\Common7\\Tools\\VsDevCmd.bat",
        "C:\\Program Files\\Microsoft Visual Studio\\18\\Professional\\Common7\\Tools\\VsDevCmd.bat",
        "C:\\Program Files\\Microsoft Visual Studio\\18\\Enterprise\\Common7\\Tools\\VsDevCmd.bat",
        "C:\\Program Files\\Microsoft Visual Studio\\17\\Community\\Common7\\Tools\\VsDevCmd.bat",
        "C:\\Program Files\\Microsoft Visual Studio\\17\\Professional\\Common7\\Tools\\VsDevCmd.bat",
        "C:\\Program Files\\Microsoft Visual Studio\\17\\Enterprise\\Common7\\Tools\\VsDevCmd.bat",
        "C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Community\\Common7\\Tools\\VsDevCmd.bat",
        "C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Professional\\Common7\\Tools\\VsDevCmd.bat",
        "C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Enterprise\\Common7\\Tools\\VsDevCmd.bat",
    };

    for (auto& p : paths) {
        if (fs_exists(p)) return p;
    }
    return "";
}
#endif


static std::string find_adb() {
#ifdef _WIN32
    // check PATH first
    if (system("adb version >nul 2>&1") == 0) return "adb";

    // common Android SDK locations on Windows
    std::vector<std::string> paths = {
        std::string(getenv("LOCALAPPDATA") ? getenv("LOCALAPPDATA") : "") + "\\Android\\Sdk\\platform-tools\\adb.exe",
        std::string(getenv("USERPROFILE") ? getenv("USERPROFILE") : "") + "\\AppData\\Local\\Android\\Sdk\\platform-tools\\adb.exe",
        "C:\\Android\\Sdk\\platform-tools\\adb.exe",
    };
#else
    // check PATH first
    if (system("adb version >/dev/null 2>&1") == 0) return "adb";

    // common Android SDK locations on Linux/macOS
    std::vector<std::string> paths = {
        std::string(getenv("HOME") ? getenv("HOME") : "") + "/Android/Sdk/platform-tools/adb",
        std::string(getenv("HOME") ? getenv("HOME") : "") + "/Library/Android/sdk/platform-tools/adb",
        "/usr/local/bin/adb",
    };
#endif

    for (auto& p : paths) {
        if (!p.empty() && fs_exists(p)) return "\"" + p + "\"";
    }
    return "";
}

static int run_android() {
    std::string android_dir = "android";

    if (!fs_exists(android_dir)) {
        std::cerr << "Error: android/ folder not found\n";
        std::cerr << "Make sure you are in your app root directory\n";
        return 1;
    }

    FluxConfig config;
    if (!config_read(config)) return 1;

#ifdef _WIN32
    std::string cmd = "gradlew.bat installDebug";
#else
    std::string cmd = "./gradlew installDebug";
#endif

    std::cout << "Building and installing " << config.name << "...\n";
    int result = process_run(cmd, android_dir);

    if (result != 0) {
        std::cerr << "Android build failed\n";
        return 1;
    }

    std::cout << "Launching " << config.name << "...\n";

    std::string adb = find_adb();
    if (adb.empty()) {
        std::cerr << "Error: adb not found\n";
#ifdef _WIN32
        std::cerr << "Add this to your PATH:\n";
        std::cerr << "%LOCALAPPDATA%\\Android\\Sdk\\platform-tools\n";
#else
        std::cerr << "Add this to your PATH:\n";
        std::cerr << "~/Android/Sdk/platform-tools\n";
#endif
        return 1;
    }

    std::string launch_cmd = adb + " shell monkey -p " + config.package + " 1";
    result = process_run(launch_cmd, "");

    if (result != 0) {
        std::cerr << "Failed to launch app\n";
        std::cerr << "Make sure a device or emulator is running\n";
        return 1;
    }

    return 0;
}

static int run_windows() {
#ifndef _WIN32
    std::cerr << "Error: cannot run windows build on this platform\n";
    return 1;
#endif

    if (!fs_exists("windows")) {
        std::cerr << "Error: windows/ folder not found\n";
        std::cerr << "Make sure you are in your app root directory\n";
        return 1;
    }

    FluxConfig config;
    if (!config_read(config)) return 1;

#ifdef _WIN32
    std::string vsdev = find_vsdevcmd();
    if (vsdev.empty()) {
        std::cerr << "Error: Visual Studio not found\n";
        std::cerr << "Please install Visual Studio with C++ workload\n";
        return 1;
    }

    std::cout << "Building " << config.name << " for Windows...\n";

    std::string configure_cmd =
        "cmd /c \"\"" + vsdev + "\" && cmake -S windows -B build\"";

    int result = process_run(configure_cmd, "");
    if (result != 0) {
        std::cerr << "CMake configure failed\n";
        return 1;
    }

    std::string build_cmd =
        "cmd /c \"\"" + vsdev + "\" && cmake --build build --config Release\"";

    result = process_run(build_cmd, "");
    if (result != 0) {
        std::cerr << "CMake build failed\n";
        return 1;
    }

    std::cout << "Launching " << config.name << "...\n";
    result = process_run("build\\app.exe", "");

    return result;
#endif
}

static int run_linux() {
#ifdef _WIN32
    std::cerr << "Error: cannot run linux build on this platform\n";
    return 1;
#endif

    std::string linux_dir = "linux";

    if (!fs_exists(linux_dir)) {
        std::cerr << "Error: linux/ folder not found\n";
        std::cerr << "Make sure you are in your app root directory\n";
        return 1;
    }

    FluxConfig config;
    if (!config_read(config)) return 1;

    std::cout << "Building " << config.name << " for Linux...\n";

    int result = process_run(
        "cmake -S . -B build -G \"Unix Makefiles\" -DCMAKE_BUILD_TYPE=Release",
        linux_dir
    );
    if (result != 0) {
        std::cerr << "CMake configure failed\n";
        return 1;
    }

    result = process_run(
        "cmake --build build",
        linux_dir
    );
    if (result != 0) {
        std::cerr << "CMake build failed\n";
        return 1;
    }

    std::cout << "Launching " << config.name << "...\n";
    result = process_run("build/app", linux_dir);

    return result;
}

int cmd_run(const std::string& platform) {
    if (platform == "android") {
        return run_android();
    }
    else if (platform == "windows") {
        return run_windows();
    }
    else if (platform == "linux") {
        return run_linux();
    }
    else {
        std::cerr << "Unknown platform: " << platform << "\n";
        std::cerr << "Available platforms: android, windows, linux\n";
        return 1;
    }
}