import socket
import time
import random
import argparse
from hexafield_protocol import HexaFieldPacket, HexaHeader, HexaBody, PacketType, AgentID

def _to_q15(v: float) -> int:
    return int(round(max(0.0, min(1.0, float(v))) * 32767))

# Persistent socket so the C++ node recognizes us as the SAME peer to track reputation!
CASTER_SOCKET = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
CASTER_SOCKET.bind(("0.0.0.0", 0))
CASTER_SOCKET.settimeout(2.0)

def cast_electron(target_ip: str = "127.0.0.1", target_port: int = 28005, 
                  run_id: int = None, decision: str = "GO", conf: float = 0.95):
    print("============================================================")
    print(f"🌌 SON NETWORK: CASTING [{decision}] ELECTRON TO {target_ip}:{target_port}")
    print("============================================================\n")
    
    run_id = run_id or random.randint(1000, 9999)
    
    header = HexaHeader(1, 0, PacketType.COLLAPSE_RESULT, 0, run_id, 0, AgentID.ROUTER, 0, 255, 0)
    body = HexaBody(
        belief_go_q15=_to_q15(conf if decision == "GO" else 0), 
        belief_wait_q15=_to_q15(conf if decision == "WAIT" else 0), 
        belief_avoid_q15=_to_q15(conf if decision == "AVOID" else 0),
        risk_q15=_to_q15(0.1), proof_q15=_to_q15(0.9), latency_q15=_to_q15(0.01), phase_q15=0,
        coherence_hint_q15=_to_q15(0.85), uncertainty_x_q15=_to_q15(0.9),
        uncertainty_p_q15=_to_q15(0.8), confidence_q15=_to_q15(0.92),
        reserved0=0, reserved1=0, reserved2=0, reserved3=0, body_crc16=0
    )
    
    # Seal the packet and ADD the Enclave Fingerprint so it passes the shield
    temp_pkt = HexaFieldPacket.create_sealed(header, body)
    
    # Python implementation of the FNV-1a fingerprint to fake a valid network gossip
    fnv_prime = 1099511628211
    h_val = 14695981039346656037
    raw_data = temp_pkt.pack()
    for i in range(15): h_val = (h_val ^ raw_data[i]) * fnv_prime
    for i in range(16, 38): h_val = (h_val ^ raw_data[i]) * fnv_prime
    h_val &= 0xFFFFFFFFFFFFFFFF
    
    body = body._replace(
        reserved0=h_val & 0xFFFF, reserved1=(h_val >> 16) & 0xFFFF,
        reserved2=(h_val >> 32) & 0xFFFF, reserved3=(h_val >> 48) & 0xFFFF
    )
    pkt = HexaFieldPacket.create_sealed(header, body)
    binary_electron = pkt.pack()
    
    print(f"[+] Electron Compiled: {len(binary_electron)} bytes of raw binary.")
    print(f"[+] Hex dump: {binary_electron.hex()[:32]}...\n")
    
    # 3. Blast the UDP Datagram into the network
    print(f"🚀 Firing Electron into the UDP Wormhole... ", end="", flush=True)
    CASTER_SOCKET.sendto(binary_electron, (target_ip, target_port))
    
    # 4. Await the Tri-Phase Sync (The Observer Echo)
    try:
        resp_data, _ = CASTER_SOCKET.recvfrom(1024)
        if len(resp_data) == 48:
            resp_pkt = HexaFieldPacket.unpack(resp_data)
            decision = "WAIT"
            if resp_pkt.body.belief_go_q15 > 16000: decision = "GO"
            print(f"[✔] Impact confirmed! Network collapsed state back to: {decision}")
    except socket.timeout:
            print("[🌊] Electron absorbed by the network! (Bounced to a peer in the Kinetic Ledger)")

def simulate_quantum_collision(target_ip: str = "127.0.0.1", target_port: int = 28005):
    shared_id = random.randint(10000, 99999)
    print(f"\n🧪 INITIATING QUANTUM COLLISION TEST (Electron ID: {shared_id})")
    print("1. Firing strong GO signal...")
    cast_electron(target_ip=target_ip, target_port=target_port, run_id=shared_id, decision="GO", conf=0.92)
    time.sleep(0.5)
    print("2. Firing weak WAIT signal on the same ID...")
    cast_electron(target_ip=target_ip, target_port=target_port, run_id=shared_id, decision="WAIT", conf=0.55)

def trigger_vacuum_supernova(target_ip: str = "127.0.0.1", target_port: int = 28005):
    shared_id = random.randint(50000, 99999)
    print(f"\n🌌 IGNITING VACUUM-BRAID SUPERNOVA (Electron ID: {shared_id})")
    print("Flooding Enclave with 6 identical consensus vectors to shatter the Octahedron limit...")
    for i in range(6):
        cast_electron(target_ip=target_ip, target_port=target_port, run_id=shared_id, decision="GO", conf=0.99)
        time.sleep(0.01) # 10ms micro-stutter to simulate 6 global peers hitting the node simultaneously
    print("============================================================")
    print("Check your C++ Terminal! The Supernova should have detonated!")
    print("============================================================")

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=28005)
    args = parser.parse_args()
    print("Welcome to the SON Orbital Caster.")
    while True:
        choice = input("\n[?] Press ENTER for normal cast, 'C' for Collision Test, or 'S' for SUPERNOVA IGNITION... ").strip().lower()
        if choice == 'c':
            simulate_quantum_collision(args.host, args.port)
        elif choice == 's':
            trigger_vacuum_supernova(args.host, args.port)
        else:
            cast_electron(args.host, args.port)
