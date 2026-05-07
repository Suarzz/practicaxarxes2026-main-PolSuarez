import socket
import threading
import time
from loguru import logger

from .session import VpnSession, SessionState
from .protocol import OpCode
from .credentials import is_valid_auth_payload

class VpnServer:
    def __init__(self, config):
        self.config = config

        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind(("0.0.0.0", self.config.port))

        self.clients = {}
        self.mac_table = {}
        self.mac_lock = threading.Lock()

        self.broadcasts = 0
        self.unicasts = 0
        self.unknown_mac = 0
        self.discarded = 0

        self._start_background_threads()

    def _start_background_threads(self):
        threading.Thread(target=self.cleanup_sessions, daemon=True).start()
        threading.Thread(target=self.print_stats, daemon=True).start()

    def run(self):
        logger.info(f"Server is now running on port {self.config.port} and waiting for packets...")
        MAX_BUFFER_SIZE = 65535 + 11


        while True:
            # Wait until a packet arrives
            data, (client_ip, client_port) = self.sock.recvfrom(MAX_BUFFER_SIZE)
            logger.debug(f"Received {len(data)} bytes from {client_ip}:{client_port}")
            self._handle_packet(data, client_ip, client_port)

    def _handle_packet(self, data: bytes, client_ip: str, client_port: int):

        if len(data) < 11:
            logger.debug("Received malformed packet (too short).")
            return

        opcode = data[0] #opcode in first byte
        client_id = (data[1] << 8) | data[2]  #CLient id in big endian

        if opcode == OpCode.REGISTER:
            self._handle_register(client_id, client_ip, client_port, data)
        elif opcode == OpCode.AUTH:
            self._handle_auth(client_id, client_ip, client_port, data)
        elif opcode == OpCode.TRAFFIC:
            self._handle_traffic(client_id, data)
        elif opcode == OpCode.KEEPALIVE:
            self._handle_keepalive(client_id)
        else:
            logger.debug(f"Unknown opcode received: {opcode}")


    def _handle_register(self, client_id, client_ip, client_port, data):
        if client_id in self.clients:
            logger.info(f"Client {client_id} roamed from {self.clients[client_id].port} to {client_port}")
            del self.clients[client_id] #replace session

        self.clients[client_id] = VpnSession(client_ip, client_port)
        self.clients[client_id].state = SessionState.registering
        self.send_ack(client_ip, client_port, data[1], data[2])
        logger.info(f"Client {client_id} successfully registering.")


    def _handle_auth(self, client_id, client_ip, client_port, data):
        payload = data[3:11]
        if not is_valid_auth_payload(payload):
            self.send_reject(client_ip, client_port, data[1], data[2])
            logger.warning(f"Invalid auth payload from {client_ip}:{client_port}")
            return
        if client_id not in self.clients or self.clients[client_id].state != SessionState.registering :
            self.send_reject(client_ip, client_port, data[1], data[2])
            logger.warning(f"Auth attempted for invalid or non-registering client {client_id}")
            return

        self.clients[client_id].state = SessionState.authenticated
        self.send_ack(client_ip, client_port, data[1], data[2])
        logger.info(f"Client {client_id} successfully authenticated.")

    def _handle_traffic(self, client_id, data):
        # Only process traffic for authenticated clients
        if client_id not in self.clients or self.clients[client_id].state != SessionState.authenticated:
            return

        if len(data) < 23:
            return # Frame is too short to contain MAC addresses

        #MAC addresses
        destination_mac_addrr = data[11:17]
        source_mac_addrr = data[17:23]
        threading.Thread(target=self.learn_mac, args=(source_mac_addrr, client_id)).start()

        if destination_mac_addrr == b'\xff\xff\xff\xff\xff\xff': #Broadcast address
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

    def _handle_keepalive(self, client_id):
        #Update last_seen
        if client_id in self.clients and self.clients[client_id].state == SessionState.authenticated:
            self.clients[client_id].last_seen = time.time()

    def send_reject(self, client_ip, client_port, id_high, id_low):
        # Build header
        response_header = bytes([OpCode.REJECT, id_high, id_low])
        response_packet = response_header + (b'\x00' * 8)

        self.sock.sendto(response_packet, (client_ip, client_port))

    def send_ack(self, client_ip, client_port, id_high, id_low):
        # Build header
        response_header = bytes([OpCode.ACK, id_high, id_low])
        response_packet = response_header + (b'\x00' * 8)

        self.sock.sendto(response_packet, (client_ip, client_port))

    def send_traffic(self, frame, destination_client_id):
        self.sock.sendto(frame, (self.clients[destination_client_id].ip, self.clients[destination_client_id].port))
        self.clients[destination_client_id].pktsOut += 1
        self.clients[destination_client_id].bytesOut += len(frame)

    def broadcast_packet(self, client_id, data):
        for iter_client_id, session in self.clients.items():
            if iter_client_id != client_id and session.state == SessionState.authenticated:
                self.send_traffic(data, iter_client_id)

    def learn_mac(self, source_mac, client_id):
        with self.mac_lock:
            self.mac_table[source_mac] = client_id

        logger.debug(f"Learned MAC {source_mac.hex()} belongs to client {client_id}")

    def cleanup_sessions(self):
        timeout_seconds = self.config.timeout

        while True:
            time.sleep(3) # The thread sleeps, then wakes up every 3 seconds
            current_time = time.time()

            for client_id in list(self.clients.keys()):
                session = self.clients[client_id]

                if session.state == SessionState.authenticated and current_time - session.last_seen > timeout_seconds:
                    logger.info(f"Client {client_id} timed out. Deleting session.")
                    del self.clients[client_id]

    def print_stats(self):
        while True:
            time.sleep(self.config.stats_interval)

            current_time = time.strftime("%Y-%m-%dT%H:%M:%S", time.localtime())
            print(f"\n=== Stats at {current_time} ===")
            print(f"{'ClientID':>8}  {'Addr':<22} {'PktsIn':>10} {'PktsOut':>10} {'BytesIn':>10} {'BytesOut':>10}")

            #Print client stats
            for client_id in list(self.clients.keys()):
                session = self.clients[client_id]
                addr_str = f"{session.ip}:{session.port}"
                print(f"{client_id:>8}  {addr_str:<22} {session.pktsIn:>10} {session.pktsOut:>10} {session.bytesIn:>10} {session.bytesOut:>10}")

            #Print the global stats
            print(f"Global — broadcasts: {self.broadcasts}  unicasts: {self.unicasts}  unknown_mac: {self.unknown_mac}  discarded: {self.discarded}")
            print("-" * 75) # A separator line for readability