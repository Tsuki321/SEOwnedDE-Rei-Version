#pragma once

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace testhelpers {

inline std::filesystem::path FindRepoRoot() {
    auto current = std::filesystem::current_path();

    for (int i = 0; i < 8; ++i) {
        const auto features = current / "SEOwnedDE" / "SEOwnedDE" / "src" / "App" / "Features";
        if (std::filesystem::exists(features)) {
            return current;
        }

        if (!current.has_parent_path()) {
            break;
        }

        current = current.parent_path();
    }

    throw std::runtime_error("Unable to locate repository root from current working directory.");
}

inline std::string ReadTextFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + path.string());
    }

    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

inline std::size_t CountOccurrences(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) {
        return 0;
    }

    std::size_t count = 0;
    std::size_t pos = 0;

    while ((pos = haystack.find(needle, pos)) != std::string::npos) {
        ++count;
        pos += needle.size();
    }

    return count;
}

inline std::vector<std::filesystem::path> CollectFiles(const std::filesystem::path& directory,
                                                       const std::string& extension) {
    std::vector<std::filesystem::path> files;

    if (!std::filesystem::exists(directory)) {
        return files;
    }

    for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
        if (entry.is_regular_file() && entry.path().extension() == extension) {
            files.push_back(entry.path());
        }
    }

    std::sort(files.begin(), files.end());
    return files;
}

inline std::size_t CountTokenAcrossFiles(const std::vector<std::filesystem::path>& files,
                                         const std::string& token) {
    std::size_t total = 0;
    for (const auto& file : files) {
        total += CountOccurrences(ReadTextFile(file), token);
    }
    return total;
}

}  // namespace testhelpers
