#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <thread>
#include <chrono>
#include <mutex>
#include "SovereignMarketLink.hpp"
#include "hexafield_protocol.h"
#include "simdjson.h"

#pragma comment(lib, "ws2_32.lib")

using namespace sovereign;

SOCKET udp_sock;
sockaddr_in engine_addr;

constexpr uint64_t fnv1a_hash(const char* str, size_t len) {
    uint64_t hash = 14695981039346656037ull;
    for (size_t i = 0; i < len; ++i) {
        hash ^= static_cast<uint64_t>(str[i]);
        hash *= 1099511628211ull;
    }
    return hash;
}

static inline uint16_t route_instrument_fast(const char* sym, size_t len) {
    uint64_t h = fnv1a_hash(sym, len);
    if (h == fnv1a_hash("btcusdt", 7)) return 0;
    if (h == fnv1a_hash("ethusdt", 7)) return 1;
    if (h == fnv1a_hash("solusdt", 7)) return 2;
    if (h == fnv1a_hash("bnbusdt", 7)) return 3;
    if (h == fnv1a_hash("xrpusdt", 7)) return 4;
    if (h == fnv1a_hash("adausdt", 7)) return 5;
    if (h == fnv1a_hash("dogeusdt", 8)) return 6;
    if (h == fnv1a_hash("avaxusdt", 8)) return 7;
    if (h == fnv1a_hash("dotusdt", 7)) return 8;
    if (h == fnv1a_hash("polusdt", 7)) return 9;
    if (h == fnv1a_hash("maticusdt", 9)) return 9;
    if (h == fnv1a_hash("linkusdt", 8)) return 10;
    if (h == fnv1a_hash("trxusdt", 7)) return 11;
    if (h == fnv1a_hash("shibusdt", 8)) return 12;
    return 999;
}

void OnMarketData(const std::string& msg, int venue_id) {
    static simdjson::ondemand::parser parser;
    try {
        simdjson::ondemand::document doc = parser.iterate(msg);
        std::string_view event_type = doc["e"];
        
        protocol::MarketFieldPacket pkt{};
        pkt.header.version_major = 1;
        pkt.header.packet_type = protocol::PacketType::MARKET_DATA_UPDATE;
        
        std::string_view sym_sv = doc["s"];
        std::string sym_lower(sym_sv.length(), ' ');
        for(size_t i=0; i<sym_sv.length(); ++i) sym_lower[i] = (char)tolower(sym_sv[i]);
        
        uint16_t id = route_instrument_fast(sym_lower.c_str(), sym_lower.length());
        if (id >= 512) return;
        pkt.body.instrument_id = id;
        pkt.body.ts_ms = doc["E"];

        if (event_type == "trade") {
            pkt.body.price = atof(std::string(doc["p"].get_string().value()).c_str());
            pkt.body.volume = atof(std::string(doc["q"].get_string().value()).c_str());
            pkt.body.side = doc["m"].get_bool().value() ? 0 : 1;
            pkt.body.update_type = 1;
        } else if (event_type == "depthUpdate") {
            pkt.body.update_type = 0;
            simdjson::ondemand::array bids = doc["b"];
            for (auto bid : bids) {
                pkt.body.price = atof(std::string(bid.at(0).get_string().value()).c_str());
                pkt.body.side = 1;
                break; 
            }
        } else return;

        // --- BATCHER ---
        static protocol::MarketFieldPacket batch_buf[16];
        static int batch_ptr = 0;
        static std::mutex batch_mtx;
        static auto last_flush = std::chrono::steady_clock::now();

        std::lock_guard<std::mutex> lock(batch_mtx);
        batch_buf[batch_ptr++] = pkt;
        auto now = std::chrono::steady_clock::now();
        if (batch_ptr >= 16 || std::chrono::duration_cast<std::chrono::milliseconds>(now - last_flush).count() > 10) {
            sendto(udp_sock, (const char*)batch_buf, sizeof(protocol::MarketFieldPacket) * batch_ptr, 0, (sockaddr*)&engine_addr, sizeof(engine_addr));
            batch_ptr = 0;
            last_flush = now;
        }
    } catch (...) {}
}

int main() {
    std::cout << "--- SOVEREIGN SIMD-GATEKEEPER STARTING ---" << std::endl;
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    udp_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    engine_addr.sin_family = AF_INET;
    engine_addr.sin_port = htons(28001);
    InetPtonA(AF_INET, "127.0.0.1", &engine_addr.sin_addr);

    sovereign::network::MarketLink binance_link;
    std::string streams = "/ws/btcusdt@trade/btcusdt@depth/ethusdt@trade/ethusdt@depth/solusdt@trade/solusdt@depth"
                          "/bnbusdt@trade/bnbusdt@depth/xrpusdt@trade/xrpusdt@depth/adausdt@trade/adausdt@depth"
                          "/dogeusdt@trade/dogeusdt@depth/avaxusdt@trade/avaxusdt@depth/dotusdt@trade/dotusdt@depth"
                          "/polusdt@trade/polusdt@depth/linkusdt@trade/linkusdt@depth/trxusdt@trade/trxusdt@depth/shibusdt@trade/shibusdt@depth";

    while (true) {
        if (!binance_link.is_active()) {
            std::cout << "[Gatekeeper] Connecting to Global Mesh (SIMD Optimized)..." << std::endl;
            binance_link.start(streams, OnMarketData);
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    closesocket(udp_sock);
    WSACleanup();
    return 0;
}
