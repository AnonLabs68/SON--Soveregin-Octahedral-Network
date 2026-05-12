# Judge Technical Brief

## Project

**AnonLabs Sovereign** is a native C++ retail trading terminal where each active terminal can become a node in the Sovereign Octahedral Network.

## What Is Built

- Native OpenGL3/ImGui trading terminal.
- Quantum embedded agentic browser.
- Wallet/node passport.
- Sovereign Oracle / Cortex market intelligence.
- Sovereign Alpha in-training local model artifacts.
- Gemini Flash current demo layer for live chat and high-fidelity synthesis.
- HexaField 48-byte packet protocol.
- Kinetic hot/cold state model.
- Active-node real candle backfill.
- Quantized ONNX local inference and small-model direction, with Gemini Flash used for the current demo's reliable live language layer.
- Deterministic physics fallback.
- MT5-style programmable control layer with one-click execution and custom strategies.
- Solana devnet identity/receipt direction.

## Code-Backed Highlights

- `native_core/main.cpp`: `run_physics_collapse` implements the local decision kernel with stochastic drift, OU mean reversion, Avellaneda-Stoikov risk, Koopman regime detection, Fisher distance, Hamiltonian energy, and action margin.
- `native_core/main.cpp`: `process_hexafield_request` routes compact HexaField packets and uses ONNX only when needed, with physics fallback.
- `native_core/main.cpp`: PoPhys counters record local collapse events as proof-like decision receipts.
- `native_core/main.cpp`: Kinetic Ice writes compact cold-state electrons for durable state.
- `native_core/main.cpp`: SON includes lighthouses, peer health, Vivaldi coordinates, Sybil collision penalties, quorum, and Matrioshka workers.
- `native_core/main.cpp`: `MarketDataEngine` and `GlobalDataHub` publish active-node real candle backfill.
- `native_core/main.cpp`: wallet identity stores node id, devnet owner, compute contribution, hot/cold policy, saves, API vault, agent memory, and receipt settings.
- `native_core/SovereignOracle.hpp`: Oracle fuses derivatives, order book, funding, OI, long/short ratios, taker flow, market structure, Birdeye top traders, Solana wallet activity, and MEV/Jito indicators.
- `native_core/SovereignOracle.hpp`: local ONNX model path exists, with optional cloud AI failover.
- `native_core/sovereign_alpha_model_v1`: in-training Sovereign Alpha model artifact set, including `model.safetensors`, tokenizer, and config.
- `native_core/sovereign_alpha_onnx/sovereign_alpha_quantized`: quantized ONNX export path for local runtime integration.
- `defai_model.onnx` and `train_defai_onnx.py`: distilled decision model and export pipeline for fast route/action classification.
- `user_strategy.py` and the terminal algo selector: custom strategy slot beside TWAP, VWAP, Iceberg, TIF, Post-Only, IOC/FOK, and one-click long/short buttons.
- `Quantum/src/security/SecurityCortex.cpp`: app-owned browser security keeps URL, pattern, structural, and AI-judge checks.

## Why It Matters

Most retail trading tools are web dashboards. Sovereign is a local execution OS:

- fast native rendering
- Gemini Flash for current live language reliability, with local Alpha training to reduce future API reliance
- flexible strategy control like a programmable workstation
- compact packets instead of heavy JSON paths
- real state sharing between active nodes
- wallet identity linked to node identity
- Solana receipts as an anchoring path

## Honest Scope

This is not a finished mainnet. The demo proves the local terminal, protocol architecture, Oracle intelligence layer, browser, identity model, and node-assisted data direction. The next milestone is multi-machine SON measurement with real peer scheduling and receipt anchoring.
