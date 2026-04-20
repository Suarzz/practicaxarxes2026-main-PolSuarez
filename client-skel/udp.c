//
// Created by polsu on 18/04/2026.
//

#include "udp.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "protocol.h"

int createSocket(void)
{
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    return sock;
}

struct sockaddr_in createServerAddress(int port, char* server_ip)
{
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(server_ip);
    return server_addr;
}

void sendResgister(int sock, vpn_header_t header, struct sockaddr_in server_addr)
{
    sendto(sock, &header, VPN_HEADER_SIZE, 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
}
