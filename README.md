# SON: Sovereign Octahedral Network

**A native C++ retail trading OS where every active terminal can become a node.**

Sovereign is an AnonLabs hackathon build for Solana retail traders who want the speed and cohesion of an institutional workstation without living inside fragmented web dashboards. The app combines a native OpenGL trading terminal, Sovereign Oracle/Cortex market intelligence, Quantum agentic browser research, a wallet/node passport concept, and SON, a peer-assisted network layer for sharing observed market state.

Website: https://sovereignoctahedral.online/

## What It Is

Sovereign is not just a chart UI. It is a local trading workstation designed around one thesis:

**The terminal is the node.**

Every running terminal can render, observe, verify, and eventually contribute market state back to the network according to the user's compute policy. The project focuses on reducing trader workflow latency, chart hydration load, duplicated API calls, and AI inference cost while keeping the experience native and responsive.

## Core Modules

### Native Trading Terminal

- C++ / OpenGL3 / Dear ImGui interface.
- Multi-panel terminal with chart, order book, order entry, time and sales, portfolio/risk, TP/SL controls, and strategy buttons.
- Custom one-click strategy controls inspired by professional terminals: TWAP, VWAP, Iceberg, post-only, IOC, FOK, and user strategy execution.
- Dynamic portfolio and order panels for resizing during live demos.

### Sovereign Oracle and Cortex

Oracle/Cortex is the forensic market intelligence layer. It is designed to fuse:

- Derivatives state.
- Funding and open interest.
- Long/short ratios.
- Taker flow.
- Order book imbalance.
- Market structure.
- Birdeye trader discovery.
- Solana wallet activity.
- MEV / Jito pattern signals.

The goal is to turn scattered market telemetry into one actionable view beside execution.

### Quantum Agentic Browser

Quantum is the built-in agentic browser and research surface.

- Browser context, page state, and chat live inside the terminal workflow.
- Image-only attachments and browser snapshots can be sent to the assistant.
- API settings are configurable by the user.
- The orchestration layer waits for page settling and reduces wasteful model calls during browser transitions.
- Current live synthesis uses Gemini Flash; the local Sovereign Alpha path is still in training.

### HexaField

HexaField is the project's strict 48-byte packet protocol used for low-overhead native message passing experiments. The protocol is intentionally compact and deterministic so that trading signals, node telemetry, receipts, and market-state deltas can be represented without heavy JSON-style overhead in the hot path.

### SON Network Layer

SON, the Sovereign Octahedral Network, is the peer-assisted market state layer.

In the current demo direction, active nodes do not invent synthetic candles for new users. Instead, a node that has been active can share the real candle state it observed while online. New nodes can hydrate faster from the network, reducing duplicate fetch and render load.

The target model is:

- Local terminal observes market data.
- Node records the candle/state range it actually witnessed.
- New or recovering nodes request missing ranges from active peers.
- Beacon/data-center nodes can provide higher-availability routing later.
- User compute contribution is policy-controlled and visible to the user.

### Wallet / Node Passport

The wallet is treated as more than a payment surface. It is the planned identity and persistence layer for:

- Node identity.
- Layouts and saves.
- Browser and agent history.
- API configuration references.
- Receipts and proofs.
- Compute contribution policy.

## AI Strategy

Sovereign uses cloud AI only where it helps the demo today. The longer-term architecture is cost-aware:

- Gemini Flash currently powers reliable live synthesis.
- A local Sovereign Alpha model path exists and is still under training.
- The intended direction is to move more agentic behavior and market analysis onto the user's machine.
- Deterministic physics/risk fallback logic is used so the system has a non-cloud path for uncertainty, volatility, coherence, and action collapse decisions.

## Repository Layout

```text
native_core/        Native C++ OpenGL terminal, Oracle/Cortex, SON/HexaField experiments
Quantum/            C++ Quantum browser and assistant surface
*.md                Submission docs, whitepaper, scripts, and judge notes
```

Large model/build/demo artifacts are not meant to be pushed directly to GitHub. See `LARGE_ASSETS_MANIFEST.md` and `GITHUB_PACKAGE_README.md`.

## Build

### Native Terminal

```powershell
cmake -S native_core -B native_core/build
cmake --build native_core/build --config Release
.\native_core\build\Release\sovereign_native_core.exe
```

### Quantum Browser

```powershell
cmake -S Quantum -B Quantum/build
cmake --build Quantum/build --config Release
.\Quantum\build\Release\Quantum.exe
```

### Optional Environment Keys

```powershell
$env:GEMINI_API_KEY="your-gemini-key"
$env:BIRDEYE_API_KEY="your-birdeye-key"
```

Keys are runtime configuration only. Do not commit secrets, wallet keypairs, browser profiles, or local identity files.

## Submission Docs

- `SOVEREIGN_OCTAHEDRAL_NETWORK_WHITEPAPER_FINAL.md`
- `JUDGE_TECHNICAL_BRIEF.md`
- `SUBMISSION_NAME_AND_NARRATIVE.md`
- `GITHUB_PACKAGE_README.md`

## Current Status

This is a hackathon prototype with a large native codebase and live demo surface. Some pieces are production-oriented foundations, while others are clearly marked demo/training paths. The strongest implemented surface is the native terminal, Oracle/Cortex concept, Quantum research loop, HexaField packet work, and SON active-node candle sharing direction.

## Safety

This repository is not financial advice and is not production trading infrastructure. Use devnet/test environments for demos. Rotate any API keys that were ever hardcoded locally before public release.
