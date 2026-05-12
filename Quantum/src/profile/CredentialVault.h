#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace quantum {

struct CredentialSummary {
    std::uint64_t id = 0;
    std::string origin;
    std::string username;
    std::int64_t createdUnix = 0;
    std::int64_t updatedUnix = 0;
};

struct CredentialSecret {
    CredentialSummary summary;
    std::string password;
};

class CredentialVault {
public:
    explicit CredentialVault(std::filesystem::path filePath);

    void load();
    void save() const;
    std::uint64_t upsert(std::string origin, std::string username, std::string password);
    void remove(std::uint64_t id);
    std::vector<CredentialSummary> records() const;
    std::optional<CredentialSecret> reveal(std::uint64_t id) const;

private:
    struct Record {
        CredentialSummary summary;
        std::string encryptedPassword;
    };

    static std::string protect(std::string plainText);
    static std::optional<std::string> unprotect(const std::string& encrypted);
    static std::string base64Encode(const std::vector<unsigned char>& bytes);
    static std::vector<unsigned char> base64Decode(const std::string& text);
    static std::int64_t nowUnix();

    std::filesystem::path filePath_;
    std::vector<Record> records_;
    std::uint64_t nextId_ = 1;
};

} // namespace quantum

