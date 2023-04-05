/*******************************************************************************
 
  Ein TCP-Echo-Server als iterativer Server: Der Server schickt einfach die
  Daten, die der Client schickt, an den Client zurück.
 
*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <ctype.h>
#include "keyValStore.h"
#include "sub.h"

#define BUFSIZE 1024 // Größe des Buffers
#define ENDLOSSCHLEIFE 1
#define PORT 5678

/*******************************************************************************

  Funktionsdeklerationen

*******************************************************************************/

//Gibt 0 zurück, wenn keine alphanumerischen Zeichen oder Leerzeichen im String sind, sonst -1
int check_key(char *key);

//Gibt 0 zurück, wenn keine alphanumerischen Zeichen im String sind, sonst -1
int check_value(char *value);

//Ausgabe im Format "> pfx:key:value\r\n"
char *getoutputString(char *pfx, char *key, char *value);

enum error_codes {
    not_alphanumeric = 1,
    unknown_command = 2,
    too_many_arguments = 3,
    unknown_error = 4
};

/**
 * Sendet einen Fehler an den Client
 * @param err_code Fehlercode aus enum error_codes
 * @param cfd Client-File-Descriptor
 * @return  Anzahl der gesendeten Bytes oder -1 bei Fehler
 */
long sendError(enum error_codes err_code, int cfd);

//Auswertung der Kommandos
int commandInterpreter(char *comm, int cfd);

/*******************************************************************************
 *
 *******************************************************************************/

int main() {

    int rfd; // Rendevouz-Descriptor
    int cfd; // Verbindungs-Descriptor

    struct sockaddr_in client; // Socketadresse eines Clients
    socklen_t client_len = sizeof(client); // Länge der Client-Daten
    char in[BUFSIZE]; // Daten vom Client an den Server
    int bytes_read; // Anzahl der Bytes, die der Client geschickt hat


    // Socket erstellen
    rfd = socket(AF_INET, SOCK_STREAM, 0);
    if (rfd < 0) {
        fprintf(stderr, "socket konnte nicht erstellt werden\n");
        exit(-1);
    }


    // Socket Optionen setzen für schnelles wiederholtes Binden der Adresse
    int option = 1;
    setsockopt(rfd, SOL_SOCKET, SO_REUSEADDR, (const void *) &option, sizeof(int));


    // Socket binden
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(PORT);
    int brt = bind(rfd, (struct sockaddr *) &server, sizeof(server));
    if (brt < 0) {
        fprintf(stderr, "socket konnte nicht gebunden werden\n");
        exit(-1);
    }


    // Socket lauschen lassen
    int lrt = listen(rfd, 5);
    if (lrt < 0) {
        fprintf(stderr, "socket konnte nicht listen gesetzt werden\n");
        exit(-1);
    }

    while (ENDLOSSCHLEIFE) {

        // Verbindung eines Clients wird entgegengenommen
        cfd = accept(rfd, (struct sockaddr *) &client, &client_len);

        // Lesen von Daten, die der Client schickt
        bytes_read = read(cfd, in, BUFSIZE);

        // Interpretieren von Daten, die der Client schickt
        while (bytes_read > 0) {
            char *comm = malloc(BUFSIZE + 1);
            strncpy(comm, in, bytes_read);
            comm[bytes_read] = '\0';

            printf("%d bytes received from %s:%d\n", bytes_read, inet_ntoa(client.sin_addr), ntohs(client.sin_port));

            //Auswertung der Kommandos
            if (commandInterpreter(comm, cfd)) {
                break;
            }

            bytes_read = read(cfd, in, BUFSIZE); //Lesen von Daten, die der Client schickt
        }
        close(cfd);
    }

    // Rendezvous Descriptor schließen
    close(rfd);

}

