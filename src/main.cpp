#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#include <cstdio>
#include <shellapi.h>
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace {

using PathString = fs::path::string_type;

#ifdef _WIN32
#define PATH_LITERAL(str) L##str
#else
#define PATH_LITERAL(str) str
#endif

PathString toLowerCopy(PathString text) {
    std::transform(text.begin(), text.end(), text.begin(), [](fs::path::value_type ch) {
#ifdef _WIN32
        return static_cast<fs::path::value_type>(std::towlower(ch));
#else
        return static_cast<fs::path::value_type>(std::tolower(static_cast<unsigned char>(ch)));
#endif
    });
    return text;
}

bool isImageFile(const fs::path &path) {
    static const std::set<PathString> kExtensions = {
        PATH_LITERAL(".jpg"), PATH_LITERAL(".jpeg"), PATH_LITERAL(".png"), PATH_LITERAL(".bmp"), PATH_LITERAL(".webp")
    };

    auto ext = toLowerCopy(path.extension().native());
    return kExtensions.count(ext) > 0;
}

std::string timestampForLog() {
    using clock = std::chrono::system_clock;
    auto now = clock::now();
    auto tt = clock::to_time_t(now);
    std::tm tm {};
#ifdef _WIN32
    localtime_s(&tm, &tt);
#else
    localtime_r(&tt, &tm);
#endif
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm);
    return std::string(buffer);
}

fs::path detectLogFilePath() {
#ifdef _WIN32
    if (const char *localAppData = std::getenv("LOCALAPPDATA")) {
        fs::path candidate = fs::path(localAppData) / "PushToFolders";
        std::error_code ec;
        fs::create_directories(candidate, ec);
        return candidate / "PushToFolders.log";
    }
    if (const char *userProfile = std::getenv("USERPROFILE")) {
        return fs::path(userProfile) / "PushToFolders.log";
    }
#endif
    std::error_code ec;
    fs::path fallback = fs::temp_directory_path(ec);
    if (ec) {
        return fs::path("PushToFolders.log");
    }
    return fallback / "PushToFolders.log";
}

class Logger {
public:
    Logger()
        : logFilePath_(detectLogFilePath()), stream_(logFilePath_, std::ios::app)
    {
        if (!stream_) {
            std::cerr << "Warning: Unable to open log file at " << logFilePath_ << "\n";
        } else {
            stream_ << "--- Run started at " << timestampForLog() << " ---\n";
        }
    }

    void logError(const fs::path &target, std::string_view message) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stream_) {
            stream_ << "[" << timestampForLog() << "] ERROR: " << message;
            if (!target.empty()) {
                stream_ << " | Target: " << target.u8string();
            }
            stream_ << "\n";
        }
    }

    void logInfo(std::string_view message) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stream_) {
            stream_ << "[" << timestampForLog() << "] INFO: " << message << "\n";
        }
    }

    const fs::path &path() const {
        return logFilePath_;
    }

private:
    fs::path logFilePath_;
    std::ofstream stream_;
    std::mutex mutex_;
};

void printUsage(const fs::path &logPath) {
    std::cout << "PushToFolders - Organise images into same-named folders\n\n"
              << "Usage:\n"
              << "  PushToFolders \"C:/path/to/folder\"  (command line folder mode)\n"
              << "  PushToFolders <image1> <image2> ...     (Explorer selection mode)\n"
              << "  PushToFolders --show-log               (display error log)\n"
              << "  PushToFolders --clear-log              (clear error log)\n\n"
              << "Log file: " << logPath << "\n";
}

bool ensureDirectory(const fs::path &dir, Logger &logger) {
    std::error_code ec;
    if (fs::exists(dir, ec)) {
        if (!fs::is_directory(dir, ec)) {
            logger.logError(dir, "A non-directory with the desired folder name already exists.");
            std::cerr << "Cannot create folder '" << dir << "' because a file exists with that name.\n";
            return false;
        }
        return true;
    }

    fs::create_directories(dir, ec);
    if (ec) {
        logger.logError(dir, std::string("Failed to create folder: ") + ec.message());
        std::cerr << "Failed to create folder '" << dir << "': " << ec.message() << "\n";
        return false;
    }
    return true;
}

bool moveFileToFolder(const fs::path &filePath, Logger &logger) {
    std::error_code ec;
    if (!fs::exists(filePath, ec)) {
        logger.logError(filePath, "File does not exist.");
        std::cerr << "File not found: " << filePath << "\n";
        return false;
    }

    if (!fs::is_regular_file(filePath, ec)) {
        logger.logError(filePath, "Path is not a regular file.");
        std::cerr << "Not a file: " << filePath << "\n";
        return false;
    }

    if (!isImageFile(filePath)) {
        logger.logInfo(std::string("Skipping non-image file: ") + filePath.u8string());
        return false;
    }

    fs::path destinationFolder = filePath.parent_path() / filePath.stem();
    if (!ensureDirectory(destinationFolder, logger)) {
        return false;
    }

    fs::path destinationFile = destinationFolder / filePath.filename();
    if (fs::exists(destinationFile, ec)) {
        logger.logError(destinationFile, "Destination file already exists.");
        std::cerr << "Destination already exists: " << destinationFile << "\n";
        return false;
    }

    fs::rename(filePath, destinationFile, ec);
    if (ec) {
        logger.logError(destinationFile, std::string("Failed to move file: ") + ec.message());
        std::cerr << "Failed to move '" << filePath << "': " << ec.message() << "\n";
        return false;
    }

    logger.logInfo(std::string("Moved ") + filePath.u8string() + " to " + destinationFolder.u8string());
    std::cout << "Moved '" << filePath.filename().u8string() << "' into '" << destinationFolder.filename().u8string() << "'\n";
    return true;
}

