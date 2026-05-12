#include "agent/BrowserTools.h"

#include <nlohmann/json.hpp>

namespace quantum {

namespace {

std::string escapeForJs(const std::string& value) {
    return nlohmann::json(value).dump();
}

} // namespace

ToolRegistry createBrowserToolRegistry() {
    ToolRegistry registry;

    registry.registerTool(
        {
            "navigate",
            "Load a URL or search query in the active tab.",
            R"json({"type":"navigate","url":"https://example.com"})json"
        },
        [](const BrowserAction& action, IBrowserView& browser) {
            browser.navigate(action.url);
        });

    registry.registerTool(
        {
            "click",
            "Click a visible element using a selector from the AXTree-lite snapshot.",
            R"json({"type":"click","selector":"button:nth-of-type(1)"})json"
        },
        [](const BrowserAction& action, IBrowserView& browser) {
            const std::string script =
                "(() => { const el = document.querySelector(" + escapeForJs(action.selector) + ");"
                "if (el) { el.scrollIntoView({block:'center', inline:'center'}); el.click(); return true; }"
                "return false; })();";
            browser.executeScript(script, nullptr);
        });

    registry.registerTool(
        {
            "type",
            "Type text into an input-like element using a selector from the snapshot.",
            R"json({"type":"type","selector":"input:nth-of-type(1)","value":"hello"})json"
        },
        [](const BrowserAction& action, IBrowserView& browser) {
            const std::string script =
                "(() => { const el = document.querySelector(" + escapeForJs(action.selector) + ");"
                "if (!el) return false;"
                "el.scrollIntoView({block:'center', inline:'center'});"
                "el.focus();"
                "if ('value' in el) el.value = " + escapeForJs(action.value) + ";"
                "else el.textContent = " + escapeForJs(action.value) + ";"
                "el.dispatchEvent(new Event('input', {bubbles:true}));"
                "el.dispatchEvent(new Event('change', {bubbles:true}));"
                "return true; })();";
            browser.executeScript(script, nullptr);
        });

    registry.registerTool(
        {
            "fill_form",
            "Fill multiple form fields from selectors in the snapshot.",
            R"json({"type":"fill_form","fields":[{"selector":"input:nth-of-type(1)","value":"hello"}]})json"
        },
        [](const BrowserAction& action, IBrowserView& browser) {
            std::string script = "(() => { const results = []; ";
            for (const auto& field : action.fields) {
                script += "(()=>{ const el = document.querySelector(" + escapeForJs(field.selector) + ");"
                    "if (!el) { results.push(false); return; }"
                    "el.scrollIntoView({block:'center', inline:'center'});"
                    "if ('value' in el) el.value = " + escapeForJs(field.value) + ";"
                    "else el.textContent = " + escapeForJs(field.value) + ";"
                    "el.dispatchEvent(new Event('input', {bubbles:true}));"
                    "el.dispatchEvent(new Event('change', {bubbles:true}));"
                    "results.push(true); })();";
            }
            script += " return results; })();";
            browser.executeScript(script, nullptr);
        });

    registry.registerTool(
        {
            "press_enter",
            "Send Enter to a selected element or the focused element.",
            R"json({"type":"press_enter","selector":"input:nth-of-type(1)"})json"
        },
        [](const BrowserAction& action, IBrowserView& browser) {
            const std::string selector = action.selector.empty() ? "null" : escapeForJs(action.selector);
            const std::string script =
                "(() => { const sel = " + selector + ";"
                "const el = sel ? document.querySelector(sel) : document.activeElement;"
                "if (!el) return false;"
                "el.focus && el.focus();"
                "el.dispatchEvent(new KeyboardEvent('keydown', {key:'Enter', code:'Enter', bubbles:true}));"
                "el.dispatchEvent(new KeyboardEvent('keyup', {key:'Enter', code:'Enter', bubbles:true}));"
                "return true; })();";
            browser.executeScript(script, nullptr);
        });

    registry.registerTool(
        {
            "wait",
            "Pause briefly while the page changes.",
            R"json({"type":"wait","seconds":0.5})json"
        },
        [](const BrowserAction&, IBrowserView&) {
        });

    return registry;
}

} // namespace quantum
