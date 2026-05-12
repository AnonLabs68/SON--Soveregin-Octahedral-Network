cmake_minimum_required(VERSION 3.10)
project(sovereign_native_core)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(ONNXRUNTIME_DIR "E:/onnx_sdk" CACHE PATH "Path to local ONNX Runtime installation")
set(QUANTUM_DIR "${CMAKE_SOURCE_DIR}/../Quantum")
set(QUANTUM_SRC_DIR "${QUANTUM_DIR}/src")

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

set(QUANTUM_ENABLE_WEBVIEW2 ON)
if(WIN32 AND QUANTUM_ENABLE_WEBVIEW2)
    FetchContent_Declare(
        webview2_sdk
        URL https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/1.0.3912.50
        DOWNLOAD_NAME Microsoft.Web.WebView2.1.0.3912.50.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(webview2_sdk)

    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(QUANTUM_WEBVIEW2_ARCH x64)
    else()
        set(QUANTUM_WEBVIEW2_ARCH x86)
    endif()
endif()

set(IMGUI_DIR ${imgui_SOURCE_DIR})
include_directories(
    ${QUANTUM_SRC_DIR}
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${ONNXRUNTIME_DIR}/include
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

set(QUANTUM_SOURCES
    ${QUANTUM_SRC_DIR}/agent/AgentBackend.cpp
    ${QUANTUM_SRC_DIR}/agent/AgentMemory.cpp
    ${QUANTUM_SRC_DIR}/agent/AgentTrace.cpp
    ${QUANTUM_SRC_DIR}/agent/AxTreeSnapshot.cpp
    ${QUANTUM_SRC_DIR}/agent/BrowserTools.cpp
    ${QUANTUM_SRC_DIR}/agent/GeminiClient.cpp
    ${QUANTUM_SRC_DIR}/agent/PromptBuilder.cpp
    ${QUANTUM_SRC_DIR}/agent/SafetyPolicy.cpp
    ${QUANTUM_SRC_DIR}/agent/ToolRegistry.cpp
    ${QUANTUM_SRC_DIR}/browser/BrowserModel.cpp
    ${QUANTUM_SRC_DIR}/browser/MockBrowserView.cpp
    ${QUANTUM_SRC_DIR}/browser/PageSnapshot.cpp
    ${QUANTUM_SRC_DIR}/core/AppPaths.cpp
    ${QUANTUM_SRC_DIR}/security/SecurityCortex.cpp
    ${QUANTUM_SRC_DIR}/security/AiJudge.cpp
    ${QUANTUM_SRC_DIR}/security/MinHash.cpp
    ${QUANTUM_SRC_DIR}/core/StringUtils.cpp
    ${QUANTUM_SRC_DIR}/profile/CommandIndex.cpp
    ${QUANTUM_SRC_DIR}/profile/CredentialVault.cpp
    ${QUANTUM_SRC_DIR}/profile/ProfileStore.cpp
    ${QUANTUM_SRC_DIR}/ui/QuantumApp.cpp
    ${QUANTUM_SRC_DIR}/ui/QuantumTheme.cpp
    ${QUANTUM_SRC_DIR}/ui/QuantumWidgets.cpp
)

if(WIN32 AND QUANTUM_ENABLE_WEBVIEW2)
    list(APPEND QUANTUM_SOURCES ${QUANTUM_SRC_DIR}/browser/WebView2BrowserView.cpp)
endif()

add_executable(sovereign_native_core
    main.cpp
    src/network_engine.cpp
    IngestionService.cpp
    ${IMGUI_SOURCES}
    ${QUANTUM_SOURCES}
)

target_compile_definitions(sovereign_native_core PRIVATE
    QUANTUM_ENABLE_WEBVIEW2=1
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    GLFW_EXPOSE_NATIVE_WIN32
)

target_link_libraries(sovereign_native_core PRIVATE
    ${ONNXRUNTIME_DIR}/lib/onnxruntime.lib
    glfw
    simdjson::simdjson
    nlohmann_json::nlohmann_json
    opengl32 user32 gdi32 shell32 winhttp ws2_32 comctl32 comdlg32 crypt32 dwmapi ole32 shlwapi
)

if(WIN32 AND QUANTUM_ENABLE_WEBVIEW2)
    target_include_directories(sovereign_native_core PRIVATE
        ${webview2_sdk_SOURCE_DIR}/build/native/include
    )
    set(QUANTUM_WEBVIEW2_LIB
        ${webview2_sdk_SOURCE_DIR}/build/native/${QUANTUM_WEBVIEW2_ARCH}/WebView2Loader.dll.lib
    )
    set(QUANTUM_WEBVIEW2_DLL
        ${webview2_sdk_SOURCE_DIR}/build/native/${QUANTUM_WEBVIEW2_ARCH}/WebView2Loader.dll
    )
    target_link_libraries(sovereign_native_core PRIVATE ${QUANTUM_WEBVIEW2_LIB})
endif()

add_custom_command(TARGET sovereign_native_core POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${ONNXRUNTIME_DIR}/lib/onnxruntime.dll"
    "$<TARGET_FILE_DIR:sovereign_native_core>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${QUANTUM_WEBVIEW2_DLL}"
    "$<TARGET_FILE_DIR:sovereign_native_core>"
)

if(MSVC)
    target_compile_options(sovereign_native_core PRIVATE /EHsc /O2 /Oi /GL /fp:fast)
    set_target_properties(sovereign_native_core PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

