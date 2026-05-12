#include "agent/GeminiClient.h"

#include "simdjson.h"

#include <windows.h>
#include <winhttp.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <sstream>

namespace quantum {

namespace {

std::string getenvString(const char* name, std::string fallback = {}) {
    size_t size = 0;
    getenv_s(&size, nullptr, 0, name);
    if (size == 0) {
        return fallback;
    }
    std::string value(size, '\0');
    getenv_s(&size, value.data(), value.size(), name);
    if (!value.empty() && value.back() == '\0') {
        value.pop_back();
    }
    return value.empty() ? fallback : value;
}

std::wstring toWide(const std::string& value) {
    if (value.empty()) {
        return {};
    }
    const int length = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
    std::wstring wide(static_cast<size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), wide.data(), length);
    return wide;
}

BrowserAction::Type actionTypeFromString(std::string_view type) {
    if (type == "navigate") return BrowserAction::Type::Navigate;
    if (type == "click") return BrowserAction::Type::Click;
    if (type == "type") return BrowserAction::Type::TypeText;
    if (type == "fill_form") return BrowserAction::Type::FillForm;
    if (type == "press_enter") return BrowserAction::Type::PressEnter;
    return BrowserAction::Type::Wait;
}

std::string jsonEscape(const std::string& input) {
    std::string output;
    for (auto c : input) {
        switch (c) {
            case '\"': output += "\\\""; break;
            case '\\': output += "\\\\"; break;
            case '\b': output += "\\b"; break;
            case '\f': output += "\\f"; break;
            case '\n': output += "\\n"; break;
            case '\r': output += "\\r"; break;
            case '\t': output += "\\t"; break;
            default:
                if ('\x00' <= c && c <= '\x1f') {
                    char buf[8];
                    sprintf_s(buf, "\\u%04x", c);
                    output += buf;
                } else {
                    output += c;
                }
        }
    }
    return output;
}

std::string imageMimeType(const std::filesystem::path& path) {
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".webp") return "image/webp";
    if (ext == ".gif") return "image/gif";
    return "image/png";
}

std::string base64Encode(const std::vector<unsigned char>& data) {
    static constexpr char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((data.size() + 2) / 3) * 4);
    for (size_t i = 0; i < data.size(); i += 3) {
        const unsigned int b0 = data[i];
        const unsigned int b1 = (i + 1 < data.size()) ? data[i + 1] : 0;
        const unsigned int b2 = (i + 2 < data.size()) ? data[i + 2] : 0;
        out.push_back(alphabet[(b0 >> 2) & 0x3F]);
        out.push_back(alphabet[((b0 << 4) | (b1 >> 4)) & 0x3F]);
        out.push_back(i + 1 < data.size() ? alphabet[((b1 << 2) | (b2 >> 6)) & 0x3F] : '=');
        out.push_back(i + 2 < data.size() ? alphabet[b2 & 0x3F] : '=');
    }
    return out;
}

std::optional<std::string> readImageBase64(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return std::nullopt;
    }
    std::vector<unsigned char> bytes;
    char byte = 0;
    while (input.get(byte)) {
        bytes.push_back(static_cast<unsigned char>(byte));
    }
    if (bytes.empty()) {
        return std::nullopt;
    }
    return base64Encode(bytes);
}

} // namespace

GeminiClient::GeminiClient()
    : apiKey_(getenvString("GEMINI_API_KEY", "")),
      model_(getenvString("QUANTUM_GEMINI_MODEL", "gemini-2.5-flash")) {
    // Also inject into environment for child processes
    _putenv_s("GEMINI_API_KEY", apiKey_.c_str());
}

void GeminiClient::setModel(const std::string& modelName) {
    model_ = modelName;
}

std::vector<std::string> GeminiClient::availableModels() const {
    return {
        "gemini-3-flash-preview",
        "gemini-3.1-flash-lite-preview",
        "gemini-2.5-flash",
        "gemini-2.5-flash-lite"
    };
}

void GeminiClient::setApiKey(const std::string& key) {
    apiKey_ = key;
    _putenv_s("GEMINI_API_KEY", apiKey_.c_str());
}

std::string GeminiClient::maskedApiKey() const {
    if (apiKey_.empty()) {
        return "";
    }
    if (apiKey_.size() <= 8) {
        return std::string(apiKey_.size(), '*');
    }
    return apiKey_.substr(0, 4) + std::string(apiKey_.size() - 8, '*') + apiKey_.substr(apiKey_.size() - 4);
}

