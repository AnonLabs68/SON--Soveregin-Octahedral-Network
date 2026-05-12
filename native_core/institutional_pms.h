#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <chrono>
#include <iomanip>
#include <csignal>
#include <algorithm>
#include <cmath>

#ifdef _WIN32
#include <windows.h>
#endif

// =============================================================================
// [ SOVEREIGN SENTINEL : THE BLACK BOX FLIGHT RECORDER ]
// =============================================================================
struct SentinelBreadcrumb {
    std::atomic<const char*> context{"BOOTING"};
    std::atomic<uint64_t> last_tick{0};
    std::atomic<bool> alive{true};
};

class SovereignSentinel {
public:
    static SovereignSentinel& instance() {
        static SovereignSentinel inst;
        return inst;
    }

    static thread_local SentinelBreadcrumb* t_breadcrumb;

    void register_thread(const std::string& name) {
        std::lock_guard<std::mutex> lock(m_registry_mutex);
        auto bc = new SentinelBreadcrumb();
        m_threads[name] = bc;
        t_breadcrumb = bc;
        trace("INIT_COMPLETE");
    }

    void trace(const char* msg) {
        if (t_breadcrumb) {
            t_breadcrumb->context.store(msg, std::memory_order_relaxed);
#ifdef _WIN32
            t_breadcrumb->last_tick.store(GetTickCount64(), std::memory_order_relaxed);
#endif
        }
    }

    void shutdown_cleanly() {
        static std::atomic<bool> already_reported{false};
        if (already_reported.exchange(true)) return;
        std::cout << "\n" << std::string(80, '=') << "\n [ SOVEREIGN SENTINEL : POST-MORTEM INTEGRITY REPORT ]\n" << std::string(80, '-') << "\n";
        std::lock_guard<std::mutex> lock(m_registry_mutex);
        for (auto const& [name, bc] : m_threads) {
            std::cout << " Thread    : [" << name << "] Context: " << bc->context.load() << "\n";
        }
        std::cout << std::string(80, '=') << "\n" << std::endl;
    }

    static void handle_signal(int sig) {
        SovereignSentinel::instance().shutdown_cleanly();
    }

private:
    SovereignSentinel() {
        std::signal(SIGSEGV, handle_signal);
        std::signal(SIGABRT, handle_signal);
        std::atexit([]() { SovereignSentinel::instance().shutdown_cleanly(); });
    }
    std::mutex m_registry_mutex;
    std::unordered_map<std::string, SentinelBreadcrumb*> m_threads;
};

#define SENTINEL_TRACE(msg) SovereignSentinel::instance().trace(msg)
#define SENTINEL_THREAD(name) SovereignSentinel::instance().register_thread(name)

// =============================================================================
// [ INSTITUTIONAL RISK & PMS CORE ]
// =============================================================================
namespace InstitutionalRisk {
    enum class RiskType { TAKE_PROFIT, STOP_LOSS, TRAILING_STOP };
    enum class TriggerRef { LAST_PRICE, MARK_PRICE, INDEX_PRICE };
    enum class RiskState { ARMED, TRIGGER_CONDITION_MET, PREEXECUTION_VALIDATION, EXECUTING, EXECUTED, CANCELLED, FAILED, EXPIRED, DETACHED };
    enum class AccountingMethod { FIFO, LIFO, VWAP };
    enum class AssetClass { Equity, Future, Option, FX, Crypto, Bond };

    struct RiskOrder {
        uint64_t id;
        uint64_t parent_pos_version = 0;
        RiskType type;
        double trigger_price;
        double limit_price;
        double qty_raw;
        bool is_percent;
        TriggerRef ref = TriggerRef::LAST_PRICE;
        RiskState state = RiskState::ARMED;
        double trail_dist = 0.0;
        double peak_price = 0.0;
        uint64_t created_at;
        uint64_t triggered_at = 0;
        uint64_t executed_at = 0;
    };

    struct PositionKey {
        std::string portfolio; std::string strategy; std::string account; std::string symbol;
        bool operator==(const PositionKey& other) const {
            return portfolio == other.portfolio && strategy == other.strategy && account == other.account && symbol == other.symbol;
        }
    };

    struct PositionKeyHash {
        std::size_t operator()(const PositionKey& k) const {
            return std::hash<std::string>()(k.portfolio) ^ (std::hash<std::string>()(k.strategy) << 1) ^ (std::hash<std::string>()(k.account) << 2) ^ (std::hash<std::string>()(k.symbol) << 3);
        }
    };

    struct Position {
        PositionKey key;
        double net_qty = 0.0;
        double avg_entry_price = 0.0;
        double mark_price = 0.0;
        double liquidation_price = 0.0;
        double realized_pnl = 0.0;
        double unrealized_pnl = 0.0;
        std::vector<RiskOrder> risk_levels;
        uint64_t version = 0;
        double multiplier = 1.0;
    };

