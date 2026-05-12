#include "agent/AgentBackend.h"
#include "browser/MockBrowserView.h"
#include "core/AppPaths.h"
#include "profile/CredentialVault.h"
#include "profile/ProfileStore.h"
#include "ui/QuantumApp.h"
#include "ui/QuantumTheme.h"

#if QUANTUM_ENABLE_WEBVIEW2
#include "browser/WebView2BrowserView.h"
#endif

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include <GLFW/glfw3.h>
#if defined(_WIN32)
#include <GLFW/glfw3native.h>
#include <windows.h>
#endif

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

#include "core/Log.h"

namespace {

void glfwErrorCallback(int error, const char* description) {
    std::fprintf(stderr, "GLFW error %d: %s\n", error, description);
    quantum::startupLog("GLFW error " + std::to_string(error) + ": " + description);
}

void loadFonts() {
    ImGuiIO& io = ImGui::GetIO();
    const char* segoe = "C:\\Windows\\Fonts\\segoeui.ttf";
    if (std::filesystem::exists(segoe)) {
        io.Fonts->AddFontFromFileTTF(segoe, 16.0f);
        const char* segoeBold = "C:\\Windows\\Fonts\\segoeuib.ttf";
        if (std::filesystem::exists(segoeBold)) {
            io.Fonts->AddFontFromFileTTF(segoeBold, 17.0f);
        }
    } else {
        io.Fonts->AddFontDefault();
    }
}

} // namespace

int runQuantum() {
    quantum::startupLog("Quantum startup begin");
#if defined(_WIN32)
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
#endif

    glfwSetErrorCallback(glfwErrorCallback);
    if (!glfwInit()) {
        quantum::startupLog("glfwInit failed");
        return 1;
    }

    quantum::startupLog("GLFW initialized");
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);

    // Dynamic Resolution Sensing
    GLFWmonitor* primary = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(primary);
    
    // Set for your 1366x720 display, but adjust if smaller
    int windowW = 1366;
    int windowH = 720;

    GLFWwindow* window = glfwCreateWindow(windowW, windowH, "Quantum", nullptr, nullptr);
    if (!window) {
        quantum::startupLog("OpenGL context fallback");
        glfwDefaultWindowHints();
        glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        window = glfwCreateWindow(windowW, windowH, "Quantum", nullptr, nullptr);
    }
    if (!window) {
        quantum::startupLog("glfwCreateWindow failed");
        glfwTerminate();
        return 1;
    }

    // Auto-Center
    glfwSetWindowPos(window, (mode->width - windowW) / 2, (mode->height - windowH) / 2);

#if defined(_WIN32)
    // DEEP FIX: Force WS_CLIPCHILDREN to prevent parent painting over WebView2
    HWND hwnd = glfwGetWin32Window(window);
    LONG style = GetWindowLong(hwnd, GWL_STYLE);
    style |= WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
    SetWindowLong(hwnd, GWL_STYLE, style);
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
#endif

    quantum::startupLog("Window created");
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    loadFonts();
    quantum::applyQuantumStyle();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    quantum::AppPaths paths = quantum::AppPaths::discover();
    paths.ensure();
    quantum::ProfileStore profile(paths);
    profile.load();
    quantum::CredentialVault vault(paths.dataDir() / "vault.json");
    vault.load();

    std::unique_ptr<quantum::IBrowserView> browser;
#if QUANTUM_ENABLE_WEBVIEW2
    browser = std::make_unique<quantum::WebView2BrowserView>(glfwGetWin32Window(window));
#else
    browser = std::make_unique<quantum::MockBrowserView>();
#endif
    browser->initialize();
    quantum::startupLog("Browser initialized");
    SetEnvironmentVariableW(L"WEBVIEW2_BROWSER_EXECUTABLE_FOLDER", NULL);

    quantum::AgentBackend agent;
    quantum::QuantumApp app(window, *browser, agent, profile, vault);

    while (!glfwWindowShouldClose(window)) {
        // High-Efficiency Wait: Don't burn CPU if nothing is happening
        // We wait up to 1/60th of a second for events
        glfwWaitEventsTimeout(0.016); 

        ImGui_ImplGlfw_NewFrame();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui::NewFrame();

        app.frame();

        ImGui::Render();
        int displayW, displayH;
        glfwGetFramebufferSize(window, &displayW, &displayH);
        glViewport(0, 0, displayW, displayH);
        glClearColor(0.05f, 0.06f, 0.07f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    quantum::startupLog("Quantum shutdown");
    profile.save();
    vault.save();
    browser.reset();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();

#if defined(_WIN32)
    CoUninitialize();
#endif
    return 0;
}

int main(int, char**) {
    return runQuantum();
}

#if defined(_WIN32)
int APIENTRY WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    return runQuantum();
}
#endif
