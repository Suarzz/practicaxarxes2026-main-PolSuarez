#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <signal.h>
#include <linux/if_tun.h>
#include <sys/select.h>
#include <sys/time.h>
#include <time.h>

#include "udp.h"
#include "protocol.h"
#include "args.h"
#include "tap.h"
#include "error.h"


#define KEEPALIVE_INTERVAL_SEC 10
#define MAX_FRAME_SIZE         65535

//global fds so the signal handler can access them
int global_tap_fd;
int global_sock_fd;


void client_run(vpn_config_t *cfg, int tap_fd)
{
    int sock = create_socket();
    global_sock_fd = sock;

    struct sockaddr_in server_addr = create_server_address(cfg->port, cfg->server_ip);

    //5 second socket timeout
    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)); //Set timer options

    //Build and send RESGISTER
    vpn_header_t reg_header = create_pixes_header(cfg->client_id, REGISTER_OPCODE, "", 0);
    send_to_server(sock, (uint8_t*)&reg_header, &server_addr, VPN_HEADER_SIZE);

    //Wait for the server's reply
    static uint8_t buffer[MAX_FRAME_SIZE + VPN_HEADER_SIZE]; //REVISAR
    struct sockaddr_in sender_addr; //use a different struct so the original doesn't get overwritten??
    ssize_t bytes_received = receive_from_server(sock, buffer, MAX_FRAME_SIZE + VPN_HEADER_SIZE, &sender_addr);

    //If after the 5s there are no bytes received, error
    if (bytes_received <= 0)
    {
        throw_error("Error: Registration failed due to timeout.\n", global_sock_fd, global_tap_fd);
    }

    vpn_header_t *received_header = (vpn_header_t *)buffer;

    if (received_header->opcode != ACK_OPCODE) //extract
    {
        throw_error("Error: Registration rejected by server.\n", global_sock_fd, global_tap_fd);
    } else
    {
        printf("REGISTRATION COMPLETED \n");
    }

    //AUTHENTICATING STATE
    vpn_header_t auth_header = create_pixes_header(cfg->client_id, AUTH_OPCODE, cfg->password, 0);
    int auth_attempts_remaining = 3;
    while (1)
    {
        if (auth_attempts_remaining == 0)
        {
            throw_error("Error: No more registration attempts remaining \n", global_sock_fd, global_tap_fd);
        }
        auth_attempts_remaining--;

        send_to_server(sock, (uint8_t*)&auth_header, &server_addr, VPN_HEADER_SIZE);

        bytes_received = receive_from_server(sock, buffer, MAX_FRAME_SIZE, &sender_addr);

        if (bytes_received <= 0)
        {
            printf("Authentication failed due to timeout. %i attempts remaining \n", auth_attempts_remaining);
            continue;
        }

        received_header = (vpn_header_t *)buffer;
        if (received_header->opcode != ACK_OPCODE)
        {
            printf("Authentication rejected by server. %i attempts remaining \n", auth_attempts_remaining);
            continue;
        }
        //If no timeout or rejection, we enter ESTABLISHED state
        printf("AUTHENTICATION COMPLETED \n");
        break;
    }

 //ESTABLISHED STATE
    uint64_t seq_num = 0;
    fd_set read_fds;
    int max_fd = (sock > tap_fd) ? sock : tap_fd;

    time_t last_seen = time(NULL); // Track the exact time we start

    while (1)
    {
        time_t now = time(NULL);
        int time_left = KEEPALIVE_INTERVAL_SEC - (now - last_seen);

        // If 10 seconds have passed, send the keepalive and reset the clock
        if (time_left <= 0)
        {
            printf("SENDING KEEPALIVE \n");
            vpn_header_t keepalive_header = create_pixes_header(cfg->client_id, KEEPALIVE_OPCODE, "", 0);
            send_to_server(sock, (uint8_t *)&keepalive_header, &server_addr, VPN_HEADER_SIZE);

            last_seen = time(NULL);
            time_left = KEEPALIVE_INTERVAL_SEC;
        }

        struct timeval select_timeout;
        select_timeout.tv_sec = time_left; // Tell select to only wait for the REMAINING time
        select_timeout.tv_usec = 0;

        // Reset and fill the checklist
        FD_ZERO(&read_fds);
        FD_SET(sock, &read_fds);
        FD_SET(tap_fd, &read_fds);

        // Pause the program until the socket or TAP gets data, or time_left runs out
        int activity = select(max_fd + 1, &read_fds, NULL, NULL, &select_timeout);

        if (activity < 0) {
            // Good practice to handle select errors (e.g., interrupted by signals)
            continue;
        }

        // Did the TAP device wake us up?
        if (FD_ISSET(tap_fd, &read_fds)) {
            uint8_t eth_frame[MAX_FRAME_SIZE];
            ssize_t frame_len = tap_read(tap_fd, eth_frame, sizeof(eth_frame));
            uint8_t packet_buffer[MAX_FRAME_SIZE + VPN_HEADER_SIZE];

            if (frame_len > 0)
            {
                printf("TAP -> Server: Forwarding Ethernet frame (%zd bytes)\n", frame_len);
                vpn_header_t traffic_header = create_pixes_header(cfg->client_id, TRAFFIC_OPCODE, "", seq_num);
                seq_num++;
                memcpy(packet_buffer, &traffic_header, VPN_HEADER_SIZE);
                memcpy(packet_buffer + VPN_HEADER_SIZE, eth_frame, frame_len);
                send_to_server(sock, packet_buffer, &server_addr, VPN_HEADER_SIZE + frame_len);

                last_seen = time(NULL);
            }
        }

        // Did the Socket wake us up?
        if (FD_ISSET(sock, &read_fds)) {
            bytes_received = receive_from_server(sock, buffer, sizeof(buffer), &sender_addr);

            if (bytes_received > 0)
            {
                printf("Server -> TAP: Received packet, opcode 0x%02x\n", buffer[0]);
                if (extract_opcode(buffer) == TRAFFIC_OPCODE && bytes_received > VPN_HEADER_SIZE)
                {
                    tap_write(tap_fd, buffer + VPN_HEADER_SIZE, bytes_received - VPN_HEADER_SIZE);
                }
            }
        }

    }


}

void handle_shutdown(int signum)
{
    (void) signum;
    close(global_sock_fd);
    close(global_tap_fd);
    exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
    signal(SIGINT, handle_shutdown); //kill signal handler
    signal(SIGTERM, handle_shutdown);

    vpn_config_t cfg;
    int ret = parse_args(argc, argv, &cfg);
    if (ret <= 0) {
        /* ret == 0 -> parsing error exit -1, ret < 0 -> help requested exit 0*/
        return (ret < 0) ? 0 : -1;
    }

    // Open TAP device
    int tap_fd = tap_open(cfg.tap_if);
    if (tap_fd < 0) {
        fprintf(stderr, "Error: could not open TAP device %s\n", cfg.tap_if);
        return 1;
    }
    global_tap_fd = tap_fd;
    client_run(&cfg, tap_fd);

    return 0;
}

