#pragma once
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <iostream>
#include <vector>
#include "SharedStructures.hpp"
#include "simdjson.h"

#pragma comment(lib, "ws2_32.lib")

namespace sovereign {
namespace network {

// --- INSTITUTIONAL UDP BYPASS LINK ---
class MarketLink {
private:
    std::atomic<bool> active_{false};
    std::thread worker_;
    SOCKET sock_ = INVALID_SOCKET;

public:
    MarketLink() = default;
    ~MarketLink() { stop(); }

    bool is_active() const { return active_.load(); }

    void start(const std::string& streams, std::function<void(const std::string&, int)> callback) {
        if (active_.load()) return;
        active_.store(true);
        
        worker_ = std::thread([this, callback]() {
            WSADATA wsa;
            if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return;
            
            sock_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if (sock_ == INVALID_SOCKET) return;

            // Enable port reuse
            BOOL optval = TRUE;
            setsockopt(sock_, SOL_SOCKET, SO_REUSEADDR, (char*)&optval, sizeof(optval));

            sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_port = htons(28001); // THE SOVEREIGN FIREHOSE PORT
            addr.sin_addr.s_addr = INADDR_ANY;
            
            if (bind(sock_, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
                std::cerr << "[UDP-Firehose] FATAL: Failed to bind to port 28001. Error: " << WSAGetLastError() << std::endl;
                return;
            }

            std::cout << "[UDP-Firehose] Ready for Inbound Data on Port 28001 (Universal)." << std::endl;

            std::vector<char> buffer(1024 * 64);
            while (active_.load()) {
                int received = recvfrom(sock_, buffer.data(), (int)buffer.size(), 0, nullptr, nullptr);
                if (received > 0) {
                    std::string raw(buffer.data(), received);
                    if (received > 10) std::cout << "[UDP-Pulse] Received: " << raw.substr(0, 40) << "..." << std::endl;
                    callback(raw, 0);
                } else if (received == SOCKET_ERROR) {
                    int err = WSAGetLastError();
                    if (err != WSAEWOULDBLOCK && err != WSAEINTR) break;
                }
            }
            
            closesocket(sock_);
            WSACleanup();
        });
    }

    void stop() {
        active_.store(false);
        if (sock_ != INVALID_SOCKET) {
            closesocket(sock_);
            sock_ = INVALID_SOCKET;
        }
        if (worker_.joinable()) worker_.detach();
    }
};

} // namespace network
} // namespace sovereign
