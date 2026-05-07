//
// Created by pol on 5/7/26.
//

#ifndef PRACTICAXARXES2026_MAIN_ARGS_H
#define PRACTICAXARXES2026_MAIN_ARGS_H

/* Configuration parsed from command-line arguments */
typedef struct {
    const char *tap_if;
    const char *server_ip;
    int         port;
    int         client_id;
    char        password[9]; /* 8 chars + NUL */
} vpn_config_t;

//Validate a password string. Returns 0 if the password is valid, or -1 if it is not.
//A password is considered valid if it is exactly 8 characters
//long and only contains alphanumeric characters (A-Za-z0-9).
int validate_password(const char *pw);

//Parse command-line arguments into a vpn_config_t structure.
//Returns 1 on success, 0 on parsing error (an error message and/or usage is printed to stderr), and -1 if "--help" was requested.
//The cfg structure is zeroed before parsing so callers do not need to initialize it.
//Return codes:
//  1  success
//  0  parsing error (an error message and/or usage is printed to stderr)
// -1  "--help" was requested (usage printed, caller should exit 0)
int parse_args(int argc, char *argv[], vpn_config_t *cfg);

void print_usage(const char *prog);


#endif //PRACTICAXARXES2026_MAIN_ARGS_H
