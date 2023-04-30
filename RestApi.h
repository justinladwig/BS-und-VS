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

#define PORT 8080
#define BUFFER_SIZE 1024

RestApi* rest_api_create();

void rest_api_run(RestApi* rest_api);

void rest_api_destroy(RestApi* rest_api);

#endif //PRAKBS23_RESTAPI_H
