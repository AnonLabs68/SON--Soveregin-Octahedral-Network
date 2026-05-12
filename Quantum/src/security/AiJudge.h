#pragma once
#include <string>
#include <vector>
#include <memory>
#include <onnxruntime_cxx_api.h>

namespace quantum {

class AiJudge {
public:
    AiJudge(const std::string& modelPath);
    
    // Returns a score between 0.0 (Safe) and 1.0 (Malicious)
    float predict(const std::string& url, const std::string& axTreeJson);

private:
    std::unique_ptr<Ort::Session> session_;
    Ort::Env env_;
    
    // Feature extraction
    std::vector<float> extractFeatures(const std::string& url, const std::string& axTree);
};

} // namespace quantum
