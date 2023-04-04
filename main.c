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

//Gibt 0 zurück, wenn keine alphanumerischen Zeichen im String sind, sonst -1
int check_key(char *key);

//Ausgabe im Format "> pfx:key:value\r\n"
char* getoutputString(char* pfx, char* key, char* value);

/**
 * Sendet einen Fehler an den Client
 * @param err_code Fehlercode (1 = not_alphanumeric, 2 = unknown_command)
 * @param cfd Client-File-Descriptor
 * @return  Anzahl der gesendeten Bytes oder -1 bei Fehler
 */
long sendError(int err_code, int cfd);

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

        // Zurückschicken der Daten, solange der Client welche schickt (und kein Fehler passiert)
        while (bytes_read > 0) {
            printf("%d bytes received from %s:%d\n", bytes_read, inet_ntoa(client.sin_addr), ntohs(client.sin_port));

            //TODO: Besseres Fehlerhandling einbauen

            // Überprüfen, ob es sich bei dem Befehl um einen PUT, GET oder DEL handelt
            if (strncmp(in, "PUT", 3) == 0) { // Befehl PUT
                char *pfx = strtok(in, " ");
                char *key = strtok(NULL, " ");
                char *value = strtok(NULL, "\r\n"); //Teilstring bis zum Zeilenumbruch
                if (check_key(key) != 0 || check_key(value) != 0) { //Überprüfen, dass Key nur alphanumerische Zeichen und keine Leerzeichen enthält
                    sendError(1, cfd);
                } else {
                    if (contains(key) == 0) { //Überprüfen, ob Key bereits vorhanden ist
                        value = "key_already_exists";
                    } else {
                        put(key, value); //Key und Value werden in die Hashmap gespeichert
                    }
                    char *out = getoutputString(pfx, key, value);
                    write(cfd, out, strlen(out) + 1);
                }
            } else if (strncmp(in, "GET", 3) == 0) { // Befehl GET
                char *pfx = strtok(in, " ");
                char *key = strtok(NULL, "\r\n"); //Teilstring bis zum Zeilenumbruch
                char *value = get(key); //Wert wird aus der Hashmap geholt
                if (check_key(key) != 0) { //Überprüfen, dass Key nur alphanumerische Zeichen und keine Leerzeichen enthält
                    sendError(1, cfd);
                } else {
                    if (value == NULL) { //Wenn der Key nicht existiert
                        value = "key_nonexistent";
                    }
                    char *out = getoutputString(pfx, key, value);
                    write(cfd, out, strlen(out) + 1);
                }
            } else if (strncmp(in, "DEL", 3) == 0) {
                char *pfx = strtok(in, " ");
                char *key = strtok(NULL, "\r\n"); //Teilstring bis zum Zeilenumbruch
                if (check_key(key) != 0) { //Überprüfen, dass Key nur alphanumerische Zeichen und keine Leerzeichen enthält
                    sendError(1, cfd);
                } else {
                    delete(key); //Key wird aus der Hashmap gelöscht
                    char *out = getoutputString(pfx, key, "key_deleted");
                    write(cfd, out, strlen(out) + 1);
                }
            } else if (strncmp(in, "QUIT", 4) == 0) {
                break;
            } else {
                sendError(2, cfd);
            }
            bytes_read = read(cfd, in, BUFSIZE);
        }
        close(cfd);
    }

    // Rendezvous Descriptor schließen
    close(rfd);

}

int check_key(char *key) {
    for (int i = 0; i < strlen(key); i++) {
        if (!isalnum(key[i])) { //isalnum prüft, ob Zeichen alphanumerisch ist
            return -1;
        }
    }
    return 0;
}

char *getoutputString(char *pfx, char *key, char *value) {
    char* out = malloc(strlen(pfx) + strlen(key) + strlen(value) + 7); // 7 = 2x ":" + " " + "\r\n" + ">" + "\0"
    strcpy(out, "> ");
    strcat(out, pfx);
    strcat(out, ":");
    strcat(out, key);
    strcat(out, ":");
    strcat(out, value);
    strcat(out, "\r\n");
    return out;
}

long sendError(int err_code, int cfd) {
    switch (err_code) {
        case 1:
            return write(cfd,"> not_alphanumeric\r\n", 20);
        case 2:
            return write(cfd,"> unknown_command\r\n", 20);
        default:
            return write(cfd,"> unknown_error\r\n", 20);
    }
    return 0;
}