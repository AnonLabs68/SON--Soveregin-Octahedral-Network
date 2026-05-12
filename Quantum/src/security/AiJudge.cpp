#include "security/AiJudge.h"
#include "core/Log.h"
#include <numeric>
#include <algorithm>
#include <cmath>

namespace quantum {

AiJudge::AiJudge(const std::string& modelPath) 
    : env_(ORT_LOGGING_LEVEL_WARNING, "QuantumGuard") {
    try {
        Ort::SessionOptions sessionOptions;
        sessionOptions.SetIntraOpNumThreads(1);
        sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
        
        std::wstring wPath(modelPath.begin(), modelPath.end());
        session_ = std::make_unique<Ort::Session>(env_, wPath.c_str(), sessionOptions);
        startupLog("AI Judge loaded successfully: " + modelPath);
    } catch (const std::exception& e) {
        startupLog("AI Judge failed to load: " + std::string(e.what()));
    }
}

float AiJudge::predict(const std::string& url, const std::string& axTreeJson) {
    if (!session_) return 0.0f;

    try {
        auto features = extractFeatures(url, axTreeJson);
        
        Ort::MemoryInfo memoryInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        std::vector<int64_t> inputShape = {1, static_cast<int64_t>(features.size())};
        
        Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
            memoryInfo, features.data(), features.size(), inputShape.data(), inputShape.size());
            
        const char* inputNames[] = {"input"};
        const char* outputNames[] = {"output"};
        
        auto outputTensors = session_->Run(Ort::RunOptions{nullptr}, inputNames, &inputTensor, 1, outputNames, 1);
        float* outputData = outputTensors.front().GetTensorMutableData<float>();
        
        return outputData[0];
    } catch (...) {
        return 0.0f;
    }
}

std::vector<float> AiJudge::extractFeatures(const std::string& url, const std::string& axTree) {
    std::vector<float> f(128, 0.0f);
    
    // Feature 0: URL Length
    f[0] = static_cast<float>(url.length()) / 256.0f;
    
    // Feature 1: Number of dots in URL
    f[1] = static_cast<float>(std::count(url.begin(), url.end(), '.')) / 10.0f;
    
    // Feature 2: Is HTTPS
    f[2] = (url.find("https://") == 0) ? 1.0f : 0.0f;
    
    // Feature 3: AXTree complexity (number of nodes)
    f[3] = static_cast<float>(std::count(axTree.begin(), axTree.end(), '{')) / 500.0f;
    
    // ... Additional 124 features would be structural shingles or keyword frequencies ...
    // For now, we pad with zero or dummy entropy
    for(size_t i = 4; i < 128; ++i) {
        f[i] = 0.0f; 
    }
    
    return f;
}

} // namespace quantum