    struct OpenPosition {
        int id;
        std::string symbol;
        bool is_long;
        double quantity;
        double entry_price;
        double mark_price;
        double liquidation_price;
        double unrealized_pnl;
        double unrealized_pnl_pct;
        uint64_t open_timestamp;
        double tp_price = 0.0;
        double sl_price = 0.0;
        double tp_edit_price = 0.0;
        double sl_edit_price = 0.0;
        bool is_editing_tp = false;
        bool is_editing_sl = false;
    };

    class PortfolioManager {
    public:
        static PortfolioManager& instance() { static PortfolioManager inst; return inst; }

        void update_mark_price(const std::string& symbol, double price) {
            std::lock_guard<std::mutex> lock(m_mutex);
            for (auto& [key, pos] : m_positions) {
                if (key.symbol == symbol) {
                    double old_price = pos.mark_price;
                    pos.mark_price = price;
                    refresh_pnl_locked(pos);
                    if (pos.net_qty == 0) continue;
                    for (auto& risk : pos.risk_levels) {
                        if (risk.state == RiskState::ARMED) {
                            if (check_trigger(pos, risk, price, old_price)) execute_risk_locked(pos, risk, price);
                        }
                    }
                    refresh_pnl_locked(pos);
                }
            }
        }

        void process_fill(const std::string& symbol, bool is_buy, double qty, double price, const std::string& strategy = "Default") {
            if (qty <= 0.0 || price <= 0.0) return;
            std::lock_guard<std::mutex> lock(m_mutex);
            PositionKey key = {"Main", strategy, "Primary", symbol};
            Position& pos = m_positions[key];
            if (pos.key.symbol.empty()) pos.key = key;
            apply_fill_locked(pos, is_buy, qty, price);
        }

        void add_risk_level(const std::string& symbol, RiskType type, double trigger, double qty, bool is_pct, const std::string& strategy = "Default") {
            if (qty <= 0.0) return;
            std::lock_guard<std::mutex> lock(m_mutex);
            PositionKey key = {"Main", strategy, "Primary", symbol};
            if (m_positions.count(key)) {
                Position& pos = m_positions[key];
                if (pos.net_qty == 0.0) return;
                RiskOrder r{};
                r.id = now_ms() ^ (uint64_t)(pos.risk_levels.size() + 1);
                r.parent_pos_version = pos.version;
                r.type = type;
                r.trigger_price = trigger;
                r.limit_price = trigger;
                r.qty_raw = qty;
                r.is_percent = is_pct;
                r.state = RiskState::ARMED;
                r.created_at = now_ms();
                if (type == RiskType::TRAILING_STOP) {
                    r.trail_dist = std::max(0.0, trigger);
                    r.peak_price = pos.mark_price > 0.0 ? pos.mark_price : pos.avg_entry_price;
                    r.trigger_price = pos.net_qty > 0 ? r.peak_price - r.trail_dist : r.peak_price + r.trail_dist;
                }
                pos.risk_levels.push_back(r);
            }
        }

        std::unordered_map<PositionKey, Position, PositionKeyHash> get_all_positions() { std::lock_guard<std::mutex> lock(m_mutex); return m_positions; }
        bool get_position_copy(const std::string& symbol, Position& out_pos, const std::string& strategy = "Default") {
            std::lock_guard<std::mutex> lock(m_mutex);
            PositionKey key = {"Main", strategy, "Primary", symbol};
            if (m_positions.count(key)) { out_pos = m_positions[key]; return true; }
            return false;
        }
        void close_position(const std::string& symbol, const std::string& strategy = "Default") {
            std::lock_guard<std::mutex> lock(m_mutex);
            PositionKey key = {"Main", strategy, "Primary", symbol};
            if (m_positions.count(key)) {
                Position& pos = m_positions[key];
                double qty = std::abs(pos.net_qty);
                double px = pos.mark_price > 0.0 ? pos.mark_price : pos.avg_entry_price;
                if (qty > 0.0 && px > 0.0) {
                    apply_fill_locked(pos, pos.net_qty < 0.0, qty, px);
                }
                pos.net_qty = 0.0;
                cancel_armed_risks_locked(pos);
                refresh_pnl_locked(pos);
            }
        }

