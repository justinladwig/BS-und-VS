//
// Created by LeonSchneider on 28.04.2023.
//

#ifndef PRAKBS23_RESTAPI_H
#define PRAKBS23_RESTAPI_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define RESTPORT 8080
#define BUFFER_SIZE 1024

typedef struct {
    // Server socket used for listening for incoming connections
    int server_socket;
} RestApi;

RestApi* create();

void run(RestApi* rest_api);

void destroy(RestApi* rest_api);

#endif //PRAKBS23_RESTAPI_H