AgentPlan GeminiClient::planFromSnapshot(
    const std::string& userTask,
    const std::string& snapshotJson,
    const std::string& currentUrl,
    const std::string& currentTitle) const {

    std::ostringstream prompt;
    prompt
        << "You are Quantum Assistant, a fast browser automation agent inside a Chromium browser, developed by AnonLabs.\n"
        << "Return strict JSON only. No markdown.\n"
        << "Schema: {\"thought\":\"internal planning and reasoning\",\"response\":\"DIRECT answer to user or short progress update\","
        << "\"actions\":[{\"type\":\"navigate|click|type|press_enter|wait\","
        << "\"url\":\"optional\",\"selector\":\"optional CSS selector from snapshot\","
        << "\"value\":\"optional text\",\"seconds\":0.5}]}\n"
        << "Rules:\n"
        << "- Use 'thought' for your internal reasoning (not shown to user).\n"
        << "- Use 'response' ONLY for the final answer or a high-level summary if taking actions.\n"
        << "- If actions are present, 'response' should be scrupulously brief.\n"
        << "- Prefer one or two actions at a time.\n"
        << "- Use selectors exactly from the snapshot when clicking or typing.\n"
        << "- Do not invent selectors.\n"
        << "- If the task asks a question, answer from the page text and use no actions.\n"
        << "- If the task needs navigation and no selector is needed, use navigate.\n\n"
        << "Current URL: " << currentUrl << "\n"
        << "Current title: " << currentTitle << "\n"
        << "User task: " << userTask << "\n"
        << "AXTree-lite snapshot JSON:\n" << snapshotJson << "\n";

    return planFromPrompt(prompt.str(), userTask, currentUrl);
}

AgentPlan GeminiClient::planFromPrompt(
    const std::string& prompt,
    const std::string& fallbackTask,
    const std::string& currentUrl,
    const std::vector<GeminiImageAttachment>& images) const {

    if (!hasApiKey()) {
        return localFallbackPlan(fallbackTask, currentUrl);
    }

    std::string apiError = "unresponsive";
    const auto text = postGenerateContent(prompt, &apiError, images);
    if (!text) {
        AgentPlan plan = localFallbackPlan(fallbackTask, currentUrl);
        plan.error = "Gemini API Error: " + apiError;
        plan.response = "Model Arsenal Exhausted (" + apiError + "). Monitoring browser for recovery.";
        return plan;
    }

    AgentPlan plan = parsePlanText(*text);
    plan.usedRemoteModel = true;
    if (plan.response.empty()) {
        plan.response = "I parsed the page and prepared the next step.";
    }
    return plan;
}

std::string GeminiClient::model() const {
    return model_;
}

bool GeminiClient::hasApiKey() const {
    return !apiKey_.empty();
}

AgentPlan GeminiClient::localFallbackPlan(const std::string& userTask, const std::string& currentUrl) const {
    AgentPlan plan;
    plan.usedRemoteModel = false;

    const auto normalized = normalizeUrl(userTask);
    if (userTask.find('.') != std::string::npos || userTask.rfind("http", 0) == 0) {
        plan.response = "Planner fallback active. I can still navigate directly and verify from the page.";
        BrowserAction action;
        action.type = BrowserAction::Type::Navigate;
        action.url = normalized;
        plan.actions.push_back(action);
    } else {
        plan.response =
            "Planner fallback active. I need a URL, search target, or one precise page action to continue safely.";
    }

    return plan;
}