    private:
        PortfolioManager() {}
        static uint64_t now_ms() {
#ifdef _WIN32
            return static_cast<uint64_t>(GetTickCount64());
#else
            return (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
#endif
        }
        static bool same_side(double a, double b) {
            return (a >= 0.0 && b >= 0.0) || (a <= 0.0 && b <= 0.0);
        }
        void refresh_pnl_locked(Position& pos) {
            if (std::abs(pos.net_qty) < 1e-12 || pos.avg_entry_price <= 0.0 || pos.mark_price <= 0.0) {
                pos.unrealized_pnl = 0.0;
                if (std::abs(pos.net_qty) < 1e-12) {
                    pos.net_qty = 0.0;
                    pos.avg_entry_price = 0.0;
                }
                return;
            }
            pos.unrealized_pnl = (pos.mark_price - pos.avg_entry_price) * pos.net_qty * pos.multiplier;
        }
        void cancel_armed_risks_locked(Position& pos) {
            for (auto& r : pos.risk_levels) {
                if (r.state == RiskState::ARMED || r.state == RiskState::TRIGGER_CONDITION_MET || r.state == RiskState::PREEXECUTION_VALIDATION) {
                    r.state = RiskState::CANCELLED;
                }
            }
        }
        void apply_fill_locked(Position& pos, bool is_buy, double qty, double price) {
            const double signed_qty = is_buy ? qty : -qty;
            const double old_qty = pos.net_qty;
            pos.version++;
            pos.mark_price = price;

            if (std::abs(old_qty) < 1e-12) {
                pos.net_qty = signed_qty;
                pos.avg_entry_price = price;
                refresh_pnl_locked(pos);
                return;
            }

            if (same_side(old_qty, signed_qty)) {
                const double total_abs = std::abs(old_qty) + std::abs(signed_qty);
                pos.avg_entry_price = total_abs > 0.0
                    ? ((pos.avg_entry_price * std::abs(old_qty)) + (price * std::abs(signed_qty))) / total_abs
                    : price;
                pos.net_qty = old_qty + signed_qty;
            } else {
                const double closing_qty = std::min(std::abs(old_qty), std::abs(signed_qty));
                const double pnl_per_unit = old_qty > 0.0 ? (price - pos.avg_entry_price) : (pos.avg_entry_price - price);
                pos.realized_pnl += pnl_per_unit * closing_qty * pos.multiplier;

                const double remaining = std::abs(signed_qty) - closing_qty;
                if (remaining > 1e-12) {
                    pos.net_qty = is_buy ? remaining : -remaining;
                    pos.avg_entry_price = price;
                } else {
                    pos.net_qty = old_qty + signed_qty;
                    if (std::abs(pos.net_qty) < 1e-12) {
                        pos.net_qty = 0.0;
                        pos.avg_entry_price = 0.0;
                        cancel_armed_risks_locked(pos);
                    }
                }
            }
            refresh_pnl_locked(pos);
        }
        bool check_trigger(const Position& pos, RiskOrder& r, double p, double old_p) {
            bool is_long = pos.net_qty > 0;
            if (r.type == RiskType::TAKE_PROFIT) return is_long ? p >= r.trigger_price : p <= r.trigger_price;
            if (r.type == RiskType::STOP_LOSS) return is_long ? p <= r.trigger_price : p >= r.trigger_price;
            if (r.type == RiskType::TRAILING_STOP) {
                if (r.trail_dist <= 0.0) return false;
                if (r.peak_price <= 0.0) r.peak_price = old_p > 0.0 ? old_p : p;
                if (is_long) {
                    if (p > r.peak_price) {
                        r.peak_price = p;
                        r.trigger_price = r.peak_price - r.trail_dist;
                    }
                    return p <= r.trigger_price;
                }
                if (p < r.peak_price) {
                    r.peak_price = p;
                    r.trigger_price = r.peak_price + r.trail_dist;
                }
                return p >= r.trigger_price;
            }
            return false;
        }
        void execute_risk_locked(Position& pos, RiskOrder& r, double p) {
            if (pos.net_qty == 0.0) {
                r.state = RiskState::CANCELLED;
                return;
            }
            r.state = RiskState::TRIGGER_CONDITION_MET;
            r.triggered_at = now_ms();
            r.state = RiskState::PREEXECUTION_VALIDATION;
            const double abs_pos = std::abs(pos.net_qty);
            double close_qty = r.is_percent
                ? abs_pos * std::clamp(r.qty_raw / 100.0, 0.0, 1.0)
                : std::min(abs_pos, std::max(0.0, r.qty_raw));
            if (close_qty <= 1e-12 || p <= 0.0) {
                r.state = RiskState::FAILED;
                return;
            }
            r.state = RiskState::EXECUTING;
            apply_fill_locked(pos, pos.net_qty < 0.0, close_qty, p);
            r.state = RiskState::EXECUTED;
            r.executed_at = now_ms();
            r.parent_pos_version = pos.version;
        }
        std::unordered_map<PositionKey, Position, PositionKeyHash> m_positions;
        std::mutex m_mutex;
    };
}
