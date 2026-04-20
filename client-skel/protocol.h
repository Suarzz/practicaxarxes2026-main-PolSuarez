//
// Created by pol on 4/18/26.
//

#ifndef PRACTICAXARXES2026_MAIN_PROTOCOL_H
#define PRACTICAXARXES2026_MAIN_PROTOCOL_H
#include <stdint.h>
#include <netinet/in.h>
#endif //PRACTICAXARXES2026_MAIN_PROTOCOL_H

#define VPN_HEADER_SIZE 11
typedef struct {
    uint8_t opcode;
    uint16_t client_id;
    uint8_t payload[8];
} vpn_header_t;