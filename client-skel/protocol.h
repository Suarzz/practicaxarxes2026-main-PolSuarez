//
// Created by pol on 4/18/26.
//

#ifndef PRACTICAXARXES2026_MAIN_PROTOCOL_H
#define PRACTICAXARXES2026_MAIN_PROTOCOL_H
#include <string.h>
#include <netinet/in.h>


#define VPN_HEADER_SIZE 11
#define REGISTER_OPCODE 0x01
#define AUTH_OPCODE 0x02
#define TRAFFIC_OPCODE 0x03
#define KEEPALIVE_OPCODE 0x04
#define ACK_OPCODE 0x05
#define REJECT_OPCODE 0x06

typedef struct {
    uint8_t opcode;
    uint16_t client_id;
    uint8_t payload[8];
} vpn_header_t;

//Changes 64bit integer from little-endian to big-endian
uint64_t htonll(uint64_t value);

//Fills the header's payload with the correct content according to pixes protocol, depending on the opcode
vpn_header_t fill_payload(vpn_header_t header, char* password, uint64_t seq_num);

//Extracts the opcode from a given pixes frame
uint8_t extract_opcode(uint8_t * buffer);

//Creates a pixes header with the parameers given
vpn_header_t create_pixes_header(uint16_t my_client_id, uint8_t opcode, char* password, uint64_t seq_num);
#endif //PRACTICAXARXES2026_MAIN_PROTOCOL_H