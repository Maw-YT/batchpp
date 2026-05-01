#pragma once

#include <filesystem>
#include <string>

struct CompileOptions {
    bool verbose = false;
    bool debugEcho = false;
    bool emitMetadataHeader = false;
    std::string compilerVersion = "unknown";
};

std::string compileBatppFile(const std::filesystem::path& input, const CompileOptions& options = {});
