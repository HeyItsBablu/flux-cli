//src/utils/fs.hpp
#pragma once
#include <string>
#include <vector>
#include <cstdint> 

bool fs_create_dir(const std::string& path);
bool fs_create_dirs(const std::string& path);
bool fs_write_file(const std::string& path, const std::vector<uint8_t>& data);
bool fs_exists(const std::string& path);
std::string fs_join(const std::string& a, const std::string& b);