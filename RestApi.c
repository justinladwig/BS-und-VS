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
        strcpy(response, "HTTP/1.1 200 OK\nContent-Type: text/plain\n\nHello, world!");
        write(client_socket, response, strlen(response));
    } else if(strncmp(buffer, "GET /key/", 9) == 0) {
        char *key = strtok(buffer + 9, " /");
        sprintf(user_json, "[{\"key\": \"%s\", \"value\": \"%s\"}]", key, get(key));
        sprintf(response, "HTTP/1.1 200 OK\nContent-Type: application/json\nContent-Length: %ld\r\n\r\n%s", strlen(user_json) , user_json);
    } else if (strncmp(buffer, "PUT /key/", 9)== 0) {
        char *key = strtok(buffer + 9, " /");
        char *body_start = strstr(buffer + 9 + strlen(key) + 1, "\r\n\r\n") + 4;
        char body[100] = {"\0"};
        strcpy(body, body_start);
        // TODO: Parse the incoming JSON body and update the key-value store
        int numTokens = 0;
        switch (numTokens = jsmn_parse(&parser, body, strlen(body), tokens, 10)) {
            case JSMN_ERROR_INVAL: printf("Invalid JSON string.\n"); return;
            case JSMN_ERROR_NOMEM: printf("Not enough tokens.\n"); return;
            case JSMN_ERROR_PART: printf("JSON string is too short, expecting more JSON data.\n"); return;
        }
        char tokenKey[100] = {"\0"};
        char tokenValue[100] = {"\0"};
        for (int i = 0; i < numTokens; i++) {
            if (tokens[i].type == JSMN_STRING && strncmp(body + tokens[i].start, "key", tokens[i].end - tokens[i].start) == 0) {
                strncpy(tokenKey, body + tokens[i + 1].start, tokens[i + 1].end - tokens[i + 1].start);
                if (strncmp(body + tokens[i + 2].start, "value", tokens[i + 2].end - tokens[i + 2].start) == 0) {
                    strncpy(tokenValue, body + tokens[i + 3].start, tokens[i + 3].end - tokens[i + 3].start);
                    break;
                }
                printf("Invalid JSON string.\n");
                return;
            }
            if (i == numTokens - 1) {
                printf("Invalid JSON string.\n");
                return;
            }
        }
        if (strcmp(tokenKey, key) != 0) {
            printf("Key, does not match to Ressource!\n");
            return;
        }
        if (contains(tokenKey) == 0) { //Überprüfen, ob Key bereits vorhanden ist, dann soll der neue Wert in der Hashmap gespeichert werden und der alte Wert zurückgegeben werden
            char *OldGet = get(tokenKey);
            char *old_ = malloc(strlen(OldGet) + 1);
            strcpy(old_,OldGet); //Alten Wert in einen neuen String kopieren, da dieser sonst überschrieben wird
            change(key, tokenValue); //Value wird geändert
            printf("PUT (REST): Key %s wurde geändert. Alter Wert: %s, neuer Wert: %s\n", key, old_, tokenValue);
            sprintf(user_json, "[{\"key\": \"%s\", \"value\": \"%s\", \"status\": \"changed\", \"old_value\": \"%s\"}]", tokenKey, tokenValue,  old_);
            sprintf(response, "HTTP/1.1 200 OK\nContent-Type: application/json\nContent-Length: %ld\r\n\r\n%s", strlen(user_json) , user_json);
        } else {
            if (put(key, tokenValue) == -1) { //Key und Value werden in die Hashmap gespeichert
                printf("PUT: Key %s konnte nicht hinzugefügt werden: Kein Speicherplatz verfügbar\n", key);
            } else {
                printf("PUT: Key %s wurde hinzugefügt. Wert: %s\n", key, tokenValue);
                sprintf(user_json, "[{\"key\": \"%s\", \"value\": \"%s\", \"status\": \"added\"}]", tokenKey, tokenValue);
                sprintf(response, "HTTP/1.1 200 OK\nContent-Type: application/json\nContent-Length: %ld\r\n\r\n%s", strlen(user_json) , user_json);
            }
        }
    } else if (strncmp(buffer, "GET /key", 8) == 0) {
        // TODO: Return all key-value pairs in the key-value store as JSON
    } else if (strncmp(buffer, "DELETE /key/", 12) == 0) {
        char *key = strtok(buffer + 12, " /");
        sprintf(user_json, "[{\"key\": \"%s\", \"value\": \"%s\", \"status\": \"deleted\"}]", key, get(key));
        delete(key);
        sprintf(response, "HTTP/1.1 200 OK\nContent-Type: application/json\nContent-Length: %ld\r\n\r\n%s", strlen(user_json) , user_json);
    } else {
        strcpy(response, "HTTP/1.1 404 Not Found\nContent-Type: text/plain\n\nNot found.");
    }
    write(client_socket, response, strlen(response));
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
        close(client_socket);
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
