//src/commands/run.cpp
#include "run.hpp"
#include "../utils/process.hpp"
#include "../utils/fs.hpp"
#include "../utils/config.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <cstdio>

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

// Find Android SDK root on this machine
static std::string find_android_sdk() {
    // ANDROID_HOME / ANDROID_SDK_ROOT env vars first
    const char* android_home = getenv("ANDROID_HOME");
    if (android_home && fs_exists(android_home)) return android_home;

    const char* android_sdk_root = getenv("ANDROID_SDK_ROOT");
    if (android_sdk_root && fs_exists(android_sdk_root)) return android_sdk_root;

#ifdef _WIN32
    std::vector<std::string> paths = {
        std::string(getenv("LOCALAPPDATA") ? getenv("LOCALAPPDATA") : "") + "\\Android\\Sdk",
        std::string(getenv("USERPROFILE")  ? getenv("USERPROFILE")  : "") + "\\AppData\\Local\\Android\\Sdk",
        "C:\\Android\\Sdk",
    };
#else
    std::vector<std::string> paths = {
        std::string(getenv("HOME") ? getenv("HOME") : "") + "/Android/Sdk",
        std::string(getenv("HOME") ? getenv("HOME") : "") + "/Library/Android/sdk",
        "/usr/local/lib/android/sdk",
    };
#endif

    for (auto& p : paths) {
        if (!p.empty() && fs_exists(p)) return p;
    }
    return "";
}

// Write android/local.properties if it doesn't exist or has no sdk.dir
static bool ensure_local_properties(const std::string& android_dir) {
    std::string props_path = fs_join(android_dir, "local.properties");

    // If it already exists, check it has sdk.dir
    if (fs_exists(props_path)) {
        std::ifstream f(props_path);
        std::string content((std::istreambuf_iterator<char>(f)),
                             std::istreambuf_iterator<char>());
        if (content.find("sdk.dir") != std::string::npos) {
            return true; // already good
        }
    }

    std::string sdk = find_android_sdk();
    if (sdk.empty()) {
        std::cerr << "Error: Android SDK not found\n";
        std::cerr << "Set ANDROID_HOME environment variable or install Android Studio\n";
        return false;
    }

#ifdef _WIN32
    // local.properties requires forward slashes or escaped backslashes
    // Use forward slashes — gradle accepts them on Windows
    std::string sdk_escaped;
    for (char c : sdk) {
        sdk_escaped += (c == '\\') ? '/' : c;
    }
#else
    std::string sdk_escaped = sdk;
#endif

    std::string text = "sdk.dir=" + sdk_escaped + "\n";
    std::vector<uint8_t> data(text.begin(), text.end());

    if (!fs_write_file(props_path, data)) {
        std::cerr << "Error: could not write local.properties\n";
        return false;
    }

    std::cout << "Created local.properties (sdk.dir=" << sdk << ")\n";
    return true;
}

// Read connected adb devices, returns list of serials
static std::vector<std::string> get_adb_devices(const std::string& adb) {
    std::vector<std::string> serials;

#ifdef _WIN32
    FILE* pipe = _popen((adb + " devices").c_str(), "r");
#else
    FILE* pipe = popen((adb + " devices").c_str(), "r");
#endif

    if (!pipe) return serials;

    char line[256];
    bool first = true;
    while (fgets(line, sizeof(line), pipe)) {
        if (first) { first = false; continue; } // skip header "List of devices attached"
        std::string s(line);
        // valid line: "emulator-5554\tdevice" or "XXXXXXX\tdevice"
        if (s.find("\tdevice") != std::string::npos) {
            auto tab = s.find('\t');
            if (tab != std::string::npos) {
                serials.push_back(s.substr(0, tab));
            }
        }
    }

#ifdef _WIN32
    _pclose(pipe);
#else
    pclose(pipe);
#endif

    return serials;
}

