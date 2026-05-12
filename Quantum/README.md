# Quantum

Quantum is a C++20, Dear ImGui, OpenGL3 browser shell with a Windows WebView2 Chromium renderer and an async Gemini-powered agent backend.

The current scaffold is optimized for fast iteration:

- custom dark Quantum UI inspired by Comet-style agent browsers
- tab/address/navigation shell drawn in Dear ImGui
- real per-tab WebView2 browser controllers on Windows
- fallback mock browser backend if WebView2 is disabled
- tab close buttons, session restore, command palette, history, bookmarks, settings
- custom downloads surface with pause/resume/cancel hooks
- site permission surface for camera, microphone, geolocation, notifications, clipboard, file, storage, and related requests
- screenshot capture through WebView2 `CapturePreview`
- encrypted DPAPI credential vault foundation
- AXTree-lite page snapshots generated inside the page
- Gemini agent planner with structured browser actions and a safety policy gate
- browser tool registry for navigate, click, type, fill_form, press_enter, and wait
- non-blocking agent execution so the UI stays responsive

## Model

Quantum defaults to Google's documented Gemini 3 Flash Preview model:

```powershell
$env:GEMINI_API_KEY="your-key"
$env:QUANTUM_GEMINI_MODEL="gemini-3-flash-preview"
```

If your account exposes a newer alias, set `QUANTUM_GEMINI_MODEL` to that value.

## Build

Install Visual Studio 2022 with the Desktop development with C++ workload, then run:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
.\build\Release\Quantum.exe
```

If you want to install the required Windows C++ build tools with winget:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\install_windows_prereqs.ps1
```

The CMake project downloads GLFW, Dear ImGui, nlohmann/json, and the WebView2 SDK from release packages. No Git executable is required.

## Backend Shape

The fast action path is:

1. WebView2 loads the real page.
2. Quantum injects an AXTree-lite collector script.
3. The agent sends current URL, title, visible text, and interactive nodes to Gemini.
4. Gemini returns JSON actions such as `navigate`, `click`, `type`, `fill_form`, `press_enter`, and `wait`.
5. Quantum executes actions through WebView2 JavaScript.

This gives us the speed of a page-side accessibility snapshot without needing to fork Chromium.

## Useful Commands

Inside Quantum:

- `Ctrl+K` opens the command palette.
- `history`, `bookmarks`, `downloads`, `permissions`, `trace`, and `settings` open internal surfaces.
- `screenshot` saves the active page to the Quantum data directory.

## Verification

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\verify.ps1
.\build\Release\Quantum.exe
```
