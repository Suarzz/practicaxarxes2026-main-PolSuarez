//
// Created by pol on 5/7/26.
//
#include "error.h"

void throw_error(const char* message, int sock, int tap)
{
    fprintf(stderr, "%s\n", message);
    close(sock);
    close(tap);
    exit(EXIT_FAILURE);
}