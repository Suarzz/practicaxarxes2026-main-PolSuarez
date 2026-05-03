import socket
from loguru import logger
class VpnServer:


    def __init__(self, config):
        self.config = config

        # Create an IPv4 UDP socket
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

        # Bind to all interfaces (0.0.0.0) on the user-provided port
        self.sock.bind(("0.0.0.0", self.config.port))

        self.clients = {}

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

            if(opcode == 1):
                #REGISTERING
                self.clients[client_id] = (client_ip, client_port)
            elif (opcode == 2):
                #AUTH
                if len(payload) == 8 and client_id in self.clients:
                    pass
                else:
                    self.sock.sendto()
            elif (opcode == 3):
                #TRAFFIC
                pass

            #Destination MAC address
            destination_mac_addrr = data[11:17]

    def send_reject(self, client_ip, client_port, id_high, id_low):
        reject_opcode = 4 # (Replace with your actual REJECT opcode!)

        # Build the 11-byte packet
        response_header = bytes([reject_opcode, id_high, id_low])
        response_packet = response_header + (b'\x00' * 8)

        # Send it back to the client
        self.sock.sendto(response_packet, (client_ip, client_port))