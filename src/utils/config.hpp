//src/utils/config.hpp
#pragma once
#include <string>

struct FluxConfig {
    std::string name;
    std::string package;
};

bool config_read(FluxConfig& config, const std::string& path = "flux.json");
bool config_write(const FluxConfig& config, const std::string& path = "flux.json");