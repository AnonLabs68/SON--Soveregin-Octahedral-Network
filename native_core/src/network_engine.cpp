#include "sovereign/network_engine.hpp"
#include "sovereign/sovereign_types.hpp"
#include <iostream>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

namespace sovereign {

std::unordered_set<std::string> g_active_peers;
std::mutex g_son_peers_mutex;

// Forward declaration of internal helpers if needed
static int hex_nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

static bool parse_hex_bytes(const std::string& hex, unsigned char* out, size_t out_len) {
    if (hex.size() != out_len * 2) return false;
    for (size_t i = 0; i < out_len; ++i) {
        int hi = hex_nibble(hex[i * 2]);
        int lo = hex_nibble(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0) return false;
        out[i] = (unsigned char)((hi << 4) | lo);
    }
    return true;
}

bool blast_tpu_udp(const std::string& ip, int port, const std::string& payload_hex) {
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == (SOCKET)-1) return false;

    sockaddr_in dest{};
    dest.sin_family = AF_INET;
    dest.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &dest.sin_addr);

    std::vector<unsigned char> bytes(payload_hex.size() / 2);
    if (!parse_hex_bytes(payload_hex, bytes.data(), bytes.size())) {
#ifdef _WIN32
        closesocket(sock);
#else
        close(sock);
#endif
        return false;
    }

    int sent = sendto(sock, (const char*)bytes.data(), (int)bytes.size(), 0, (sockaddr*)&dest, sizeof(dest));
#ifdef _WIN32
    closesocket(sock);
#else
    close(sock);
#endif
    return sent > 0;
}

// NOTE: Workers (son_network_worker, execution_dispatch_worker) are coming in next block
// as they require more shared state from main.cpp which I will extract shortly.

} // namespace sovereign
