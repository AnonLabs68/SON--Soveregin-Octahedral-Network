#pragma once
#include <atomic>
#include <cmath>
#include <vector>
#include <algorithm>

/**
 * SOVEREIGN ENTROPY CORE (PHYSICS-BASED AI)
 * ----------------------------------------
 * Linearizing market chaos into observable physical quantities.
 * Uses Kinetic Energy and Information Entropy to govern the 512-instrument mesh.
 */

namespace sovereign::physics {

struct InstrumentPhysics {
    std::atomic<double> kinetic_energy{0.0};
    std::atomic<double> pressure{0.0};
    std::atomic<double> entropy{0.0};
    
    double last_price{0.0};
    double velocity{0.0};
    uint64_t last_ts{0};
};

class EntropyEngine {
private:
    std::vector<InstrumentPhysics> field;

public:
    EntropyEngine() : field(512) {}

    static EntropyEngine& instance() {
        static EntropyEngine inst;
        return inst;
    }

    /**
     * COLLAPSE THE WAVEFUNCTION
     * ------------------------
     * Updates the physical state of an instrument based on a new tick.
     */
    void update(uint16_t id, double price, double volume, uint64_t ts) {
        if (id >= 512) return;
        auto& p = field[id];

        double dt = (ts > p.last_ts) ? (double)(ts - p.last_ts) / 1000.0 : 0.001;
        if (dt <= 0) dt = 0.001;

        // 1. Calculate Velocity (Price Change / Time)
        double dP = (p.last_price > 0) ? (price - p.last_price) : 0.0;
        p.velocity = dP / dt;

        // 2. Kinetic Energy: E = 0.5 * m * v^2 (Volume is "Mass")
        double ke = 0.5 * volume * (p.velocity * p.velocity);
        p.kinetic_energy.store(ke, std::memory_order_relaxed);

        // 3. Information Entropy (Simplified Shannon approx)
        // Measures how "surprising" the move is relative to the kinetic average
        double surprise = std::abs(p.velocity) / (std::sqrt(ke + 0.0001) + 1.0);
        double entropy_inc = std::log2(1.0 + surprise);
        
        double current_entropy = p.entropy.load(std::memory_order_relaxed);
        p.entropy.store(current_entropy * 0.95 + entropy_inc * 0.05, std::memory_order_relaxed);

        p.last_price = price;
        p.last_ts = ts;
    }

    double get_kinetic_energy(uint16_t id) const {
        return (id < 512) ? field[id].kinetic_energy.load(std::memory_order_relaxed) : 0.0;
    }

    double get_entropy(uint16_t id) const {
        return (id < 512) ? field[id].entropy.load(std::memory_order_relaxed) : 0.0;
    }

    /**
     * DISCOVERY SCANNER
     * ----------------
     * Identifies "Undiscovered Gems" by finding instruments with 
     * High Entropy but Low Kinetic Energy (Early breakouts).
     */
    bool is_gem_forming(uint16_t id) const {
        if (id >= 512) return false;
        double e = get_entropy(id);
        double k = get_kinetic_energy(id);
        return (e > 0.8 && k < 100.0); // High info, but "quiet" volume = Smart Money accumulation
    }
};

} // namespace sovereign::physics
