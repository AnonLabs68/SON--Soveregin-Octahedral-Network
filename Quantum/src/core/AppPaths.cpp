#include "core/AppPaths.h"

#include <windows.h>
#include <shlobj.h>

#include <cstdlib>

namespace quantum {

namespace {

std::filesystem::path knownFolder(REFKNOWNFOLDERID id, std::filesystem::path fallback) {
    PWSTR raw = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(id, 0, nullptr, &raw))) {
        std::filesystem::path path(raw);
        CoTaskMemFree(raw);
        return path;
    }
    return fallback;
}

} // namespace

AppPaths AppPaths::discover() {
    const auto local = knownFolder(FOLDERID_LocalAppData, std::filesystem::temp_directory_path());
    const auto roaming = knownFolder(FOLDERID_RoamingAppData, local);

    AppPaths paths;
    paths.dataDir_ = roaming / "Quantum";
    paths.cacheDir_ = local / "Quantum" / "Cache";
    paths.profileDir_ = local / "Quantum" / "WebView2Profile";
    paths.logsDir_ = local / "Quantum" / "Logs";
    return paths;
}

const std::filesystem::path& AppPaths::dataDir() const {
    return dataDir_;
}

const std::filesystem::path& AppPaths::cacheDir() const {
    return cacheDir_;
}

const std::filesystem::path& AppPaths::profileDir() const {
    return profileDir_;
}

const std::filesystem::path& AppPaths::logsDir() const {
    return logsDir_;
}

void AppPaths::ensure() const {
    std::filesystem::create_directories(dataDir_);
    std::filesystem::create_directories(cacheDir_);
    std::filesystem::create_directories(profileDir_);
    std::filesystem::create_directories(logsDir_);
}

std::string readEnvironment(std::string name, std::string fallback) {
    size_t size = 0;
    getenv_s(&size, nullptr, 0, name.c_str());
    if (size == 0) {
        return fallback;
    }
    std::string value(size, '\0');
    getenv_s(&size, value.data(), value.size(), name.c_str());
    if (!value.empty() && value.back() == '\0') {
        value.pop_back();
    }
    return value.empty() ? fallback : value;
}

} // namespace quantum