std::optional<std::string> GeminiClient::postGenerateContent(
    const std::string& prompt,
    std::string* outError,
    const std::vector<GeminiImageAttachment>& images) const {
    static ULONGLONG lastRequestTime = 0;
    ULONGLONG currentTime = GetTickCount64();
    if (currentTime - lastRequestTime < 5000) {
        Sleep(5000 - (DWORD)(currentTime - lastRequestTime));
    }
    lastRequestTime = GetTickCount64();

    static size_t currentKeyIdx = 0;
    std::vector<std::string> keys = { 
        apiKey_, 
        apiKey_ // Placeholder for Second Key
    };

    std::vector<std::string> fallbackModels = availableModels();
    // Move selected model to front
    for (size_t i = 0; i < fallbackModels.size(); ++i) {
        if (fallbackModels[i] == model_) {
            std::swap(fallbackModels[0], fallbackModels[i]);
            break;
        }
    }

    for (size_t k = 0; k < keys.size(); ++k) {
        size_t idx = (currentKeyIdx + k) % keys.size();
        const std::string& key = keys[idx];
        bool keyRateLimited = false;

        for (const auto& modelName : fallbackModels) {
            std::string escapedPrompt = jsonEscape(prompt);
            std::string parts = "{\"text\":\"" + escapedPrompt + "\"}";
            for (const auto& image : images) {
                const auto encoded = readImageBase64(image.path);
                if (!encoded) {
                    continue;
                }
                parts += ",{\"inline_data\":{\"mime_type\":\"" + imageMimeType(image.path) + "\",\"data\":\"" + *encoded + "\"}}";
            }
            std::string body = "{\"contents\":[{\"role\":\"user\",\"parts\":[" + parts + "]}],\"generationConfig\":{\"temperature\":0.15,\"responseMimeType\":\"application/json\"}}";
            
            std::string modelPath = (modelName.rfind("models/", 0) == 0) ? modelName : "models/" + modelName;
            const std::wstring path = toWide("/v1beta/" + modelPath + ":generateContent");
            const std::wstring headers = toWide("Content-Type: application/json\r\nx-goog-api-key: " + key + "\r\n");

            HINTERNET session = WinHttpOpen(L"Quantum/1.7", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, nullptr, nullptr, 0);
            if (!session) continue;
            HINTERNET connect = WinHttpConnect(session, L"generativelanguage.googleapis.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
            if (!connect) { WinHttpCloseHandle(session); continue; }
            HINTERNET request = WinHttpOpenRequest(connect, L"POST", path.c_str(), nullptr, nullptr, nullptr, WINHTTP_FLAG_SECURE);
            if (!request) { WinHttpCloseHandle(connect); WinHttpCloseHandle(session); continue; }

            BOOL ok = WinHttpSendRequest(request, headers.c_str(), (DWORD)headers.size(), (void*)body.data(), (DWORD)body.size(), (DWORD)body.size(), 0);
            ok = ok && WinHttpReceiveResponse(request, nullptr);

            DWORD statusCode = 0;
            DWORD statusCodeSize = sizeof(statusCode);
            WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &statusCode, &statusCodeSize, nullptr);

            if (statusCode == 429) {
                keyRateLimited = true;
                WinHttpCloseHandle(request); WinHttpCloseHandle(connect); WinHttpCloseHandle(session);
                break; // Try next key
            }

            std::string response;
            DWORD available = 0;
            while (WinHttpQueryDataAvailable(request, &available) && available > 0) {
                std::string chunk(available, '\0');
                DWORD read = 0;
                WinHttpReadData(request, chunk.data(), available, &read);
                chunk.resize(read);
                response += chunk;
            }

            WinHttpCloseHandle(request); WinHttpCloseHandle(connect); WinHttpCloseHandle(session);

            if (statusCode != 200) {
                if (outError) {
                    simdjson::dom::parser p;
                    simdjson::dom::element d;
                    if (p.parse(response).get(d) == simdjson::SUCCESS) {
                        try {
                            *outError = std::string(d["error"]["message"]);
                        } catch (...) { *outError = "HTTP " + std::to_string(statusCode); }
                    } else {
                        *outError = "HTTP " + std::to_string(statusCode);
                    }
                }
                continue;
            }

            if (response.empty()) continue;

            simdjson::dom::parser parser;
            simdjson::dom::element doc;
            if (parser.parse(response).get(doc) == simdjson::SUCCESS) {
                try {
                    return std::string(doc["candidates"].at(0)["content"]["parts"].at(0)["text"]);
                } catch (...) {}
            }
        }
        if (keyRateLimited) {
            currentKeyIdx = (idx + 1) % keys.size();
        } else {
            break; // No need to try other keys if models are just failing normally
        }
    }
    return std::nullopt;
}

AgentPlan GeminiClient::parsePlanText(const std::string& rawText) const {
    AgentPlan plan;
    std::string text = rawText;
    
    // Attempt to find JSON block if wrapped in markdown or text
    size_t start = text.find("{");
    size_t end = text.rfind("}");
    if (start != std::string::npos && end != std::string::npos && end > start) {
        text = text.substr(start, end - start + 1);
    }

    simdjson::dom::parser parser;
    simdjson::dom::element doc;
    auto error = parser.parse(text).get(doc);
    
    if (error) {
        // Parsing failed - find the most likely text response if JSON is missing
        if (start != std::string::npos && start > 2) {
            plan.response = rawText.substr(0, start);
            // Clean up trailing punctuation or noise
            size_t lastDot = plan.response.find_last_of(".!?");
            if (lastDot != std::string::npos) plan.response = plan.response.substr(0, lastDot + 1);
        } else {
            plan.response = "I've analyzed the page state and am preparing the next kinetic action.";
        }
        return plan;
    }

    try {
        std::string_view resp;
        if (doc["response"].get(resp) == simdjson::SUCCESS) {
            plan.response = std::string(resp);
        }
        
        std::string_view th;
        if (doc["thought"].get(th) == simdjson::SUCCESS) {
            plan.thought = std::string(th);
        }
        
        simdjson::dom::array actions;
        if (doc["actions"].get(actions) == simdjson::SUCCESS) {
            for (simdjson::dom::element item : actions) {
                BrowserAction action;
                std::string_view type;
                if (item["type"].get(type) == simdjson::SUCCESS) {
                    action.type = actionTypeFromString(type);
                }
                
                std::string_view sel;
                if (item["selector"].get(sel) == simdjson::SUCCESS) action.selector = std::string(sel);
                
                std::string_view val;
                if (item["value"].get(val) == simdjson::SUCCESS) action.value = std::string(val);
                
                std::string_view url;
                if (item["url"].get(url) == simdjson::SUCCESS) action.url = std::string(url);
                
                double sec;
                if (item["seconds"].get(sec) == simdjson::SUCCESS) action.seconds = sec;
                
                plan.actions.push_back(action);
            }
        }
    } catch (...) {
        plan.response = "I had trouble processing the plan, but I'm staying focused on the task.";
    }
    return plan;
}

} // namespace quantum
