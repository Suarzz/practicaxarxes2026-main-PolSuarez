//
// Created by pol on 5/7/26.
//
#include "args.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int validate_password(const char *pw)
{
    int i;
    if (strlen(pw) != 8) {
        return -1;
    }
    for (i = 0; i < 8; i++) {
        if (!isalnum((unsigned char)pw[i])) {
            return -1;
        }
    }
    return 0;
}

int parse_args(int argc, char *argv[], vpn_config_t *cfg)
{
    int i;
    int has_tap = 0, has_server = 0, has_port = 0, has_id = 0, has_password = 0;

    memset(cfg, 0, sizeof(*cfg));

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return -1; /* help requested */

        } else if (strcmp(argv[i], "--tap") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --tap requires an argument\n");
                print_usage(argv[0]);
                return 0;
            }
            cfg->tap_if = argv[++i];
            has_tap = 1;

        } else if (strcmp(argv[i], "--server") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --server requires an argument\n");
                print_usage(argv[0]);
                return 0;
            }
            cfg->server_ip = argv[++i];
            has_server = 1;

        } else if (strcmp(argv[i], "--port") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --port requires an argument\n");
                print_usage(argv[0]);
                return 0;
            }
            {
                char *end;
                long val = strtol(argv[++i], &end, 10);
                if (*end != '\0' || val < 1 || val > 65535) {
                    fprintf(stderr, "Error: --port must be in range 1-65535\n");
                    print_usage(argv[0]);
                    return 0;
                }
                cfg->port = (int)val;
            }
            has_port = 1;

        } else if (strcmp(argv[i], "--id") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --id requires an argument\n");
                print_usage(argv[0]);
                return 0;
            }
            {
                char *end;
                long val = strtol(argv[++i], &end, 10);
                if (*end != '\0' || val < 0 || val > 65535) {
                    fprintf(stderr, "Error: --id must be in range 0-65535\n");
                    print_usage(argv[0]);
                    return 0;
                }
                cfg->client_id = (int)val;
            }
            has_id = 1;

        } else if (strcmp(argv[i], "--password") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --password requires an argument\n");
                print_usage(argv[0]);
                return 0;
            }
            {
                const char *pw = argv[++i];
                if (validate_password(pw) != 0) {
                    fprintf(stderr,
                        "Error: --password must be exactly 8 alphanumeric characters [A-Za-z0-9]\n");
                    return 0;
                }
                memcpy(cfg->password, pw, 8);
                cfg->password[8] = '\0';
            }
            has_password = 1;

        } else {
            fprintf(stderr, "Error: unknown argument: %s\n", argv[i]);
            print_usage(argv[0]);
            return 0;
        }
    }

    if (!has_tap || !has_server || !has_port || !has_id || !has_password) {
        fprintf(stderr, "Error: missing required arguments\n");
        print_usage(argv[0]);
        return 0;
    }

    return 1;
}

void print_usage(const char *prog)
{
    fprintf(stderr,
        "Usage: %s --tap <interface> --server <ip> --port <port>\n"
        "           --id <number> --password <string>\n"
        "\n"
        "Options:\n"
        "  --tap      <interface>  TAP interface name (e.g. tap0)\n"
        "  --server   <ip>         Server IPv4 address\n"
        "  --port     <port>       UDP port (1-65535)\n"
        "  --id       <number>     Client ID (0-65535)\n"
        "  --password <string>     8-character alphanumeric password [A-Za-z0-9]\n"
        "  --help                  Print this usage and exit\n",
        prog);
}