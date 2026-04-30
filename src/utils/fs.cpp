//src/utils/fs.cpp
#include "fs.hpp"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

bool fs_create_dir(const std::string& path) {
    std::error_code ec;
    fs::create_directory(path, ec);
    return !ec;
}

bool fs_create_dirs(const std::string& path) {
    std::error_code ec;
    fs::create_directories(path, ec);
    return !ec;
}

bool fs_write_file(const std::string& path, const std::vector<uint8_t>& data) {
    // make sure parent dirs exist
    fs_create_dirs(fs::path(path).parent_path().string());

    std::ofstream file(path, std::ios::binary);
    if (!file) return false;
    file.write(reinterpret_cast<const char*>(data.data()), data.size());
    return file.good();
}

bool fs_exists(const std::string& path) {
    return fs::exists(path);
}

std::string fs_join(const std::string& a, const std::string& b) {
    return (fs::path(a) / fs::path(b)).string();
}