//src/commands/create.cpp
#include "create.hpp"
#include "../utils/fs.hpp"
#include "../utils/config.hpp"
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include <cstdlib>

#include <miniz.h>

namespace fs = std::filesystem;

static const std::string DOWNLOAD_URL = "https://codeload.github.com/HeyItsBablu/flux-cli/zip/refs/heads/main";
static const std::string ZIP_NAME     = "flux_template.zip";

static std::vector<uint8_t> download_zip() {
    std::cout << "Downloading template...\n";

    std::string cmd = "curl -L -o " + ZIP_NAME + " " + DOWNLOAD_URL;
    int result = system(cmd.c_str());

    if (result != 0) {
        std::cerr << "Download failed. Make sure curl is available.\n";
        return {};
    }

    // read zip into memory
    std::ifstream file(ZIP_NAME, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open downloaded zip\n";
        return {};
    }


std::vector<uint8_t> data(
    (std::istreambuf_iterator<char>(file)),
    (std::istreambuf_iterator<char>())
);

    file.close();
    std::remove(ZIP_NAME.c_str());

    std::cout << "Download complete.\n";
    return data;
}

static bool extract_zip(const std::vector<uint8_t>& zip_data, const std::string& dest) {
    std::cout << "Extracting template...\n";

    mz_zip_archive zip = {};
    if (!mz_zip_reader_init_mem(&zip, zip_data.data(), zip_data.size(), 0)) {
        std::cerr << "Failed to open zip archive\n";
        return false;
    }

    int file_count = (int)mz_zip_reader_get_num_files(&zip);

    for (int i = 0; i < file_count; i++) {
        mz_zip_archive_file_stat stat;
        if (!mz_zip_reader_file_stat(&zip, i, &stat)) continue;

        std::string raw_path = stat.m_filename;

        // strip first level github adds (flux-cli-main/)
        auto slash1 = raw_path.find('/');
        if (slash1 == std::string::npos) continue;
        std::string after_root = raw_path.substr(slash1 + 1);

        // only keep files inside templates/
        static const std::string TEMPLATES_PREFIX = "templates/";
        if (after_root.rfind(TEMPLATES_PREFIX, 0) != 0) continue;

        // strip templates/
        std::string rel_path = after_root.substr(TEMPLATES_PREFIX.size());
        if (rel_path.empty()) continue;

        std::string out_path = fs_join(dest, rel_path);

        if (mz_zip_reader_is_file_a_directory(&zip, i)) {
            fs_create_dirs(out_path);
        } else {
            fs_create_dirs(fs::path(out_path).parent_path().string());

            size_t size = 0;
            void* data = mz_zip_reader_extract_to_heap(&zip, i, &size, 0);
            if (!data) {
                std::cerr << "Failed to extract: " << rel_path << "\n";
                mz_zip_reader_end(&zip);
                return false;
            }

            std::vector<uint8_t> file_data(
                static_cast<uint8_t*>(data),
                static_cast<uint8_t*>(data) + size
            );
            mz_free(data);

            if (!fs_write_file(out_path, file_data)) {
                std::cerr << "Failed to write: " << out_path << "\n";
                mz_zip_reader_end(&zip);
                return false;
            }
        }
    }

    mz_zip_reader_end(&zip);
    std::cout << "Extraction complete.\n";
    return true;
}

int cmd_create(const std::string& app_name) {
    if (fs_exists(app_name)) {
        std::cerr << "Error: folder '" << app_name << "' already exists\n";
        return 1;
    }

    auto zip_data = download_zip();
    if (zip_data.empty()) return 1;

    if (!extract_zip(zip_data, app_name)) return 1;

    FluxConfig config;
    config.name    = app_name;
    config.package = app_name;

    if (!config_write(config, fs_join(app_name, "flux.json"))) return 1;

    std::cout << "\n";
    std::cout << "Created " << app_name << " successfully!\n\n";
    std::cout << "Next steps:\n";
    std::cout << "  cd " << app_name << "\n";
    std::cout << "  flux run windows\n";

    return 0;
}