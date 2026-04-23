//
// Created by polsu on 18/04/2026.
//

#include "udp.h"

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


int create_socket(void)
{
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    return sock;
}

struct sockaddr_in create_server_address(int port, char* server_ip)
{
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(server_ip);
    return server_addr;
}

ssize_t send_to_server(int sock, uint8_t* packet, const struct sockaddr_in* server_addr, int packet_len)
{
    return sendto(sock, packet, packet_len, 0, (struct sockaddr *)server_addr, sizeof(struct sockaddr_in));
}

ssize_t receive_from_server(int sock, uint8_t* buffer, size_t buffer_len, struct sockaddr_in* server_addr)
{
    socklen_t addr_len = sizeof(struct sockaddr_in);
    return recvfrom(sock, buffer, buffer_len, 0, (struct sockaddr *)server_addr, &addr_len);
}

void close_socket(int sock)
{
    close(sock);
}
