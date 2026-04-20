//
// Created by pol on 4/18/26.
//

#include "protocol.h"



vpn_header_t create_register_header(uint16_t my_client_id) {
    vpn_header_t header;
    header.opcode = 0x01;

    // Fill payload with zeros according to protocol
    for(int i = 0; i < 8; i++) {
        header.payload[i] = 0;
    }

    header.client_id = htons(my_client_id);

    return header;
}
