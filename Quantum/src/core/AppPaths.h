#pragma once

#include <filesystem>
#include <string>

namespace quantum {

class AppPaths {
public:
    static AppPaths discover();

    const std::filesystem::path& dataDir() const;
    const std::filesystem::path& cacheDir() const;
    const std::filesystem::path& profileDir() const;
    const std::filesystem::path& logsDir() const;

    void ensure() const;

private:
    std::filesystem::path dataDir_;
    std::filesystem::path cacheDir_;
    std::filesystem::path profileDir_;
    std::filesystem::path logsDir_;
};

std::string readEnvironment(std::string name, std::string fallback = {});

} // namespace quantum

