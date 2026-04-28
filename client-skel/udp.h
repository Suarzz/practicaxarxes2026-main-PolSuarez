//
// Created by polsu on 18/04/2026.
//

#ifndef PRACTICAXARXES2026_MAIN_POLSUAREZ_UDP_H
#define PRACTICAXARXES2026_MAIN_POLSUAREZ_UDP_H



#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

//Creates a socket and returns the file descriptor number for that socket
int create_socket(void);

//Given the server ip and the port we want to assign it, creates the struct for the server address
struct sockaddr_in create_server_address(int port, const char* server_ip);

//Sends the given packet to the server and returns the number of bytes sent
ssize_t send_to_server(int sock, uint8_t* packet, const struct sockaddr_in* server_addr, int packet_len);

//Receives date from the server and places it in the given buffer, returns the number of bytes received
ssize_t receive_from_server(int sock, uint8_t* buffer, size_t buffer_len, struct sockaddr_in* server_addr);

//Closes the socket for the given file descriptor number
void close_socket(int sock);

#endif //PRACTICAXARXES2026_MAIN_POLSUAREZ_UDP_H