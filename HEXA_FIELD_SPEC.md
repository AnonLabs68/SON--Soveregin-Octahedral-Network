# HexaField Protocol v1

## Purpose
HexaField v1 defines a machine-native communication contract for the 6-agent sovereign runtime.

Goals:
- Replace text-heavy inter-agent exchange on the hot path.
- Keep deterministic collapse behavior across runs and hardware.
- Enable C++ first execution with Python as a thin shell.
- Preserve proof/audit compatibility with receipt and chain tooling.

Scope:
- Quantum-inspired behavior on classical hardware.
- Fixed-size binary structs with optional hex rendering for logging.

Non-goals:
- Literal quantum computation.
- Human-readable runtime payloads in the hot path.

---

## Agent Topology
Canonical 6 agents:
1. `quantum-router`
2. `data-retriever`
3. `proof-verifier`
4. `nowcast-forecast`
5. `defai-strategist`
6. `risk-critic`

Agent IDs (`u8`):
- `0x01` router
- `0x02` retriever
- `0x03` verifier
- `0x04` nowcast
- `0x05` strategist
- `0x06` critic

---

## Packet Model
Every hot-path tick operates over fixed-size packets.

### 1) Header (`16 bytes`)
```
byte 0   : version_major (u8) = 1
byte 1   : version_minor (u8) = 0
byte 2   : packet_type (u8)
byte 3   : flags (u8)
byte 4-7 : run_id (u32 LE)
byte 8-11: seq_no (u32 LE)
byte 12  : agent_id (u8)
byte 13  : route_code (u8)
byte 14  : ttl_ticks (u8)
byte 15  : header_crc8 (u8 xor of bytes 0..14)
```

### 2) Field Body (`32 bytes`)
All values are fixed-point for deterministic math.

```
byte 16-17 : belief_go_q15 (u16 LE)
byte 18-19 : belief_wait_q15 (u16 LE)
byte 20-21 : belief_avoid_q15 (u16 LE)
byte 22-23 : risk_q15 (u16 LE)
byte 24-25 : proof_q15 (u16 LE)
byte 26-27 : latency_q15 (u16 LE)
byte 28-29 : phase_q15 (u16 LE)          // mapped [0, 2pi)
byte 30-31 : coherence_hint_q15 (u16 LE)
byte 32-33 : uncertainty_x_q15 (u16 LE)  // software observable X
byte 34-35 : uncertainty_p_q15 (u16 LE)  // software observable P
byte 36-37 : confidence_q15 (u16 LE)
byte 38-39 : reserved0 (u16 LE)
byte 40-41 : reserved1 (u16 LE)
byte 42-43 : reserved2 (u16 LE)
byte 44-45 : reserved3 (u16 LE)
byte 46-47 : body_crc16 (u16 LE, xor-folded)
```

Total packet size: `48 bytes`.

### 3) Packet Types (`packet_type`)
- `0x10`: AgentFieldUpdate
- `0x20`: CollapseRequest
- `0x21`: CollapseResult
- `0x30`: ProofAttach
- `0x40`: HealthTelemetry

### 4) Route Codes (`route_code`)
- `0x00` unknown
- `0x01` btc_price
- `0x02` top_headlines
- `0x03` weather
- `0x04` market_research

---

## Fixed-Point Conventions
`q15` maps `[0.0, 1.0] -> [0, 32767]`.

Encoding:
- `q15 = round(clamp(v, 0, 1) * 32767)`

Decoding:
- `v = q15 / 32767.0`

Phase mapping for `phase_q15`:
- `phase_rad = (phase_q15 / 32767.0) * 2pi`

---

## Quantum-Inspired Collapse Math (Deterministic)

### 1) Agent complex amplitude
For agent `i`:
- `c_i = confidence_i`
- `phi_i = phase_i`
- `a_i = c_i * exp(j * phi_i)`

### 2) Interference aggregate
- `A = sum_i (w_i * a_i)` where `sum_i w_i = 1`
- `I = min(1, |A|^2)`

