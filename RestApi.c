//
// Created by LeonSchneider on 28.04.2023.
//
#include "RestApi.h"

typedef struct {
    // Server socket used for listening for incoming connections
    int server_socket;
} RestApi;

void handle_request(int client_socket) {
    char buffer[BUFFER_SIZE] = {0};
    ssize_t read_size = read(client_socket, buffer, BUFFER_SIZE);

    // Parse the incoming request to determine what action to take.
    if (strncmp(buffer, "GET /hello", 10) == 0) {
        const char* response = "HTTP/1.1 200 OK\nContent-Type: text/plain\n\nHello, world!";
        write(client_socket, response, strlen(response));
    } else {
        const char* response = "HTTP/1.1 404 Not Found\nContent-Type: text/plain\n\nNot found.";
        write(client_socket, response, strlen(response));
    }

    close(client_socket);
}

// Creates a new instance of the RestApi class
// @return A pointer to a new instance of the RestApi class
RestApi* rest_api_create() {
    RestApi* rest_api = malloc(sizeof(RestApi));
    if (rest_api == NULL) {
        perror("failed to allocate memory for RestApi");
        exit(EXIT_FAILURE);
    }

    rest_api->server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (rest_api->server_socket == -1) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in address = {0};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    int bind_result = bind(rest_api->server_socket, (struct sockaddr*)&address, sizeof(address));
    if (bind_result == -1) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    int listen_result = listen(rest_api->server_socket, 5);
    if (listen_result == -1) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    return rest_api;
}

// Starts the REST API server and listens for incoming connections
// @param rest_api A pointer to an instance of the RestApi class
void rest_api_run(RestApi* rest_api) {
    while (true) {
        struct sockaddr_in client_address = {0};
        socklen_t client_address_length = sizeof(client_address);
        int client_socket = accept(rest_api->server_socket, (struct sockaddr*)&client_address, &client_address_length);
        if (client_socket == -1) {
            perror("accept failed");
            continue;
        }

        handle_request(client_socket);
    }
}

// Destroys an instance of the RestApi class and frees the associated resources
// @param rest_api A pointer to an instance of the RestApi class
void rest_api_destroy(RestApi* rest_api) {
    if (rest_api != NULL) {
        close(rest_api->server_socket);
        free(rest_api);
    }
}