int commandInterpreter(char *comm, int cfd) {
    // Befehl in einzelne Teile zerlegen
    char delimiter[] = " \r\n";
    char *pfx = strtok(comm, delimiter);
    char *key = strtok(NULL, delimiter);
    char *value = strtok(NULL, "\r\n");

    // Überprüfen, ob es sich bei dem Befehl um einen PUT, GET oder DEL handelt
    if (strcmp(comm, "PUT") == 0) { // Befehl PUT
        if (check_key(key) != 0 || check_value(value) !=0) { //Überprüfen, dass Key nur alphanumerische Zeichen und keine Leerzeichen enthält und Value nur alphanumerische Zeichen enthält
            sendError(not_alphanumeric, cfd);
        } else {
            if (contains(key) ==0) { //Überprüfen, ob Key bereits vorhanden ist, dann soll der neue Wert in der Hashmap gespeichert werden und der alte Wert zurückgegeben werden
                char *OldGet = get(key);
                char *old_ = malloc(strlen(OldGet) + 1);
                strcpy(old_, OldGet); //Alten Wert in einen neuen String kopieren, da dieser sonst überschrieben wird
                change(key, value); //Value wird geändert
                value = old_;
            } else {
                put(key, value); //Key und Value werden in die Hashmap gespeichert
            }
            char *out = getoutputString(pfx, key, value);
            write(cfd, out, strlen(out) + 1);
        }
    } else if (strcmp(comm, "GET") == 0) { // Befehl GET
        if (value != NULL) { //Überprüfen, dass kein Value angegeben wurde
            sendError(too_many_arguments, cfd);
        } else {
            value = get(key); //Wert wird aus der Hashmap geholt
            if (check_key(key) !=0) { //Überprüfen, dass Key nur alphanumerische Zeichen und keine Leerzeichen enthält
                sendError(not_alphanumeric, cfd);
            } else {
                if (value == NULL) { //Wenn der Key nicht existiert
                    value = "key_nonexistent";
                }
                char *out = getoutputString(pfx, key, value);
                write(cfd, out, strlen(out) + 1);
            }
        }
    } else if (strcmp(comm, "DEL") == 0) { // Befehl DEL
        if (value != NULL) { //Überprüfen, dass kein Value angegeben wurde
            sendError(too_many_arguments, cfd);
        } else {
            if (check_key(key) !=0) { //Überprüfen, dass Key nur alphanumerische Zeichen und keine Leerzeichen enthält
                sendError(not_alphanumeric, cfd);
            } else {
                if (delete(key) == -1) { //Key wird aus der Hashmap gelöscht
                    value = "key_nonexistent";
                } else {
                    value = "key_deleted";
                }
                char *out = getoutputString(pfx, key, value);
                write(cfd, out, strlen(out) + 1);
            }
        }
    } else if (strcmp(comm, "QUIT") == 0) { // Befehl QUIT
        if (value != NULL || key != NULL) { //Überprüfen, dass kein Value angegeben wurde
            sendError(too_many_arguments, cfd);
        } else {
            return 1;
        }
    } else { // Befehl unbekannt
        sendError(unknown_command, cfd);
    }
    return 0;
}

int check_key(char *key) {
    for (int i = 0; i < strlen(key); i++) {
        if (!isalnum(key[i])) { //isalnum prüft, ob Zeichen alphanumerisch ist
            return -1;
        }
    }
    return 0;
}

int check_value(char *value) {
    for (int i = 0; i < strlen(value); i++) {
        if (!isalnum(value[i])) { //isalnum prüft, ob Zeichen alphanumerisch ist
            if (value[i] == ' ') {
                return 0;
            }
            return -1;
        }
    }
    return 0;
}

char *getoutputString(char *pfx, char *key, char *value) {
    char *out = malloc(strlen(pfx) + strlen(key) + strlen(value) + 7); // 7 = 2x ":" + " " + "\r\n" + ">" + "\0"
    strcpy(out, "> ");
    strcat(out, pfx);
    strcat(out, ":");
    strcat(out, key);
    strcat(out, ":");
    strcat(out, value);
    strcat(out, "\r\n");
    return out;
}

long sendError(enum error_codes err_code, int cfd) {
    switch (err_code) {
        case 1:
            return write(cfd, "> not_alphanumeric\r\n", 20);
        case 2:
            return write(cfd, "> command_nonexistent\r\n", 23);
        case 3:
            return write(cfd, "> too_many_arguments\r\n", 22);
        default:
            return write(cfd, "> unknown_error\r\n", 17);
    }
    return 0;
}