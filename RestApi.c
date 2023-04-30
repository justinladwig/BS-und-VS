#include "RestApi.h"
#include "keyValStore.h"
#include "jsmn.h"

jsmn_parser parser;
jsmntok_t tokens[10];


void handle_request(int client_socket) {
    char buffer[BUFFER_SIZE] = {0};
    ssize_t read_size = read(client_socket, buffer, BUFFER_SIZE);

    char user_json[200];
    char response[200];
    // Parse the incoming request to determine what action to take.
    if (strncmp(buffer, "GET /hello", 10) == 0) {
        const char *response = "HTTP/1.1 200 OK\nContent-Type: text/plain\n\nHello, world!";
        write(client_socket, response, strlen(response));
    } else if(strncmp(buffer, "GET /key/", 9) == 0) {
        char *key = strtok(buffer + 9, " /");
        sprintf(user_json, "[{\"key\": \"%s\", \"value\": \"%s\"}]", key, get(key));
    } else if (strncmp(buffer, "PUT /key/", 9)== 0) {
        char *key = strtok(buffer + 9, " /");
        char *body_start = strstr(buffer + 9 + strlen(key) + 1, "\r\n\r\n") + 4;
        char body[100] = {"\0"};
        strcpy(body, body_start);
        // TODO: Parse the incoming JSON body and update the key-value store
        switch (jsmn_parse(&parser, body, strlen(body), tokens, 10)) {
            case JSMN_ERROR_INVAL: printf("Invalid JSON string.\n"); return;
            case JSMN_ERROR_NOMEM: printf("Not enough tokens.\n"); return;
            case JSMN_ERROR_PART: printf("JSON string is too short, expecting more JSON data.\n"); return;
        }
        char tokenKey[100] = {"\0"};
        strncpy(tokenKey, body + tokens[2].start, tokens[2].end - tokens[2].start);
        if ((tokens[2].type != JSMN_STRING) || (strcmp(tokenKey, key) != 0)) {
            printf("Invalid JSON string.\n");
            return;
        }
        char tokenValue[100] = {"\0"};
        strncpy(tokenValue, body + tokens[4].start, tokens[4].end - tokens[4].start);
        if (tokens[4].type != JSMN_STRING) {
            printf("Invalid JSON string.\n");
            return;
        }
        //Telnet stürzt ab ...
        put(tokenKey,tokenValue);
    } else if (strncmp(buffer, "GET /key", 8) == 0) {
        // TODO: Return all key-value pairs in the key-value store as JSON
    } else if (strncmp(buffer, "DELETE /key/", 12) == 0) {
        char *key = strtok(buffer + 12, " /");
        sprintf(user_json, "[{\"key\": \"%s\", \"value\": \"%s\", \"status\": \"deleted\"}]", key, get(key));
        delete(key);
    } else {
        const char* response = "HTTP/1.1 404 Not Found\nContent-Type: text/plain\n\nNot found.";
        write(client_socket, response, strlen(response));
        return;
    }
    sprintf(response, "HTTP/1.1 200 OK\nContent-Type: application/json\nContent-Length: %ld\r\n\r\n%s", strlen(user_json) , user_json);
    write(client_socket, response, strlen(response));

    close(client_socket);
}

// Creates a new instance of the RestApi class
// @return A pointer to a new instance of the RestApi class
RestApi* create() {
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
    address.sin_port = htons(RESTPORT);

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
void run(RestApi* rest_api) {
    jsmn_init(&parser);
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
void destroy(RestApi* rest_api) {
    if (rest_api != NULL) {
        close(rest_api->server_socket);
        free(rest_api);
    }
}
