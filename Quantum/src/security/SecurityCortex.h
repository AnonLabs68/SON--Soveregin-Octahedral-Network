#pragma once

#include "security/BloomFilter.h"
#include "security/MinHash.h"
#include <memory>
#include <string>
#include <vector>

namespace quantum {
 
class AiJudge;
class BloomFilter;
class MinHash;

enum class SecurityLevel {
    Safe,
    Suspicious,
    Malicious
};

struct SecurityReport {
    SecurityLevel level;
    std::string reason;
};

class SecurityCortex {
public:
    SecurityCortex();
    ~SecurityCortex(); // Explicit destructor for unique_ptr

    SecurityReport assessUrl(const std::string& url);
    SecurityReport assessPageContent(const std::string& axTreeJson);

    // Dynamic training: Learn from new threats
    void learnThreat(const std::string& signature, bool isDna = false);

private:
    std::unique_ptr<BloomFilter> urlFilter_;
    std::unique_ptr<BloomFilter> patternFilter_;
    
    // Sovereign guard stack: URL/pattern Bloom filters, structural DNA, and AI judge.
    std::unique_ptr<MinHash> dnaEngine_;
    std::unique_ptr<AiJudge> aiJudge_;
    std::vector<std::vector<uint64_t>> maliciousDnaSignatures_;

    void loadDefaultSignatures();
};

} // namespace quantum
