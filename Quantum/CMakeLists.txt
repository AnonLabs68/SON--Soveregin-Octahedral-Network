cmake_minimum_required(VERSION 3.25)

project(QuantumBrowser LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(QUANTUM_ENABLE_WEBVIEW2 "Enable the Windows WebView2 browser backend" ON)

include(FetchContent)

FetchContent_Declare(
    glfw
    URL https://github.com/glfw/glfw/archive/refs/tags/3.4.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

FetchContent_Declare(
    imgui
    URL https://github.com/ocornut/imgui/archive/refs/tags/v1.91.9b.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(imgui)

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui PUBLIC glfw)

FetchContent_Declare(
    simdjson
    URL https://github.com/simdjson/simdjson/archive/refs/tags/v3.9.1.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(simdjson)

FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/archive/refs/tags/v3.11.3.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

find_package(OpenGL REQUIRED)

set(QUANTUM_SOURCES
    src/main.cpp
    src/agent/AgentBackend.cpp
    src/agent/AgentMemory.cpp
    src/agent/AgentTrace.cpp
    src/agent/AxTreeSnapshot.cpp
    src/agent/BrowserTools.cpp
    src/agent/GeminiClient.cpp
    src/agent/PromptBuilder.cpp
    src/agent/SafetyPolicy.cpp
    src/agent/ToolRegistry.cpp
    src/browser/BrowserModel.cpp
    src/browser/MockBrowserView.cpp
    src/browser/PageSnapshot.cpp
    src/core/AppPaths.cpp
    src/security/SecurityCortex.cpp
    src/security/AiJudge.cpp
    src/security/MinHash.cpp
    src/core/StringUtils.cpp
    src/profile/CommandIndex.cpp
    src/profile/CredentialVault.cpp
    src/profile/ProfileStore.cpp
    src/ui/QuantumApp.cpp
    src/ui/QuantumTheme.cpp
    src/ui/QuantumWidgets.cpp
)

if(WIN32 AND QUANTUM_ENABLE_WEBVIEW2)
    FetchContent_Declare(
        webview2_sdk
        URL https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/1.0.3912.50
        DOWNLOAD_NAME Microsoft.Web.WebView2.1.0.3912.50.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(webview2_sdk)

    if(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|arm64|aarch64")
        set(QUANTUM_WEBVIEW2_ARCH arm64)
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(QUANTUM_WEBVIEW2_ARCH x64)
    else()
        set(QUANTUM_WEBVIEW2_ARCH x86)
    endif()

    list(APPEND QUANTUM_SOURCES src/browser/WebView2BrowserView.cpp)
endif()

# ONNX Runtime for Layer 3 (AI Judge)
include(FetchContent)
FetchContent_Declare(
  onnxruntime
  URL https://github.com/microsoft/onnxruntime/releases/download/v1.17.3/onnxruntime-win-x64-1.17.3.zip
)
FetchContent_MakeAvailable(onnxruntime)

add_executable(Quantum WIN32
    ${QUANTUM_SOURCES}
)

# Link ONNX Runtime
target_include_directories(Quantum PRIVATE ${onnxruntime_SOURCE_DIR}/include)
target_link_libraries(Quantum PRIVATE 
    ${onnxruntime_SOURCE_DIR}/lib/onnxruntime.lib
)

# Copy DLLs to output
add_custom_command(TARGET Quantum POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${onnxruntime_SOURCE_DIR}/lib/onnxruntime.dll"
    $<TARGET_FILE_DIR:Quantum>
)

target_include_directories(Quantum PRIVATE src)
target_compile_definitions(Quantum PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    GLFW_EXPOSE_NATIVE_WIN32
)

target_link_libraries(Quantum PRIVATE
    imgui
    simdjson::simdjson
    nlohmann_json::nlohmann_json
    OpenGL::GL
)

if(WIN32)
    target_link_libraries(Quantum PRIVATE
        comctl32
        comdlg32
        crypt32
        dwmapi
        ole32
        shell32
        shlwapi
        user32
        winhttp
    )
endif()

if(WIN32 AND QUANTUM_ENABLE_WEBVIEW2)
    target_compile_definitions(Quantum PRIVATE QUANTUM_ENABLE_WEBVIEW2=1)
    target_include_directories(Quantum PRIVATE
        ${webview2_sdk_SOURCE_DIR}/build/native/include
    )
    set(QUANTUM_WEBVIEW2_LIB
        ${webview2_sdk_SOURCE_DIR}/build/native/${QUANTUM_WEBVIEW2_ARCH}/WebView2Loader.dll.lib
    )
    set(QUANTUM_WEBVIEW2_DLL
        ${webview2_sdk_SOURCE_DIR}/build/native/${QUANTUM_WEBVIEW2_ARCH}/WebView2Loader.dll
    )
    target_link_libraries(Quantum PRIVATE ${QUANTUM_WEBVIEW2_LIB})
    add_custom_command(TARGET Quantum POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${QUANTUM_WEBVIEW2_DLL}
            $<TARGET_FILE_DIR:Quantum>
    )
else()
    target_compile_definitions(Quantum PRIVATE QUANTUM_ENABLE_WEBVIEW2=0)
endif()