// Pick a device serial — auto if one, prompt if many, error if none
static std::string pick_device(const std::string& adb) {
    auto serials = get_adb_devices(adb);

    if (serials.empty()) {
        std::cerr << "Error: no devices connected\n";
        std::cerr << "Start an emulator or connect a device\n";
        return "";
    }

    if (serials.size() == 1) {
        return serials[0];
    }

    std::cout << "Multiple devices connected:\n";
    for (size_t i = 0; i < serials.size(); i++) {
        std::cout << "  [" << i << "] " << serials[i] << "\n";
    }
    std::cout << "Select device (0-" << serials.size() - 1 << "): ";
    size_t choice = 0;
    std::cin >> choice;
    if (choice >= serials.size()) {
        std::cerr << "Invalid choice\n";
        return "";
    }
    return serials[choice];
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

    // Ensure local.properties exists before any gradle call
    if (!ensure_local_properties("android")) return 1;

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

    // Pick device before building so we fail fast on no device
    std::string serial = pick_device(adb);
    if (serial.empty()) return 1;

    // Use -s <serial> for all adb commands — works correctly with one or many devices
    std::string adb_dev = adb + " -s " + serial;

    // Check if already installed on this specific device
    bool already_installed =
        (process_run(adb_dev + " shell pm list packages " + config.package, "") == 0);

    int result;
    if (!already_installed) {
        // First time: full build + install
        std::cout << "Building and installing " << config.name << "...\n";
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
            std::cerr << "APK not found at android/" << apk << "\n";
            return 1;
        }

        result = process_run(adb_dev + " install " + apk, "android");
        if (result != 0) {
            std::cerr << "Install failed\n";
            return 1;
        }

    } else {
        // Already installed: incremental build + replace
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
            std::cerr << "APK not found at android/" << apk << "\n";
            return 1;
        }

        std::cout << "Installing...\n";
        // -r = replace existing install, no uninstall needed
        result = process_run(adb_dev + " install -r " + apk, "android");
        if (result != 0) {
            std::cerr << "Install failed\n";
            return 1;
        }
    }

    std::cout << "Launching " << config.name << "...\n";
    result = process_run(
        adb_dev + " shell monkey -p " + config.package + " 1", ""
    );
    if (result != 0) {
        std::cerr << "Failed to launch app\n";
        std::cerr << "Make sure a device or emulator is running\n";
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

    // cmake -S windows -B build (your original paths)
    std::string src_dir   = "windows";
    std::string build_dir = "build";

    if (needs_configure(src_dir, build_dir)) {
        std::cout << "Configuring " << config.name
                  << " (first time downloads flux — this takes a while)...\n";

        std::string cmd =
            "cmd /c \"\"" + vsdev + "\" && cmake -S windows -B build\"";

        int r = process_run(cmd, "");
        if (r != 0) {
            std::cerr << "CMake configure failed\n";
            return 1;
        }
    }

    std::cout << "Building " << config.name << " for Windows...\n";
    std::string build_cmd =
        "cmd /c \"\"" + vsdev + "\" && cmake --build build --config Release\"";

    int result = process_run(build_cmd, "");
    if (result != 0) {
        std::cerr << "CMake build failed\n";
        return 1;
    }

    std::cout << "Launching " << config.name << "...\n";
    // your original exe path
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

    // cmake -S . -B build run from linux/ so build dir is linux/build
    std::string src_dir   = "linux";
    std::string build_dir = "linux/build";

    if (needs_configure(src_dir, build_dir)) {
        std::cout << "Configuring " << config.name
                  << " (first time downloads flux — this takes a while)...\n";

        int r = process_run(
            "cmake -S . -B build -G \"Unix Makefiles\" -DCMAKE_BUILD_TYPE=Release",
            "linux"
        );
        if (r != 0) {
            std::cerr << "CMake configure failed\n";
            return 1;
        }
    }

    std::cout << "Building " << config.name << " for Linux...\n";
    int result = process_run(
        "cmake --build build -- -j$(nproc)",
        "linux"
    );
    if (result != 0) {
        std::cerr << "CMake build failed\n";
        return 1;
    }

    std::cout << "Launching " << config.name << "...\n";
    // your original exe path
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