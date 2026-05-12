#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <cstdint>

#ifdef _WIN32
#include <winsock2.h>
#else
#define SOCKET int
#endif

namespace sovereign {

enum class CollapseDecision : uint8_t { WAIT = 0, GO = 1, AVOID = 2 };

inline const char* decision_str(CollapseDecision d) {
    switch (d) {
        case CollapseDecision::GO: return "GO";
        case CollapseDecision::AVOID: return "AVOID";
        default: return "WAIT";
    }
}

struct PhysicsCollapseResult {
    CollapseDecision decision;
    double confidence;
    double coherence;
    double energy;
    double drift;
    double uncertainty_product;
    double octahedral_alignment;
    double octahedral_penalty;
    double fisher_distance;
    double fractional_memory;
    double delay_error;
    double pt_risk;
    double topology_break;
    double resonance_gate;
    double action_margin;
    int koopman_regime;
};

struct VivaldiCoord {
    double x = 0.0, y = 0.0, z = 0.0;
    double error = 1.0;
};

struct SonPeer {
    sockaddr_in addr;
    double   rtt_ms;
    std::chrono::steady_clock::time_point last_seen;
    uint32_t run_id;
    bool     is_lighthouse    = false;
    double   trust_weight     = 1.0;
    double   cpu_load_pct     = 50.0;
    uint64_t remote_collapses = 0;
    VivaldiCoord coord;
    double   bayesian_prior   = 0.5;
};

struct KineticEntry {
    CollapseDecision decision;
    double confidence;
    double kinetic_proxy;
    double energy;
    double coherence;
    uint64_t ts_ns;
};

} // namespace sovereign
