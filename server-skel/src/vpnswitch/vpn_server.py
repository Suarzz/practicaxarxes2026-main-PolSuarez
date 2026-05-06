import socket
import threading
import time
from enum import Enum

from loguru import logger

from dataclasses import dataclass, field


class SessionState(str, Enum):
    none = "none"
    registering = "registering"
    authenticated = "authenticated"
@dataclass
class VpnSession:
    ip: str
    port: int
    state: SessionState = SessionState.none
    last_seen: float = field(default_factory=time.time)
    pktsIn: int = 0
    pktsOut: int = 0
    bytesIn: int = 0
    bytesOut: int = 0
class VpnServer:


    def __init__(self, config):
        self.config = config

        # Create an IPv4 UDP socket
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

        # Bind to all interfaces (0.0.0.0) on the user-provided port
        self.sock.bind(("0.0.0.0", self.config.port))

        self.clients = {}
        self.mac_table = {}
        self.mac_lock = threading.Lock()

        self.broadcasts = 0
        self.unicasts = 0
        self.unknown_mac = 0
        self.discarded = 0

        threading.Thread(target=self.cleanup_sessions, daemon=True).start()
        threading.Thread(target=self.print_stats, daemon=True).start()

    def run(self):
        logger.info("Server is now running and waiting for packets...")
        MAX_BUFFER_SIZE = 65535


        while True:
            # Wait until a packet arrives
            data, (client_ip, client_port) = self.sock.recvfrom(MAX_BUFFER_SIZE)

            logger.debug(f"Received {len(data)} bytes from {client_ip}:{client_port}")

            #opcode in first byte
            opcode = data[0]

            #CLient id in big endian
            client_id = (data[1] << 8) | data[2]

            #Payload (8 bytes from index 4 up to, but not including, 11)
            payload = data[3:11]


            if opcode == 1:
                #REGISTERING
                if client_id in self.clients:
                    logger.info(f"Client {client_id} roamed from {self.clients[client_id].port} to {client_port}")
                    del self.clients[client_id] #replace session

                self.clients[client_id] = VpnSession(client_ip, client_port)
                self.clients[client_id].state = SessionState.registering
                self.send_ack(client_ip, client_port, data[1], data[2])
                logger.info(f"Client {client_id} successfully registering.")
            elif opcode == 2:
                #AUTH
                if len(payload) != 8:
                    self.send_reject(client_ip, client_port, data[1], data[2])
                    continue
                if client_id not in self.clients or self.clients[client_id].state != SessionState.registering :
                    self.send_reject(client_ip, client_port, data[1], data[2])
                    continue

                self.clients[client_id].state = SessionState.authenticated
                self.send_ack(client_ip, client_port, data[1], data[2])
                logger.info(f"Client {client_id} successfully authenticated.")

            elif opcode == 3:
                #MAC addresses
                destination_mac_addrr = data[11:17]
                source_mac_addrr = data[17:23]
                threading.Thread(target=self.learn_mac, args=(source_mac_addrr, client_id)).start()

                if destination_mac_addrr == b'\xff\xff\xff\xff\xff\xff':
                    self.broadcasts += 1
                    self.broadcast_packet(client_id, data)
                else:
                    if destination_mac_addrr in self.mac_table:
                        destination_client_id = self.mac_table[destination_mac_addrr]
                        self.send_traffic(data, destination_client_id)
                        self.unicasts += 1
                    else:
                        self.unknown_mac += 1
                        if self.config.unknown_mac == "discard":
                            logger.debug("Discarding unknown MAC")
                            self.discarded += 1
                        elif self.config.unknown_mac == "flood":
                            self.broadcast_packet(client_id, data)

                #Update last_seen
                self.clients[client_id].last_seen = time.time()
                self.clients[client_id].pktsIn += 1
                self.clients[client_id].bytesIn += len(data)
            elif opcode == 4:
                #Update last_seen
                if client_id in self.clients and self.clients[client_id].state == SessionState.authenticated:
                    self.clients[client_id].last_seen = time.time()


    def send_reject(self, client_ip, client_port, id_high, id_low):
        reject_opcode = 0x06

        # Build header
        response_header = bytes([reject_opcode, id_high, id_low])
        response_packet = response_header + (b'\x00' * 8)

        # Send it back to the client
        self.sock.sendto(response_packet, (client_ip, client_port))

    def send_ack(self, client_ip, client_port, id_high, id_low):
        ack_opcode = 0x05

        # Build header
        response_header = bytes([ack_opcode, id_high, id_low])
        response_packet = response_header + (b'\x00' * 8)

        # Send it back to the client
        self.sock.sendto(response_packet, (client_ip, client_port))

    def send_traffic(self, frame, destination_client_id):
        # 1. Make a mutable copy of the frame
        mutable_frame = bytearray(frame)

        # 2. Overwrite index 1 with the High Byte of the destination ID
        mutable_frame[1] = (destination_client_id >> 8) & 0xFF

        # 3. Overwrite index 2 with the Low Byte of the destination ID
        mutable_frame[2] = destination_client_id & 0xFF

        #Forward to destination client
        self.sock.sendto(mutable_frame, (self.clients[destination_client_id].ip, self.clients[destination_client_id].port))
        self.clients[destination_client_id].pktsOut += 1
        self.clients[destination_client_id].bytesOut += len(mutable_frame)

    def broadcast_packet(self, client_id, data):
        for iter_client_id, session in self.clients.items():
            if iter_client_id != client_id and session.state == SessionState.authenticated:
                self.send_traffic(data, iter_client_id)

    def learn_mac(self, source_mac, client_id):
        # 1. Wait in line and grab the lock
        with self.mac_lock:
            # 2. Safely update the shared dictionary
            self.mac_table[source_mac] = client_id

        # 3. The lock is automatically released when you un-indent!
        logger.debug(f"Learned MAC {source_mac.hex()} belongs to client {client_id}")

    def cleanup_sessions(self):
        timeout_seconds = 30

        while True:
            time.sleep(10) # The thread sleeps, then wakes up every 10 seconds
            current_time = time.time()

            # Loop through a snapshot of the keys so we don't crash while deleting
            for client_id in list(self.clients.keys()):
                session = self.clients[client_id]

                # If the difference between now and their last message is too big...
                if session.state == SessionState.authenticated and current_time - session.last_seen > timeout_seconds:
                    logger.info(f"Client {client_id} timed out. Deleting session.")
                    del self.clients[client_id]

    def print_stats(self):
        while True:
            time.sleep(self.config.stats_interval)

            # Format the current time exactly how you requested
            current_time = time.strftime("%Y-%m-%dT%H:%M:%S", time.localtime())

            print(f"\n=== Stats at {current_time} ===")
            # The header row
            print(f"{'ClientID':>8}  {'Addr':<22} {'PktsIn':>10} {'PktsOut':>10} {'BytesIn':>10} {'BytesOut':>10}")

            # Loop through a snapshot of active clients
            for client_id in list(self.clients.keys()):
                session = self.clients[client_id]

                # Combine IP and port into a single string for cleaner alignment
                addr_str = f"{session.ip}:{session.port}"

                # Print the data row
                print(f"{client_id:>8}  {addr_str:<22} {session.pktsIn:>10} {session.pktsOut:>10} {session.bytesIn:>10} {session.bytesOut:>10}")

            # Print the global stats
            print(f"Global — broadcasts: {self.broadcasts}  unicasts: {self.unicasts}  unknown_mac: {self.unknown_mac}  discarded: {self.discarded}")
            print("-" * 75) # A separator line for readability