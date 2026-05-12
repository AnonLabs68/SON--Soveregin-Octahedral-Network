#pragma once
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <atomic>
#include <iostream>
#include <functional>
#include "hexafield_protocol.h"

#pragma comment(lib, "ws2_32.lib")

namespace sovereign::network {

class HexafieldUdpLink {
private:
    SOCKET sock = INVALID_SOCKET;
    std::atomic<bool> is_running{false};
    std::thread worker_thread;
    
    // UPDATED: Now supports batched raw buffers for high-density global mesh
    using BatchHandler = std::function<void(const char*, int)>;
    BatchHandler on_batch;

public:
    HexafieldUdpLink() {
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
    }

    ~HexafieldUdpLink() {
        stop();
        WSACleanup();
    }

    bool start(int port, BatchHandler handler) {
        on_batch = handler;
        
        sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (sock == INVALID_SOCKET) return false;

        // HIGH-PERFORMANCE SOCKET OPTIONS
        int recv_buf = 1024 * 1024 * 16; // 16MB Kernel Buffer
        setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (const char*)&recv_buf, sizeof(recv_buf));
        
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = INADDR_ANY;

        if (bind(sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
            closesocket(sock);
            return false;
        }

        is_running = true;
        worker_thread = std::thread(&HexafieldUdpLink::run_loop, this);
        return true;
    }

    void stop() {
        is_running = false;
        if (sock != INVALID_SOCKET) {
            closesocket(sock);
            sock = INVALID_SOCKET;
        }
        if (worker_thread.joinable()) worker_thread.join();
    }

private:
    void run_loop() {
        char buffer[65536]; // 64KB MTU buffer
        sockaddr_in from{};
        int from_len = sizeof(from);

        while (is_running) {
            int received = recvfrom(sock, buffer, sizeof(buffer), 0, (sockaddr*)&from, &from_len);
            
            if (received > 0) {
                if (on_batch) on_batch(buffer, received);
            } else if (received == SOCKET_ERROR) {
                if (is_running) {
                    int err = WSAGetLastError();
                    if (err != WSAEINTR && err != WSAECONNRESET)
                        std::cerr << "[HexafieldUDP] Recv Error: " << err << std::endl;
                }
                break;
            }
        }
    }
};

} // namespace sovereign::network
