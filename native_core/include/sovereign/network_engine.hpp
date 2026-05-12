#pragma once
#include "sovereign/sovereign_types.hpp"
#include <string>

namespace sovereign {

// --- Orbital Accelerator: UDP Network Protocol ---
void son_network_worker(int port);
void execution_dispatch_worker();
bool blast_tpu_udp(const std::string& ip, int port, const std::string& payload_hex);

// Global Network State
extern std::unordered_set<std::string> g_active_peers;
extern std::mutex g_son_peers_mutex;

} // namespace sovereign
