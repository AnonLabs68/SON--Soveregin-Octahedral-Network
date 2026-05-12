#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>
#include "IngestionService.hpp"
#include "SharedStructures.hpp"
#include "simdjson.h"

#pragma comment(lib, "winhttp.lib")

// --- THE CENTRAL DATA SPINE LINK ---
extern LockFreeQueue<MarketUpdate> G_CENTRAL_SPINE_QUEUE;

namespace sovereign {
namespace network {

IngestionService::IngestionService(LockFreeQueue<MarketUpdate>& queue)
    : market_queue_(queue), running_(false) {}

IngestionService::~IngestionService() {
    stop();
}

bool IngestionService::start(const std::string& config_path) {
    if (running_.load()) return false;
    running_.store(true);
    worker_ = std::thread(&IngestionService::worker_thread_main, this, config_path);
    return true;
}

void IngestionService::stop() {
    running_.store(false);
    if (worker_.joinable()) worker_.join();
}

void IngestionService::worker_thread_main(const std::string& config_path) {
    std::cout << "[NativeMesh] Initializing High-Throughput Session..." << std::endl;
    
    while (running_.load()) {
        HINTERNET hSession = WinHttpOpen(L"SovereignHFT/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hSession) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            continue;
        }

        DWORD protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 | 0x00000800;
        WinHttpSetOption(hSession, WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols, sizeof(protocols));

        HINTERNET hConnect = WinHttpConnect(hSession, L"fstream.binance.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (!hConnect) {
            WinHttpCloseHandle(hSession);
            std::this_thread::sleep_for(std::chrono::seconds(5));
            continue;
        }

        std::wstring streams = L"/stream?streams=btcusdt@trade/btcusdt@depth@100ms/btcusdt@markPrice@1s/ethusdt@trade/ethusdt@depth@100ms/ethusdt@markPrice@1s/solusdt@trade/solusdt@depth@100ms/solusdt@markPrice@1s/bnbusdt@trade/bnbusdt@depth@100ms/adausdt@trade/adausdt@depth@100ms/xrpusdt@trade/xrpusdt@depth@100ms/dotusdt@trade/dotusdt@depth@100ms/dogeusdt@trade/dogeusdt@depth@100ms/maticusdt@trade/maticusdt@depth@100ms/linkusdt@trade/linkusdt@depth@100ms/uniusdt@trade/uniusdt@depth@100ms/ltcusdt@trade/ltcusdt@depth@100ms/bchusdt@trade/bchusdt@depth@100ms/trxusdt@trade/trxusdt@depth@100ms/shibusdt@trade/shibusdt@depth@100ms/avaxusdt@trade/avaxusdt@depth@100ms/atomusdt@trade/atomusdt@depth@100ms/nearusdt@trade/nearusdt@depth@100ms/aptusdt@trade/aptusdt@depth@100ms";

        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", streams.c_str(), NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
        
        DWORD ignoreFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_CN_INVALID | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;
        WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &ignoreFlags, sizeof(ignoreFlags));

        bool connected = false;
        if (WinHttpSetOption(hRequest, WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, NULL, 0)) {
            if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
                if (WinHttpReceiveResponse(hRequest, NULL)) {
                    HINTERNET hWebSocket = WinHttpWebSocketCompleteUpgrade(hRequest, NULL);
                    if (hWebSocket) {
                        connected = true;
                        WinHttpCloseHandle(hRequest);
                        std::cout << "[NativeMesh] Institutional Firehose Connected. Fragmentation Buffer Active." << std::endl;
                        
                        std::vector<char> accumulation_buffer;
                        std::vector<char> read_buffer(256 * 1024); // 256KB read chunks
                        
                        while (running_.load()) {
                            DWORD bytesRead = 0;
                            WINHTTP_WEB_SOCKET_BUFFER_TYPE bufferType;
                            DWORD dwError = WinHttpWebSocketReceive(hWebSocket, read_buffer.data(), (DWORD)read_buffer.size(), &bytesRead, &bufferType);
                            
                            if (dwError == ERROR_SUCCESS) {
                                if (bytesRead > 0) {
                                    auto start_ts = std::chrono::high_resolution_clock::now();
                                    accumulation_buffer.insert(accumulation_buffer.end(), read_buffer.data(), read_buffer.data() + bytesRead);
                                    
                                    if (bufferType == WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE || 
                                        bufferType == WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE) {
                                        process_message(std::string(accumulation_buffer.begin(), accumulation_buffer.end()));
                                        accumulation_buffer.clear();
                                    }
                                    auto end_ts = std::chrono::high_resolution_clock::now();
                                    uint64_t elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(end_ts - start_ts).count();
                                    sovereign::telemetry::g_internal_latency_sum_us.fetch_add(elapsed_us, std::memory_order_relaxed);
                                    sovereign::telemetry::g_internal_latency_count.fetch_add(1, std::memory_order_relaxed);
                                }
                            } else {
                                std::cerr << "[NativeMesh] WebSocket Error: " << dwError << ". Retrying in 5s..." << std::endl;
                                break;
                            }
                        }
                        WinHttpWebSocketClose(hWebSocket, WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS, NULL, 0);
                        WinHttpCloseHandle(hWebSocket);
                    }
                }
            }
        }
        
