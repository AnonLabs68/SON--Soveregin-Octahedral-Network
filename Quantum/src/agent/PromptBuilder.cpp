#include "agent/PromptBuilder.h"

#include <sstream>
#include <nlohmann/json.hpp>

namespace quantum {

std::string PromptBuilder::buildAgentPrompt(const PromptContext& context) {
    std::ostringstream prompt;
    prompt
        << "You are Quantum Assistant, a very fast browser automation agent inside a C++ Chromium shell.\n"
        << "Return strict JSON only. No markdown. No commentary outside JSON.\n"
        << "Schema: {\"thought\":\"internal planning and reasoning\",\"response\":\"DIRECT answer to user or short progress update\","
        << "\"actions\":[{\"type\":\"navigate|click|type|fill_form|press_enter|wait\","
        << "\"url\":\"optional\",\"selector\":\"optional CSS selector from snapshot\","
        << "\"value\":\"optional text\",\"fields\":[{\"selector\":\"CSS\",\"value\":\"text\"}],\"seconds\":0.5}],"
        << "\"memory\":\"optional short thing worth remembering\"}\n\n"
        << "Operating style:\n"
        << "- Use 'thought' for your internal reasoning (not shown to user).\n"
        << "- Use 'response' ONLY for the final answer or a high-level summary if taking actions.\n"
        << "- If actions are present, 'response' should be scrupulously brief.\n"
        << "- Be fast and incremental. Prefer 1-3 actions, then wait for a fresh page snapshot.\n\n"
        << "- Treat every task as a checklist: observe, select exact target, act, verify on the next snapshot.\n"
        << "- If a page is settling, has a spinner, or just changed state, prefer a short wait action instead of more clicks.\n"
        << "- Do not spam repeated clicks, form fills, or navigation. If the last action did not visibly change the page, verify or ask.\n"
        << "- If the page is ambiguous, ask one precise question in response and return no actions.\n"
        << "- Prefer deterministic browser actions over vague status prose.\n\n"
        << "Example Valid JSON Output:\n"
        << "{\n"
        << "  \"thought\": \"I see the search box. I will type the query and press enter.\",\n"
        << "  \"response\": \"Searching for the target.\",\n"
        << "  \"actions\": [\n"
        << "    { \"type\": \"type\", \"selector\": \"#search\", \"value\": \"TradingView\" },\n"
        << "    { \"type\": \"press_enter\" }\n"
        << "  ]\n"
        << "}\n\n"
        << "Rules:\n"
        << "- Return ONLY JSON. No markdown backticks.\n"
        << "- Use selectors exactly from the snapshot.\n"
        << "- Never invent selectors.\n\n"
        << "- Never claim success until the snapshot or visible page text supports it.\n\n"
        << "Available tools:\n"
        << context.toolDescriptions << "\n"
        << "Institutional History (Memory & Actions):\n"
        << (context.memory.empty() ? "- none\n" : context.memory) << "\n"
        << "Current URL: " << context.currentUrl << "\n"
        << "Current title: " << context.currentTitle << "\n"
        << "Page State: " << (context.snapshotJson.find("\"loading\":true") != std::string::npos ? "STILL LOADING (WAIT RECOMMENDED)" : "READY") << "\n"
        << "User task: " << context.userTask << "\n"
        << "AXTree Snapshot (Interactive Nodes):\n";
    
    // Parse the JSON manually to format it nicely for the AI context (Elite Compact View)
    try {
        auto j = nlohmann::json::parse(context.snapshotJson);
        if (j.contains("nodes") && j["nodes"].is_array()) {
            for (const auto& n : j["nodes"]) {
                prompt << "[" << n.value("id", 0) << "] " 
                       << n.value("role", "") << " \"" << n.value("name", "") << "\" ("
                       << n["bounds"].value("x", 0) << "," << n["bounds"].value("y", 0) << ") -> "
                       << n.value("selector", "") << "\n";
            }
        }
        
        std::string pageText = j.value("text", "");
        if (pageText.length() > 2000) pageText = pageText.substr(0, 2000) + "... [truncated]";
        prompt << "\nPage Text Excerpt:\n" << pageText << "\n";
    } catch (...) {
        prompt << context.snapshotJson << "\n";
    }
    return prompt.str();
}

} // namespace quantum
