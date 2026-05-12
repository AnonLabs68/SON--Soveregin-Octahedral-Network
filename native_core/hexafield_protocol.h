#pragma once
#include <cstdint>

// This header defines the C++ data structures for the HexaField v1 protocol.
// It is designed for high-performance, low-latency IPC between the Python
// runtime and the C++ native core.

// Use pragma pack to ensure structs have a specific memory layout without
// any compiler-inserted padding. This is critical for binary serialization.
#pragma pack(push, 1)

namespace sovereign {
namespace protocol {

// All multi-byte fields are Little Endian (LE) as per HEXA_FIELD_SPEC.md

/**
 * @brief The 16-byte header of a HexaField packet.
 */
struct HexaHeader {
    uint8_t version_major;      // = 1
    uint8_t version_minor;      // = 0
    uint8_t packet_type;
    uint8_t flags;
    uint32_t run_id;
    uint32_t seq_no;
    uint8_t agent_id;
    uint8_t route_code;
    uint8_t ttl_ticks;
    uint8_t header_crc8;        // XOR checksum of header bytes 0-14
};

/**
 * @brief The 32-byte body of a HexaField packet, containing agent state.
 */
struct HexaBody {
    uint16_t belief_go_q15;
    uint16_t belief_wait_q15;
    uint16_t belief_avoid_q15;
    uint16_t risk_q15;
    uint16_t proof_q15;
    uint16_t latency_q15;
    uint16_t phase_q15;
    uint16_t coherence_hint_q15;
    uint16_t uncertainty_x_q15;
    uint16_t uncertainty_p_q15;
    uint16_t confidence_q15;
    uint16_t reserved0;
    uint16_t reserved1;
    uint16_t reserved2;
    uint16_t reserved3;
    uint16_t body_crc16;         // XOR-folded checksum of body bytes
};

/**
 * @brief High-Frequency Market Data Body (48-byte total packet)
 */
struct MarketFieldBody {
    uint16_t instrument_id;      // 2 bytes
    uint8_t  update_type;        // 1 byte
    uint8_t  side;               // 1 byte
    double   price;              // 8 bytes
    double   volume;             // 8 bytes
    uint64_t ts_ms;              // 8 bytes
    uint16_t checksum;           // 2 bytes
    uint16_t padding;            // 2 bytes (Total: 32 bytes)
};

/**
 * @brief The complete 48-byte HexaField packet.
 */
struct HexaFieldPacket {
    HexaHeader header;
    HexaBody body;
};

/**
 * @brief Unified 48-byte Market Packet
 */
struct MarketFieldPacket {
    HexaHeader header;
    MarketFieldBody body;
};

// Static assertions are a world-class engineering practice. They guarantee
// at compile-time that our struct sizes are correct, preventing catastrophic
// serialization errors if compiler settings were to change.
static_assert(sizeof(HexaHeader) == 16, "HexaHeader must be 16 bytes");
static_assert(sizeof(HexaBody) == 32, "HexaBody must be 32 bytes");
static_assert(sizeof(MarketFieldBody) == 32, "MarketFieldBody must be 32 bytes");
static_assert(sizeof(HexaFieldPacket) == 48, "HexaFieldPacket must be 48 bytes");
static_assert(sizeof(MarketFieldPacket) == 48, "MarketFieldPacket must be 48 bytes");

namespace PacketType {
    constexpr uint8_t AGENT_FIELD_UPDATE = 0x10;
    constexpr uint8_t MARKET_DATA_UPDATE = 0x15;
    constexpr uint8_t COLLAPSE_REQUEST = 0x20;
    constexpr uint8_t COLLAPSE_RESULT = 0x21;
    constexpr uint8_t PROOF_ATTACH = 0x30;
    constexpr uint8_t HEALTH_TELEMETRY = 0x40;
    constexpr uint8_t NODE_DISCOVERY = 0x50;
    constexpr uint8_t NODE_DISCOVERY_ACK = 0x51;
    constexpr uint8_t LIGHTHOUSE_BEACON = 0x60;  // Long-lived relay node identity broadcast
    constexpr uint8_t ELECTRON_FREEZE = 0x70;  // Cold state anchor signal
}

namespace AgentID {
    constexpr uint8_t ROUTER = 0x01;
    constexpr uint8_t RETRIEVER = 0x02;
    constexpr uint8_t VERIFIER = 0x03;
    constexpr uint8_t NOWCAST = 0x04;
    constexpr uint8_t STRATEGIST = 0x05;
    constexpr uint8_t CRITIC = 0x06;
}

} // namespace protocol
} // namespace sovereign

// Restore default packing alignment
#pragma pack(pop)