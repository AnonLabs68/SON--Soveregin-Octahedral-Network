#pragma once
#include <string>
#include <thread>
#include <atomic>
#include "SharedStructures.hpp"

namespace sovereign {
namespace network {

class IngestionService {
public:
    IngestionService(LockFreeQueue<MarketUpdate>& queue);
    ~IngestionService();

    bool start(const std::string& config_path);
    void stop();

private:
    void worker_thread_main(const std::string& config_path);
    void process_message(const std::string& msg);

    std::atomic<bool> running_;
    std::thread worker_;
    LockFreeQueue<MarketUpdate>& market_queue_;
};

} // namespace network
} // namespace sovereign