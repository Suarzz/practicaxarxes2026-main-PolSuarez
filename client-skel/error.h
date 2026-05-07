//
// Created by pol on 5/7/26.
//

#ifndef PRACTICAXARXES2026_MAIN_ERROR_H
#define PRACTICAXARXES2026_MAIN_ERROR_H
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

//Prints an error message and closes the client's sockets and tap devices
void throw_error(const char* message, int sock, int tap);
#endif //PRACTICAXARXES2026_MAIN_ERROR_H
