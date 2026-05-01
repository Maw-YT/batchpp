#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "compiler_api.hpp"

namespace fs = std::filesystem;

namespace {

constexpr const char* kVersion = "0.0.1";

void printUsage() {
    std::cerr
        << "Usage: batppc [options] <input.batpp|input.cmdpp> <output.bat|output.cmd>\n"
        << "       batppc [options] <input.batpp|input.cmdpp> --stdout\n"
        << "       batppc [options] <input.batpp|input.cmdpp> -o -\n\n"
        << "Options:\n"
        << "  -v, --version   Print batppc version\n"
        << "  --verbose       Print import resolution and emitted label count\n"
        << "  --check         Parse/transpile only; do not write output file\n"
        << "  --debug-echo    Emit '@echo on' in generated batch\n"
        << "  --emit-header   Emit generated metadata header comments\n"
        << "  --stdout        Write generated batch to stdout\n"
        << "  -o <path>       Output path; use '-' to write to stdout\n";
}

std::string normalizeCrlf(std::string s) {
    std::string out;
    out.reserve(s.size() + 256);
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\n') {
            if (i == 0 || s[i - 1] != '\r') out.push_back('\r');
            out.push_back('\n');
        } else {
            out.push_back(s[i]);
        }
    }
    return out;
}

int countLabels(const std::string& batchText) {
    int labels = 0;
    bool atLineStart = true;
    for (char ch : batchText) {
        if (atLineStart && ch == ':') {
            ++labels;
        }
        if (ch == '\n') {
            atLineStart = true;
            continue;
        }
        if (ch != '\r') {
            atLineStart = false;
        }
    }
    return labels;
}

bool canCreateOrWrite(const fs::path& outputPath, std::string& errorOut) {
    std::error_code ec;
    fs::path parent = outputPath.parent_path();
    if (parent.empty()) {
        parent = fs::current_path(ec);
        if (ec) {
            errorOut = "Cannot resolve current working directory.";
            return false;
        }
    }

    if (!fs::exists(parent, ec) || ec) {
        errorOut = "Output directory does not exist: " + parent.string();
        return false;
    }
    if (!fs::is_directory(parent, ec) || ec) {
        errorOut = "Output parent is not a directory: " + parent.string();
        return false;
    }

    std::ofstream test(outputPath, std::ios::binary | std::ios::trunc);
    if (!test) {
        errorOut = "Cannot write output file: " + outputPath.string();
        return false;
    }
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc <= 1) {
        printUsage();
        return 1;
    }

    bool verbose = false;
    bool checkOnly = false;
    bool stdoutMode = false;
    bool debugEcho = false;
    bool emitHeader = false;
    std::optional<std::string> inputPath;
    std::optional<std::string> outputPath;

    std::vector<std::string> args;
    args.reserve(static_cast<size_t>(argc - 1));
    for (int i = 1; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }

    for (size_t i = 0; i < args.size(); ++i) {
        const std::string& arg = args[i];
        if (arg == "--version" || arg == "-v") {
            if (args.size() == 1) {
                std::cout << "batppc " << kVersion << "\n";
                return 0;
            }
            // Keep `-v` as a usable alias in compile mode.
            verbose = true;
            continue;
        }
        if (arg == "--verbose") {
            verbose = true;
            continue;
        }
        if (arg == "--check") {
            checkOnly = true;
            continue;
        }
        if (arg == "--debug-echo") {
            debugEcho = true;
            continue;
        }
        if (arg == "--emit-header") {
            emitHeader = true;
            continue;
        }
        if (arg == "--stdout") {
            stdoutMode = true;
            continue;
        }
        if (arg == "-o") {
            if (i + 1 >= args.size()) {
                std::cerr << "Missing value after -o.\n";
                printUsage();
                return 1;
            }
            outputPath = args[++i];
            continue;
        }
        if (!arg.empty() && arg[0] == '-') {
            std::cerr << "Unknown option: " << arg << "\n";
            printUsage();
            return 1;
        }
        if (!inputPath.has_value()) {
            inputPath = arg;
            continue;
        }
        if (!outputPath.has_value()) {
            outputPath = arg;
            continue;
        }
        std::cerr << "Unexpected extra positional argument: " << arg << "\n";
        printUsage();
        return 1;
    }

    if (!inputPath.has_value()) {
        std::cerr << "Missing input file.\n";
        printUsage();
        return 1;
    }

    if (outputPath.has_value() && *outputPath == "-") {
        stdoutMode = true;
    }

    if (!checkOnly && !stdoutMode && !outputPath.has_value()) {
        std::cerr << "Missing output file path.\n";
        printUsage();
        return 1;
    }

    if (!checkOnly && !stdoutMode && outputPath.has_value()) {
        std::string writeError;
        if (!canCreateOrWrite(*outputPath, writeError)) {
            std::cerr << writeError << "\n";
            return 1;
        }
    }

    try {
        CompileOptions options;
        options.verbose = verbose;
        options.debugEcho = debugEcho;
        options.emitMetadataHeader = emitHeader;
        options.compilerVersion = kVersion;
        std::string code = compileBatppFile(*inputPath, options);
        code = normalizeCrlf(std::move(code));

        if (checkOnly) {
            std::cout << "Check passed: " << *inputPath << "\n";
            if (verbose) {
                std::cerr << "[batppc] emitted labels: " << countLabels(code) << "\n";
            }
            return 0;
        }

        if (stdoutMode) {
            std::cout << code;
            if (verbose) {
                std::cerr << "[batppc] emitted labels: " << countLabels(code) << "\n";
            }
            return 0;
        }

        std::ofstream out(*outputPath, std::ios::binary | std::ios::trunc);
        if (!out) {
            std::cerr << "Cannot write output file: " << *outputPath << "\n";
            return 1;
        }
        out << code;
        std::cout << "Compiled " << *inputPath << " -> " << *outputPath << "\n";
        if (verbose) {
            std::cerr << "[batppc] emitted labels: " << countLabels(code) << "\n";
        }
    } catch (const std::exception& e) {
        std::cerr << "Compilation failed: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
