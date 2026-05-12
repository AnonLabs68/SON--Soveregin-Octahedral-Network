import socket
import argparse
from hexafield_protocol import HexaFieldPacket, HexaHeader, HexaBody, PacketType, AgentID

def ping_network(target_ip="127.0.0.1", target_port=28005):
    print(f"🌍 Broadcasting NODE_DISCOVERY to SON Node {target_ip}:{target_port}...")
    
    # Create an empty body, but flag it as a NODE_DISCOVERY packet
    header = HexaHeader(1, 0, PacketType.NODE_DISCOVERY, 0, 0, 0, AgentID.ROUTER, 0, 255, 0)
    body = HexaBody(0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0)
    pkt = HexaFieldPacket.create_sealed(header, body)
    
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    
    # Bind to an ephemeral port so we can stay alive and listen for gossip
    sock.bind(("0.0.0.0", 0))
    local_port = sock.getsockname()[1]
    print(f"📡 Local Node spawned on UDP port {local_port}")
    sock.settimeout(2.0)
    
    sock.sendto(pkt.pack(), (target_ip, target_port))
    try:
        resp, addr = sock.recvfrom(1024)
        if len(resp) == 48:
            resp_pkt = HexaFieldPacket.unpack(resp)
            if resp_pkt.header.packet_type == PacketType.NODE_DISCOVERY_ACK:
                print(f"✅ Handshake successful! Acknowledged by {addr[0]}:{addr[1]}")
                print("   -> We are now registered in their active_peers routing table.")
    except socket.timeout:
        print("❌ Discovery timeout. Node unreachable.")
        return
        
    print("\n🎧 Listening for Tri-Phase Gossip from the SON Network...")
    sock.settimeout(None)  # Wait infinitely for network gossip
    
    try:
        while True:
            data, addr = sock.recvfrom(1024)
            if len(data) == 48:
                gossip_pkt = HexaFieldPacket.unpack(data)
                if gossip_pkt.header.packet_type == PacketType.COLLAPSE_RESULT:
                    decision = "WAIT"
                    if gossip_pkt.body.belief_go_q15 > 16000: decision = "GO"
                    elif gossip_pkt.body.belief_avoid_q15 > 16000: decision = "AVOID"
                    print(f"⚡ [GOSSIP RECEIVED] from {addr[0]}:{addr[1]} -> Network State Collapsed: [{decision}]")
    except KeyboardInterrupt:
        print("\n🛑 Disconnecting from SON Network.")

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=28005)
    args = parser.parse_args()
    ping_network(args.host, args.port)
