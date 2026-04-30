//src/utils/config.cpp
#include "config.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <regex>

static std::string extract_value(const std::string& json, const std::string& key) {
    // matches "key": "value"
    std::regex pattern("\"" + key + "\"\\s*:\\s*\"([^\"]+)\"");
    std::smatch match;
    if (std::regex_search(json, match, pattern)) {
        return match[1].str();
    }
    return "";
}

bool config_read(FluxConfig& config, const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        std::cerr << "Error: flux.json not found\n";
        std::cerr << "Make sure you are in your app root directory\n";
        return false;
    }

    std::stringstream ss;
    ss << file.rdbuf();
    std::string json = ss.str();

    config.name    = extract_value(json, "name");
    config.package = extract_value(json, "package");

    if (config.name.empty() || config.package.empty()) {
        std::cerr << "Error: flux.json is missing name or package\n";
        return false;
    }

    return true;
}

bool config_write(const FluxConfig& config, const std::string& path) {
    std::ofstream file(path);
    if (!file) {
        std::cerr << "Error: could not write flux.json\n";
        return false;
    }

    file << "{\n";
    file << "    \"name\": \"" << config.name << "\",\n";
    file << "    \"package\": \"com.example." << config.package << "\"\n";
    file << "}\n";

    return file.good();
}