        if (!connected) {
            std::cerr << "[NativeMesh] Connection Failed. Retrying in 5s..." << std::endl;
            WinHttpCloseHandle(hRequest);
        }
        
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        
        if (running_.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }
}

void IngestionService::process_message(const std::string& msg) {
    extern std::atomic<uint64_t> G_PULSE_INGESTION;
    G_PULSE_INGESTION.fetch_add(1, std::memory_order_relaxed);

    thread_local simdjson::ondemand::parser parser; 
    try {
        std::string padded_msg = msg;
        padded_msg.append(simdjson::SIMDJSON_PADDING, '\0');
        simdjson::ondemand::document doc = parser.iterate(padded_msg.data(), msg.size(), padded_msg.size());
        
        simdjson::ondemand::object obj;
        auto doc_obj = doc.get_object().value();
        
        bool is_multi = false;
        for (auto field : doc_obj) {
            if (field.unescaped_key().value() == "data") {
                obj = field.value().get_object().value();
                is_multi = true;
                break;
            }
        }
        if (!is_multi) obj = doc_obj;

        std::string_view event_type;
        std::string_view sym_sv;
        uint64_t ts = 0;

        for (auto field : obj) {
            std::string_view key = field.unescaped_key().value();
            if (key == "e") event_type = field.value().get_string().value();
            else if (key == "s") sym_sv = field.value().get_string().value();
            else if (key == "E" || key == "T") ts = (uint64_t)field.value().get_int64().value();
        }

        if (sym_sv.empty()) return;

        char sym_lower[32];
        size_t len = (sym_sv.length() < 31) ? sym_sv.length() : 31;
        for(size_t i=0; i<len; ++i) {
            char c = sym_sv[i];
            sym_lower[i] = (c >= 'A' && c <= 'Z') ? (c + 32) : c;
        }
        sym_lower[len] = '\0';
        
        uint16_t id = sovereign::network::route_instrument_fast(sym_lower, len);
        if (id >= 512) return;

        if (event_type == "trade") {
            double p = std::stod(std::string(obj["p"].get_string().value()));
            double q = std::stod(std::string(obj["q"].get_string().value()));
            bool is_m = obj["m"].get_bool().value();
            G_CENTRAL_SPINE_QUEUE.push({1, is_m, id, p, q, ts});
        } else if (event_type == "markPriceUpdate") {
            double p_mark = std::stod(std::string(obj["p"].get_string().value()));
            double p_index = std::stod(std::string(obj["i"].get_string().value()));
            // We use a special type '2' to represent the Basis/Index sync in the Spine
            G_CENTRAL_SPINE_QUEUE.push({2, true, id, p_mark, p_index, ts});
        } else if (event_type == "depthUpdate" || event_type == "depth") {
            simdjson::ondemand::array bids_arr = obj["b"].get_array().value();
            bool first_b = true;
            int b_count = 0;
            for (auto bid : bids_arr) {
                simdjson::ondemand::array b = bid.get_array().value();
                double p = std::stod(std::string(b.at(0).get_string().value()));
                double q = std::stod(std::string(b.at(1).get_string().value()));
                // Type 3 = Depth with snapshot clear flag
                G_CENTRAL_SPINE_QUEUE.push({first_b ? 3 : 0, true, id, p, q, ts});
                first_b = false;
                if (++b_count >= 20) break;
            }
            simdjson::ondemand::array asks_arr = obj["a"].get_array().value();
            bool first_a = true;
            int a_count = 0;
            for (auto ask : asks_arr) {
                simdjson::ondemand::array a = ask.get_array().value();
                double p = std::stod(std::string(a.at(0).get_string().value()));
                double q = std::stod(std::string(a.at(1).get_string().value()));
                G_CENTRAL_SPINE_QUEUE.push({first_a ? 3 : 0, false, id, p, q, ts});
                first_a = false;
                if (++a_count >= 20) break;
            }
        }
    } catch (...) {}
}

} // namespace network
} // namespace sovereign