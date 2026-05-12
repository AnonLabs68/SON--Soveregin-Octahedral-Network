#include "security/SecurityCortex.h"
#include "security/AiJudge.h"
#include "core/Log.h"

namespace quantum {

SecurityCortex::SecurityCortex() {
    urlFilter_ = std::make_unique<BloomFilter>(4096, 0.0005);
    patternFilter_ = std::make_unique<BloomFilter>(4096, 0.0005);
    dnaEngine_ = std::make_unique<MinHash>(64);
    aiJudge_ = std::make_unique<AiJudge>("quantum_guard.onnx");
    loadDefaultSignatures();
    startupLog("Quantum SecurityCortex owns page safety; native browser reputation stack disabled in WebView2.");
}

SecurityCortex::~SecurityCortex() = default;

SecurityReport SecurityCortex::assessUrl(const std::string& url) {
    if (urlFilter_->contains(url)) {
        return { SecurityLevel::Malicious, "Blacklisted URL (Bloom Filter)" };
    }
    return { SecurityLevel::Safe, "" };
}

SecurityReport SecurityCortex::assessPageContent(const std::string& axTreeJson) {
    // Level 1: Pattern check
    if (patternFilter_->contains(axTreeJson)) {
        return { SecurityLevel::Malicious, "Malicious DOM Pattern detected" };
    }

    // Level 2: DNA Analysis (MinHash)
    auto currentSketch = dnaEngine_->computeSketch(axTreeJson);
    for (const auto& maliciousDna : maliciousDnaSignatures_) {
        float similarity = dnaEngine_->computeSimilarity(currentSketch, maliciousDna);
        if (similarity > 0.85f) {
            return { SecurityLevel::Malicious, "DNA MATCH: This page is structurally identical to a known phishing template." };
        }
    }

    // Level 3: AI Behavioral Analysis (Quantum-owned judge)
    if (aiJudge_) {
        float aiScore = aiJudge_->predict("", axTreeJson);
        if (aiScore > 0.98f) {
            return { SecurityLevel::Suspicious, "AI Behavioral Warning: Page structure looks abnormal." };
        }
    }

    return { SecurityLevel::Safe, "" };
}

void SecurityCortex::loadDefaultSignatures() {
    urlFilter_->add("http://malicious-drainer.xyz");
    urlFilter_->add("https://phantom-wallet-fix.com");
    
    patternFilter_->add("drain_all_tokens");
}

} // namespace quantum