bool processDirectory(const fs::path &directoryPath, Logger &logger) {
    std::error_code ec;
    if (!fs::exists(directoryPath, ec) || !fs::is_directory(directoryPath, ec)) {
        logger.logError(directoryPath, "The supplied path is not a directory.");
        std::cerr << "The path is not a folder: " << directoryPath << "\n";
        return false;
    }

    bool anyProcessed = false;
    for (const auto &entry : fs::directory_iterator(directoryPath, ec)) {
        if (ec) {
            logger.logError(directoryPath, std::string("Failed to scan directory: ") + ec.message());
            std::cerr << "Failed to scan directory '" << directoryPath << "': " << ec.message() << "\n";
            return false;
        }

        if (!entry.is_regular_file()) {
            continue;
        }

        if (isImageFile(entry.path())) {
            if (moveFileToFolder(entry.path(), logger)) {
                anyProcessed = true;
            }
        }
    }

    if (!anyProcessed) {
        std::cout << "No image files found in " << directoryPath << "\n";
    }

    return anyProcessed;
}

bool processFiles(const std::vector<fs::path> &files, Logger &logger) {
    bool anyProcessed = false;
    for (const auto &file : files) {
        if (moveFileToFolder(file, logger)) {
            anyProcessed = true;
        }
    }

    if (!anyProcessed) {
        std::cout << "No image files were processed.\n";
    }

    return anyProcessed;
}

std::optional<std::string> readFileContents(const fs::path &path) {
    std::ifstream input(path);
    if (!input) {
        return std::nullopt;
    }

    std::string contents;
    input.seekg(0, std::ios::end);
    contents.resize(static_cast<size_t>(input.tellg()));
    input.seekg(0, std::ios::beg);
    input.read(contents.data(), static_cast<std::streamsize>(contents.size()));
    return contents;
}

bool clearLogFile(const fs::path &path) {
    std::ofstream output(path, std::ios::trunc);
    return static_cast<bool>(output);
}

int runApplication(const std::vector<PathString> &args) {
    Logger logger;
    if (args.empty()) {
        printUsage(logger.path());
        return 1;
    }

    const PathString showLogLong = PATH_LITERAL("--show-log");
    const PathString showLogShort = PATH_LITERAL("/showlog");
    const PathString clearLogLong = PATH_LITERAL("--clear-log");
    const PathString clearLogShort = PATH_LITERAL("/clearlog");

    if (args.size() == 1) {
        if (args[0] == showLogLong || args[0] == showLogShort) {
            auto contents = readFileContents(logger.path());
            if (!contents) {
                std::cerr << "No log file found at " << logger.path() << "\n";
                return 1;
            }
            std::cout << "Log file: " << logger.path() << "\n" << *contents;
            return 0;
        }
        if (args[0] == clearLogLong || args[0] == clearLogShort) {
            if (clearLogFile(logger.path())) {
                std::cout << "Log file cleared: " << logger.path() << "\n";
                return 0;
            }
            std::cerr << "Unable to clear log file at " << logger.path() << "\n";
            return 1;
        }
    }

    if (args.size() == 1) {
        fs::path potentialDirectory(args[0]);
        std::error_code ec;
        if (fs::exists(potentialDirectory, ec) && fs::is_directory(potentialDirectory, ec)) {
            bool success = processDirectory(potentialDirectory, logger);
            std::cout << "Finished processing folder." << std::endl;
            std::cout << "Check the log for any errors: " << logger.path() << "\n";
            return success ? 0 : 1;
        }
        // Not a directory: treat as single file
    }

    std::vector<fs::path> filePaths;
    filePaths.reserve(args.size());
    for (const auto &arg : args) {
        filePaths.emplace_back(arg);
    }

    bool success = processFiles(filePaths, logger);
    std::cout << "Finished processing files. Check the log for any errors: "
              << logger.path() << "\n";
    return success ? 0 : 1;
}

#undef PATH_LITERAL

} // namespace

#ifdef _WIN32
int APIENTRY wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    int argc = 0;
    std::vector<PathString> args;
    if (LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc)) {
        if (argc > 1) {
            args.reserve(static_cast<size_t>(argc - 1));
            for (int i = 1; i < argc; ++i) {
                args.emplace_back(argv[i]);
            }
        }
        LocalFree(argv);
    }

    bool attachedConsole = false;
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        attachedConsole = true;

        FILE *dummy = nullptr;
        freopen_s(&dummy, "CONOUT$", "w", stdout);
        freopen_s(&dummy, "CONOUT$", "w", stderr);
        freopen_s(&dummy, "CONIN$", "r", stdin);
    }

    int result = runApplication(args);

    if (attachedConsole) {
        FreeConsole();
    }

    return result;
}
#else
int main(int argc, char *argv[]) {
    std::vector<PathString> args;
    if (argc > 1) {
        args.reserve(static_cast<size_t>(argc - 1));
    }
    for (int i = 1; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }
    return runApplication(args);
}
#endif

