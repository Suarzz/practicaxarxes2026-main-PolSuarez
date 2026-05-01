//
// Created by pol on 4/18/26.
//

#include "protocol.h"

uint64_t htonll(uint64_t value) {
    //The number is 64 bits (8 bytes). We use standard 32-bit htonl on the top and bottom halves.
    int num = 1;
    if(*(char *)&num == 1) { //If the system has its first byte set as 1 when storing 1, it means it uses Little-Endian
        uint32_t high_part = htonl((uint32_t)(value >> 32));
        uint32_t low_part = htonl((uint32_t)value);
        return (((uint64_t)low_part) << 32) | high_part;
        //Slice the number, apply htonl and then do a final swap for the 2 parts
    }
    return value; //Already Big-Endian
}

vpn_header_t fill_payload(vpn_header_t header, const char* password, uint64_t seq_num)
{
    switch (header.opcode)
    {
    case AUTH_OPCODE:
        for (int i = 0; i < 8; i++)
        {
            header.payload[i] = password[i];
        }
        break;
    case TRAFFIC_OPCODE:
            seq_num = htonll(seq_num);
            memcpy(header.payload, &seq_num, 8);
        break;
    default: //Any other case is filled with zeros
        for(int i = 0; i < 8; i++) {
            header.payload[i] = 0;
        }
        break;
    }

    return header;
}

uint8_t extract_opcode(uint8_t * buffer)
{
    vpn_header_t* received_header = (vpn_header_t *)buffer;
    return received_header->opcode;
}

vpn_header_t create_pixes_header(uint16_t my_client_id, uint8_t opcode, char* password, uint64_t seq_num)
{
    vpn_header_t header;
    header.opcode = opcode;

    header.client_id = htons(my_client_id);

    header = fill_payload(header, password, seq_num);


    return header;
}