`I` is coherence strength in `[0,1]`.

### 3) Binary uncertainty gate (software analogue)
Let:
- `DX = sqrt(pX * (1 - pX))`
- `DP = sqrt(pP * (1 - pP))`
- `U = DX * DP`
- `penalty_uncertainty = max(0, k_u - U)`

Where `pX`, `pP` are decoded probabilities from `uncertainty_x_q15`, `uncertainty_p_q15`.

### 4) Collapse energy rule
- `E = alpha*(1 - I) + beta*risk + gamma*(1 - proof) + delta*latency + eta*penalty_uncertainty`

Decision trigger policy:
- If `E < E_go` and `belief_go` is dominant -> `GO`
- Else if `E > E_avoid` or `proof < proof_min` -> `AVOID`
- Else -> `WAIT`

### 5) Tie-break order (strict)
1. lower `E`
2. higher `proof`
3. higher `coherence I`
4. lower `risk`
5. lower `latency`
6. lexicographic decision order: `GO > WAIT > AVOID`

This order is mandatory for reproducibility.

---

## Default Constants (v1)
- `alpha = 0.38`
- `beta  = 0.24`
- `gamma = 0.20`
- `delta = 0.08`
- `eta   = 0.10`
- `k_u = 0.18`
- `E_go = 0.32`
- `E_avoid = 0.72`
- `proof_min = 0.45`

All constants must be emitted in trace metadata for audit.

---

## Determinism Requirements
- No random numbers in collapse kernel.
- No floating branch dependence on hardware-specific approximations.
- Prefer fixed-point arithmetic internally after decode.
- All packet parsing failures must return explicit status codes.
- Same packet stream + same constants => same collapse output.

---

## Proof-Carrying Extension (Packet Type `0x30`)
ProofAttach payload fields (mapped from existing receipt system):
- `payload_sha256` (32 bytes)
- `public_key_fingerprint` (8 bytes)
- `signature_fingerprint` (8 bytes)
- `prev_receipt_hash_fingerprint` (8 bytes)

Full receipt remains in existing JSON artifacts. Packet carries compact integrity anchors.

---

## Test Vectors (v1)

### Vector A: coherent GO
Inputs (decoded):
- Agent confidences approx: `[0.82, 0.79, 0.88, 0.76, 0.74, 0.81]`
- phase cluster near alignment (small phase spread)
- risk `0.18`, proof `0.92`, latency `0.12`, `pX=0.61`, `pP=0.57`

Expected:
- high `I` (>= 0.75)
- low `E` (< `E_go`)
- collapse `GO`

### Vector B: proof-failure AVOID
Inputs:
- moderate coherence
- risk `0.31`, proof `0.22`, latency `0.10`

Expected:
- `proof < proof_min`
- collapse `AVOID`

### Vector C: mixed WAIT
Inputs:
- medium coherence (`I` around `0.45-0.60`)
- risk `0.42`, proof `0.64`, latency `0.20`
- uncertainty penalty active (`U < k_u`)

Expected:
- `E_go <= E <= E_avoid`
- collapse `WAIT`

---

## Migration Plan Hooks
Phase linkage:
1. `orchestrate_v3` consumes `CollapseRequest` packets and outputs `CollapseResult`.
2. Runtime fast lane uses HexaField packets as primary hot path.
3. Existing receipt/on-chain pipeline remains source of truth for full artifacts.

Compatibility:
- Keep `orchestrate_v2` as fallback path under feature flag until parity passes.
- Add C++/Python parity tests from these vectors before enabling by default.

---

## Language for Submissions
Recommended claim wording:
- "HexaField is a machine-native, fixed-size agent communication protocol for deterministic multi-agent collapse."
- "The system is quantum-inspired (interference + uncertainty-gated energy collapse) on classical hardware."
- "Trust is enforced through cryptographic receipts and chain verification, with optional on-chain anchoring."
