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

// ── Helpers ───────────────────────────────────────────────────────────────────

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
    if (system("adb version >nul 2>&1") == 0) return "adb";
    std::vector<std::string> paths = {
        std::string(getenv("LOCALAPPDATA") ? getenv("LOCALAPPDATA") : "") + "\\Android\\Sdk\\platform-tools\\adb.exe",
        std::string(getenv("USERPROFILE")  ? getenv("USERPROFILE")  : "") + "\\AppData\\Local\\Android\\Sdk\\platform-tools\\adb.exe",
        "C:\\Android\\Sdk\\platform-tools\\adb.exe",
    };
#else
    if (system("adb version >/dev/null 2>&1") == 0) return "adb";
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

// Only re-configure if:
//   - build dir doesn't exist (first time)
//   - CMakeCache.txt missing (previous configure failed)
//   - CMakeLists.txt is newer than CMakeCache.txt (cmake config changed)
static bool needs_configure(const std::string& src_dir,
                             const std::string& build_dir) {
    std::string cache      = fs_join(build_dir, "CMakeCache.txt");
    std::string cmake_list = fs_join(src_dir,   "CMakeLists.txt");

    if (!fs_exists(build_dir) || !fs_exists(cache)) return true;
    if (!fs_exists(cmake_list)) return true;

    return fs::last_write_time(cmake_list) > fs::last_write_time(cache);
}

// ── Android ───────────────────────────────────────────────────────────────────

static int run_android() {
    if (!fs_exists("android")) {
        std::cerr << "Error: android/ folder not found\n";
        std::cerr << "Make sure you are in your app root directory\n";
        return 1;
    }

    FluxConfig config;
    if (!config_read(config)) return 1;

    std::string adb = find_adb();
    if (adb.empty()) {
        std::cerr << "Error: adb not found\n";
#ifdef _WIN32
        std::cerr << "Add to PATH: %LOCALAPPDATA%\\Android\\Sdk\\platform-tools\n";
#else
        std::cerr << "Add to PATH: ~/Android/Sdk/platform-tools\n";
#endif
        return 1;
    }

    // Check if already installed to decide install strategy
    std::string check_cmd  = adb + " shell pm list packages " + config.package;
    bool already_installed = (process_run(check_cmd, "") == 0);

    int result;
    if (!already_installed) {
        std::cout << "First install — full build...\n";
#ifdef _WIN32
        result = process_run("gradlew.bat installDebug", "android");
#else
        result = process_run("./gradlew installDebug", "android");
#endif
        if (result != 0) {
            std::cerr << "Android build failed\n";
            return 1;
        }
    } else {
        // assembleDebug + adb install -r is faster than installDebug
        std::cout << "Building " << config.name << " (incremental)...\n";
#ifdef _WIN32
        result = process_run("gradlew.bat assembleDebug", "android");
#else
        result = process_run("./gradlew assembleDebug", "android");
#endif
        if (result != 0) {
            std::cerr << "Android build failed\n";
            return 1;
        }

        std::string apk = "app/build/outputs/apk/debug/app-debug.apk";
        if (!fs_exists(fs_join("android", apk))) {
            std::cerr << "APK not found — check gradle output path\n";
            return 1;
        }

        std::cout << "Installing...\n";
        result = process_run(adb + " install -r " + apk, "android");
        if (result != 0) {
            std::cerr << "Install failed\n";
            return 1;
        }
    }

    std::cout << "Launching " << config.name << "...\n";
    result = process_run(
        adb + " shell monkey -p " + config.package + " 1", ""
    );
    if (result != 0) {
        std::cerr << "Failed to launch — make sure a device or emulator is running\n";
        return 1;
    }

    return 0;
}

// ── Windows ───────────────────────────────────────────────────────────────────

static int run_windows() {
#ifndef _WIN32
    std::cerr << "Error: cannot run windows build on this platform\n";
    return 1;
#else
    if (!fs_exists("windows")) {
        std::cerr << "Error: windows/ folder not found\n";
        std::cerr << "Make sure you are in your app root directory\n";
        return 1;
    }

    FluxConfig config;
    if (!config_read(config)) return 1;

    std::string vsdev = find_vsdevcmd();
    if (vsdev.empty()) {
        std::cerr << "Error: Visual Studio not found\n";
        std::cerr << "Please install Visual Studio with C++ workload\n";
        return 1;
    }

    std::string src_dir   = "windows";
    std::string build_dir = "build";  // your original: cmake -S windows -B build

    if (needs_configure(src_dir, build_dir)) {
        std::cout << "Configuring (first time, downloading flux — this takes a while)...\n";

        std::string cmd =
            "cmd /c \"\"" + vsdev + "\" && cmake -S windows -B build\"";

        int r = process_run(cmd, "");
        if (r != 0) {
            std::cerr << "CMake configure failed\n";
            return 1;
        }
    }

    std::cout << "Building " << config.name << "...\n";
    std::string build_cmd =
        "cmd /c \"\"" + vsdev + "\" && cmake --build build --config Release\"";

    int result = process_run(build_cmd, "");
    if (result != 0) {
        std::cerr << "CMake build failed\n";
        return 1;
    }

    std::cout << "Launching " << config.name << "...\n";
    // your original
    return process_run("build\\app.exe", "");
#endif
}

// ── Linux ─────────────────────────────────────────────────────────────────────

static int run_linux() {
#ifdef _WIN32
    std::cerr << "Error: cannot run linux build on this platform\n";
    return 1;
#else
    if (!fs_exists("linux")) {
        std::cerr << "Error: linux/ folder not found\n";
        std::cerr << "Make sure you are in your app root directory\n";
        return 1;
    }

    FluxConfig config;
    if (!config_read(config)) return 1;

    std::string src_dir   = "linux";
    std::string build_dir = "linux/build";  // cmake -S . -B build run from linux/

    if (needs_configure(src_dir, build_dir)) {
        std::cout << "Configuring (first time, downloading flux — this takes a while)...\n";

        int r = process_run(
            "cmake -S . -B build -G \"Unix Makefiles\" -DCMAKE_BUILD_TYPE=Release",
            "linux"
        );
        if (r != 0) {
            std::cerr << "CMake configure failed\n";
            return 1;
        }
    }

    std::cout << "Building " << config.name << "...\n";
    int result = process_run(
        "cmake --build build -- -j$(nproc)",
        "linux"
    );
    if (result != 0) {
        std::cerr << "CMake build failed\n";
        return 1;
    }

    std::cout << "Launching " << config.name << "...\n";
    // your original
    return process_run("build/app", "linux");
#endif
}

// ─────────────────────────────────────────────────────────────────────────────

int cmd_run(const std::string& platform) {
    if (platform == "android") return run_android();
    if (platform == "windows") return run_windows();
    if (platform == "linux")   return run_linux();

    std::cerr << "Unknown platform: " << platform << "\n";
    std::cerr << "Available platforms: android, windows, linux\n";
    return 1;
}