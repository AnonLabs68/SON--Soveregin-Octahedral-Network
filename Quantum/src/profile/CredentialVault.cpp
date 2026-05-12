#include "profile/CredentialVault.h"

#include <nlohmann/json.hpp>

#include <windows.h>
#include <wincrypt.h>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <utility>

namespace quantum {

void to_json(nlohmann::json& json, const CredentialSummary& item) {
    json = {
        {"id", item.id},
        {"origin", item.origin},
        {"username", item.username},
        {"createdUnix", item.createdUnix},
        {"updatedUnix", item.updatedUnix}
    };
}

void from_json(const nlohmann::json& json, CredentialSummary& item) {
    item.id = json.value("id", 0ULL);
    item.origin = json.value("origin", "");
    item.username = json.value("username", "");
    item.createdUnix = json.value("createdUnix", 0LL);
    item.updatedUnix = json.value("updatedUnix", 0LL);
}

CredentialVault::CredentialVault(std::filesystem::path filePath)
    : filePath_(std::move(filePath)) {
}

void CredentialVault::load() {
    records_.clear();
    nextId_ = 1;
    std::ifstream in(filePath_);
    if (!in) {
        return;
    }
    try {
        nlohmann::json json;
        in >> json;
        if (!json.is_array()) {
            return;
        }
        for (const auto& item : json) {
            Record record;
            record.summary = item.value("summary", CredentialSummary{});
            record.encryptedPassword = item.value("encryptedPassword", "");
            if (record.summary.id != 0) {
                records_.push_back(record);
                nextId_ = std::max(nextId_, record.summary.id + 1);
            }
        }
    } catch (...) {
        records_.clear();
        nextId_ = 1;
    }
}

void CredentialVault::save() const {
    std::filesystem::create_directories(filePath_.parent_path());
    nlohmann::json json = nlohmann::json::array();
    for (const auto& record : records_) {
        json.push_back({
            {"summary", record.summary},
            {"encryptedPassword", record.encryptedPassword}
        });
    }
    const auto temp = filePath_.string() + ".tmp";
    {
        std::ofstream out(temp, std::ios::trunc);
        out << json.dump(2);
    }
    if (std::filesystem::exists(filePath_)) {
        std::filesystem::remove(filePath_);
    }
    std::filesystem::rename(temp, filePath_);
}

std::uint64_t CredentialVault::upsert(std::string origin, std::string username, std::string password) {
    auto it = std::find_if(records_.begin(), records_.end(), [&](const Record& record) {
        return record.summary.origin == origin && record.summary.username == username;
    });

    const std::int64_t now = nowUnix();
    if (it == records_.end()) {
        Record record;
        record.summary.id = nextId_++;
        record.summary.origin = std::move(origin);
        record.summary.username = std::move(username);
        record.summary.createdUnix = now;
        record.summary.updatedUnix = now;
        record.encryptedPassword = protect(std::move(password));
        records_.push_back(std::move(record));
        save();
        return records_.back().summary.id;
    }

    it->summary.updatedUnix = now;
    it->encryptedPassword = protect(std::move(password));
    save();
    return it->summary.id;
}

void CredentialVault::remove(std::uint64_t id) {
    records_.erase(
        std::remove_if(records_.begin(), records_.end(), [&](const Record& record) {
            return record.summary.id == id;
        }),
        records_.end());
    save();
}

std::vector<CredentialSummary> CredentialVault::records() const {
    std::vector<CredentialSummary> summaries;
    summaries.reserve(records_.size());
    for (const auto& record : records_) {
        summaries.push_back(record.summary);
    }
    return summaries;
}

std::optional<CredentialSecret> CredentialVault::reveal(std::uint64_t id) const {
    auto it = std::find_if(records_.begin(), records_.end(), [&](const Record& record) {
        return record.summary.id == id;
    });
    if (it == records_.end()) {
        return std::nullopt;
    }
    auto plain = unprotect(it->encryptedPassword);
    if (!plain) {
        return std::nullopt;
    }
    return CredentialSecret{it->summary, *plain};
}

std::string CredentialVault::protect(std::string plainText) {
    DATA_BLOB input{};
    input.pbData = reinterpret_cast<BYTE*>(plainText.data());
    input.cbData = static_cast<DWORD>(plainText.size());
    DATA_BLOB output{};
    if (!CryptProtectData(&input, L"Quantum credential", nullptr, nullptr, nullptr, 0, &output)) {
        return {};
    }
    std::vector<unsigned char> bytes(output.pbData, output.pbData + output.cbData);
    LocalFree(output.pbData);
    return base64Encode(bytes);
}

std::optional<std::string> CredentialVault::unprotect(const std::string& encrypted) {
    std::vector<unsigned char> bytes = base64Decode(encrypted);
    if (bytes.empty()) {
        return std::nullopt;
    }
    DATA_BLOB input{};
    input.pbData = bytes.data();
    input.cbData = static_cast<DWORD>(bytes.size());
    DATA_BLOB output{};
    if (!CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr, 0, &output)) {
        return std::nullopt;
    }
    std::string plain(reinterpret_cast<char*>(output.pbData), reinterpret_cast<char*>(output.pbData) + output.cbData);
    LocalFree(output.pbData);
    return plain;
}

std::string CredentialVault::base64Encode(const std::vector<unsigned char>& bytes) {
    if (bytes.empty()) {
        return {};
    }
    DWORD size = 0;
    CryptBinaryToStringA(bytes.data(), static_cast<DWORD>(bytes.size()), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, nullptr, &size);
    std::string output(size, '\0');
    if (!CryptBinaryToStringA(bytes.data(), static_cast<DWORD>(bytes.size()), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, output.data(), &size)) {
        return {};
    }
    if (!output.empty() && output.back() == '\0') {
        output.pop_back();
    }
    return output;
}

std::vector<unsigned char> CredentialVault::base64Decode(const std::string& text) {
    if (text.empty()) {
        return {};
    }
    DWORD size = 0;
    if (!CryptStringToBinaryA(text.c_str(), static_cast<DWORD>(text.size()), CRYPT_STRING_BASE64, nullptr, &size, nullptr, nullptr)) {
        return {};
    }
    std::vector<unsigned char> bytes(size);
    if (!CryptStringToBinaryA(text.c_str(), static_cast<DWORD>(text.size()), CRYPT_STRING_BASE64, bytes.data(), &size, nullptr, nullptr)) {
        return {};
    }
    bytes.resize(size);
    return bytes;
}

std::int64_t CredentialVault::nowUnix() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

} // namespace quantum
