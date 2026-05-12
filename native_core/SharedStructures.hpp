#pragma once
#include <atomic>
#include <vector>
#include <mutex>
#include <map>
#include <algorithm>
#include <utility>

// --- CORE MARKET DATA PACKET ---
struct MarketUpdate {
    int type; // 0=depth, 1=trade
    bool isBid;
    int instrument_id;
    double price;
    double volume;
    uint64_t ts_ms;
};

// --- LOCK-FREE QUEUE (HIGH-PERFORMANCE CIRCULAR BUFFER) ---
template <typename T>
class LockFreeQueue {
private:
    std::vector<T> buffer;
    std::atomic<size_t> head{0};
    std::atomic<size_t> tail{0};
    size_t capacity;

public:
    LockFreeQueue(size_t size = 1024 * 1024) : capacity(size) {
        buffer.resize(size);
    }

    bool push(const T& item) {
        size_t h = head.load(std::memory_order_acquire);
        size_t t = tail.load(std::memory_order_relaxed);
        size_t next_t = (t + 1) % capacity;
        if (next_t == h) return false;
        buffer[t] = item;
        tail.store(next_t, std::memory_order_release);
        return true;
    }

    bool pop(T& item) {
        size_t h = head.load(std::memory_order_relaxed);
        size_t t = tail.load(std::memory_order_acquire);
        if (h == t) return false;
        item = buffer[h];
        head.store((h + 1) % capacity, std::memory_order_release);
        return true;
    }
};

// --- ORDER BOOK STRUCTURES ---
struct OrderBookSnapshot {
    uint64_t sequence;
    std::vector<std::pair<double, double>> bids;
    std::vector<std::pair<double, double>> asks;
};

class OrderBook {
public:
    std::map<double, double, std::greater<double>> bids;
    std::map<double, double> asks;
    std::mutex mtx;

    OrderBookSnapshot snapshots_[2];
    alignas(64) std::atomic<int> active_read_{0};
    uint64_t write_seq_ = 0;

    void apply(const MarketUpdate& u, bool force_clear = false) {
        std::lock_guard<std::mutex> lock(mtx);
        if (force_clear) {
            if (u.isBid) bids.clear();
            else asks.clear();
        }
        
        if (u.isBid) {
            if (u.volume <= 1e-10) bids.erase(u.price); // Precision-safe erasure
            else bids[u.price] = u.volume;
        } else {
            if (u.volume <= 1e-10) asks.erase(u.price);
            else asks[u.price] = u.volume;
        }
    }

    void publish_snapshot() {
        int read_idx = active_read_.load(std::memory_order_acquire);
        int write_idx = 1 - read_idx;
        auto& snap = snapshots_[write_idx];
        snap.sequence = ++write_seq_;
        {
            std::lock_guard<std::mutex> lock(mtx);
            // Optimization: Only copy top 150 levels for UI/Calc efficiency (increased for Titan Scan)
            snap.bids.clear();
            auto bit = bids.begin();
            for (int i=0; i<150 && bit != bids.end(); ++i, ++bit) snap.bids.push_back(*bit);

            snap.asks.clear();
            auto ait = asks.begin();
            for (int i=0; i<150 && ait != asks.end(); ++i, ++ait) snap.asks.push_back(*ait);
        }
        active_read_.store(write_idx, std::memory_order_release);
    }

    // --- TITAN LIQUIDITY ENGINE: Find the thickest walls in the book ---
    std::vector<std::pair<double, double>> get_titans(bool is_bid, double mid, float sensitivity = 50.0f, int count = 25) {
        std::vector<std::pair<double, double>> titans;
        {
            std::lock_guard<std::mutex> lock(mtx);
            if (is_bid) {
                for (auto const& p : bids) titans.push_back({p.first, p.second});
            } else {
                for (auto const& p : asks) titans.push_back({p.first, p.second});
            }
        }
        
        // Smart Algo: Distance-Adjusted Liquidity Score (DALS)
        // Score = Volume / (1.0 + %_distance_from_mid * sensitivity)
        auto get_score = [&](double price, double volume) {
            double dist_pct = std::abs(price - mid) / (mid > 0 ? mid : 1.0);
            return volume / (1.0 + dist_pct * (double)sensitivity);
        };

        std::sort(titans.begin(), titans.end(), [&](const auto& a, const auto& b) {
            return get_score(a.first, a.second) > get_score(b.first, b.second);
        });
        
        if ((int)titans.size() > count) titans.resize(count);
        return titans;
    }

    void snapshot(std::vector<std::pair<double,double>>& outBids,
                  std::vector<std::pair<double,double>>& outAsks) {
        int idx = active_read_.load(std::memory_order_acquire);
        const auto& snap = snapshots_[idx];
        outBids = snap.bids;
        outAsks = snap.asks;
    }

    uint64_t read_sequence() const {
        int idx = active_read_.load(std::memory_order_acquire);
        return snapshots_[idx].sequence;
    }
};

// --- GLOBAL TELEMETRY SPINE ---
namespace sovereign {
namespace telemetry {
    inline std::atomic<uint64_t> g_internal_latency_sum_us{0};
    inline std::atomic<uint64_t> g_internal_latency_count{0};
    inline std::atomic<double> g_btcusdt_basis{0.01};
}
}

namespace sovereign {
namespace network {

constexpr uint64_t fnv1a_hash_internal(const char* str, size_t len) {
    uint64_t hash = 14695981039346656037ull;
    for (size_t i = 0; i < len; ++i) {
        hash ^= static_cast<uint64_t>(str[i]);
        hash *= 1099511628211ull;
    }
    return hash;
}

static inline uint16_t route_instrument_fast(const char* sym, size_t len) {
    uint64_t h = fnv1a_hash_internal(sym, len);
    if (h == fnv1a_hash_internal("btcusdt", 7)) return 0;
    if (h == fnv1a_hash_internal("ethusdt", 7)) return 1;
    if (h == fnv1a_hash_internal("solusdt", 7)) return 2;
    if (h == fnv1a_hash_internal("bnbusdt", 7)) return 3;
    if (h == fnv1a_hash_internal("xrpusdt", 7)) return 4;
    if (h == fnv1a_hash_internal("adausdt", 7)) return 5;
    if (h == fnv1a_hash_internal("dogeusdt", 8)) return 6;
    if (h == fnv1a_hash_internal("avaxusdt", 8)) return 7;
    if (h == fnv1a_hash_internal("dotusdt", 7)) return 8;
    if (h == fnv1a_hash_internal("maticusdt", 9)) return 9;
    if (h == fnv1a_hash_internal("polusdt", 7)) return 9;
    if (h == fnv1a_hash_internal("linkusdt", 8)) return 10;
    if (h == fnv1a_hash_internal("trxusdt", 7)) return 11;
    if (h == fnv1a_hash_internal("shibusdt", 8)) return 12;
    return 999;
}

} // namespace network
} // namespace sovereign
