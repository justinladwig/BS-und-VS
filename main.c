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
#include <signal.h>
#include "keyValStore.h"
#include "sub.h"

#define BUFSIZE 1024 // Größe des Buffers
#define ENDLOSSCHLEIFE 1
#define PORT 5678

/*******************************************************************************
 *
 * Funktionsdeklerationen
 *
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
    too_few_arguments = 4,
    unknown_error = 5
};

/**
 * Sendet einen Fehler an den Client
 * @param err_code Fehlercode aus enum error_codes
 * @param cfd Client-File-Descriptor
 * @return  Anzahl der gesendeten Bytes oder -1 bei Fehler
 */
long sendError(enum error_codes err_code, int cfd);

//Auswertung der Kommandos. Gibt die Anzahl der gesendeten Bytes zurück oder -1 bei Fehler
long commandInterpreter(char *comm, int cfd);

/*******************************************************************************
 *
 *******************************************************************************/

// Signal Handler für SIGINT
void sigintHandler(int sig_num) {
    signal(SIGINT, sigintHandler);
    //TODO: Shared Memory und Semaphore freigeben
    printf("SIGINT received. Release Shared Mem. and Semaphore.\n");
    exit(0);
}

int main() {
    signal(SIGINT, sigintHandler); // Wird aufgerufen, wenn SIGINT empfangen wird (z.B. durch Strg+C)

    printf("Programm wurde gestartet. (PID: %d)\n", getpid());

    //Prozessübergreiifende Initialisierung

    //TODO: Shared Memory und Semaphore initialisieren
    //initCommon();

    printf("Allgemeine Initialisierung abgeschlossen.\n");

    //Initialisierung des Socket-Servers

    int rfd; // Rendevouz-Descriptor
    int cfd; // Verbindungs-Descriptor

    struct sockaddr_in client; // Socketadresse eines Clients
    socklen_t client_len = sizeof(client); // Länge der Client-Daten
    char in[BUFSIZE]; // Daten vom Client an den Server
    long bytes_read; // Anzahl der Bytes, die der Client geschickt hat
    long bytes_sent; // Anzahl der Bytes, die der Server geschickt hat


    // Socket erstellen
    rfd = socket(AF_INET, SOCK_STREAM, 0);
    if (rfd < 0) {
        fprintf(stderr, "FEHLER: Socket konnte nicht erstellt werden\n");
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
        fprintf(stderr, "FEHLER: Socket konnte nicht gebunden werden\n");
        exit(-1);
    }


    // Socket lauschen lassen
    int lrt = listen(rfd, 5);
    if (lrt < 0) {
        fprintf(stderr, "FEHLER: Socket konnte nicht listen gesetzt werden\n");
        exit(-1);
    }

    printf("Socket-Server läuft auf %s:%d und wartet auf Verbindungen...\n", inet_ntoa(server.sin_addr), ntohs(server.sin_port));
    if (server.sin_addr.s_addr == INADDR_ANY) printf("HINWEIS: 0.0.0.0 bedeutet, dass der Server auf allen Netzwerk-Interfaces lauscht.\n");

    while (ENDLOSSCHLEIFE) {

        // Verbindung eines Clients wird entgegengenommen
        cfd = accept(rfd, (struct sockaddr *) &client, &client_len);
        if (cfd < 0) {
            fprintf(stderr, "FEHLER: Verbindung konnte nicht akzeptiert werden\n");
            exit(-1);
        }

        printf("Verbindung von %s:%d\n", inet_ntoa(client.sin_addr), ntohs(client.sin_port));
        printf("Warten auf Kommandos von %s:%d..., QUIT beendet die Verbindung!\n", inet_ntoa(client.sin_addr), ntohs(client.sin_port));

        // Lesen von Daten, die der Client schickt
        bytes_read = read(cfd, in, BUFSIZE);

        // Interpretieren von Daten, die der Client schickt
        while (bytes_read > 0) {
            char *comm = malloc(BUFSIZE + 1);
            strncpy(comm, in, bytes_read);
            comm[bytes_read] = '\0';

            printf("%ld bytes received from %s:%d\n", bytes_read, inet_ntoa(client.sin_addr), ntohs(client.sin_port));

            //Auswertung der Kommandos
            bytes_sent = commandInterpreter(comm, cfd);
            if (bytes_sent == 0) break; //Wenn der Client QUIT geschickt hat, wird die Verbindung beendet

            printf("%ld bytes sent to %s:%d, waiting for next command...\n", bytes_sent, inet_ntoa(client.sin_addr), ntohs(client.sin_port));

            bytes_read = read(cfd, in, BUFSIZE); //Lesen von Daten, die der Client schickt
        }
        printf("Client %s:%d disconnected.\n", inet_ntoa(client.sin_addr), ntohs(client.sin_port));
        close(cfd);
    }

    // Rendezvous Descriptor schließen
    close(rfd);

}

long commandInterpreter(char *comm, int cfd) {
    long bytes_sent; // Anzahl der Bytes, die der Server an den Client geschickt hat

    // Befehl in einzelne Teile zerlegen
    char delimiter[] = " \r\n"; //Telnet schickt \r\n als Zeilenumbruch
    char *pfx = strtok(comm, delimiter);
    char *key = strtok(NULL, delimiter);
    char *value = strtok(NULL, "\r\n");

    // Überprüfen, ob es sich bei dem Befehl um einen PUT, GET oder DEL handelt
    if (strcmp(comm, "PUT") == 0) { // Befehl PUT
        if (key == NULL || value == NULL) { //Überprüfen, ob Key und Value angegeben wurden
            printf("PUT: Zu wenig Argumente angegeben.\n");
            bytes_sent = sendError(too_few_arguments, cfd);
        } else {
            if (check_key(key) != 0 || check_value(value) !=0) { //Überprüfen, dass Key nur alphanumerische Zeichen und keine Leerzeichen enthält und Value nur alphanumerische Zeichen enthält
                printf("PUT: Key oder Value enthält nicht alphanumerische Zeichen.\n");
                bytes_sent = sendError(not_alphanumeric, cfd);
            } else {
                if (contains(key) == 0) { //Überprüfen, ob Key bereits vorhanden ist, dann soll der neue Wert in der Hashmap gespeichert werden und der alte Wert zurückgegeben werden
                    char *OldGet = get(key);
                    char *old_ = malloc(strlen(OldGet) + 1);
                    strcpy(old_,OldGet); //Alten Wert in einen neuen String kopieren, da dieser sonst überschrieben wird
                    change(key, value); //Value wird geändert
                    printf("PUT: Key %s wurde geändert. Alter Wert: %s, neuer Wert: %s\n", key, old_, value);
                    value = old_;
                } else {
                    printf("PUT: Key %s wurde hinzugefügt. Wert: %s\n", key, value);
                    put(key, value); //Key und Value werden in die Hashmap gespeichert
                }
                char *out = getoutputString(pfx, key, value);
                bytes_sent = write(cfd, out, strlen(out) + 1);
            }
        }
    } else if (strcmp(comm, "GET") == 0) { // Befehl GET
        if (value != NULL) { //Überprüfen, dass kein Value angegeben wurde
            printf("GET: Zu viele Argumente angegeben\n");
            bytes_sent = sendError(too_many_arguments, cfd);
        } else if (key == NULL) { //Überprüfen, dass ein Key angegeben wurde
            printf("GET: Zu wenige Argumente angegeben\n");
            bytes_sent = sendError(too_few_arguments, cfd);
        } else {
            value = get(key); //Wert wird aus der Hashmap geholt
            if (check_key(key) !=0) { //Überprüfen, dass Key nur alphanumerische Zeichen und keine Leerzeichen enthält
                printf("GET: Key enthält nicht alphanumerische Zeichen\n");
                bytes_sent = sendError(not_alphanumeric, cfd);
            } else {
                if (value == NULL) { //Wenn der Key nicht existiert
                    printf("GET: Key existiert nicht\n");
                    value = "key_nonexistent";
                }
                char *out = getoutputString(pfx, key, value);
                printf("GET: Key %s, Wert: %s\n", key, value);
                bytes_sent = write(cfd, out, strlen(out) + 1);
            }
        }
    } else if (strcmp(comm, "DEL") == 0) { // Befehl DEL
        if (value != NULL) { //Überprüfen, dass kein Value angegeben wurde
            printf("DEL: Zu viele Argumente angegeben\n");
            bytes_sent = sendError(too_many_arguments, cfd);
        } else if (key == NULL) { //Überprüfen, dass ein Key angegeben wurde
            printf("DEL: Zu wenige Argumente angegeben\n");
            bytes_sent = sendError(too_few_arguments, cfd);
        } else {
            if (check_key(key) !=0) { //Überprüfen, dass Key nur alphanumerische Zeichen und keine Leerzeichen enthält
                printf("DEL: Key enthält nicht alphanumerische Zeichen\n");
                bytes_sent = sendError(not_alphanumeric, cfd);
            } else {
                if (delete(key) == -1) { //Key wird aus der Hashmap gelöscht;
                    printf("DEL: Key nicht vorhanden\n");
                    value = "key_nonexistent";
                } else {
                    printf("DEL: Key %s wurde gelöscht\n", key);
                    value = "key_deleted";
                }
                char *out = getoutputString(pfx, key, value);
                printf("DEL: Key %s, Wert: %s\n", key, value);
                bytes_sent = write(cfd, out, strlen(out) + 1);
            }
        }
    } else if (strcmp(comm, "QUIT") == 0) { // Befehl QUIT
        if (value != NULL || key != NULL) { //Überprüfen, dass kein Value angegeben wurde
            printf("QUIT: Zu viele Argumente angegeben\n");
            bytes_sent = sendError(too_many_arguments, cfd);
        } else {
            printf("QUIT: Verbindung wird beendet\n");
            return 0;
        }
    } else { // Befehl unbekannt
        printf("Kommando nicht vorhanden\n");
        bytes_sent = sendError(unknown_command, cfd);
    }
    return bytes_sent;
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
    char *out = malloc(strlen(pfx) + strlen(key) + strlen(value) + 5); // 7 = 2x ":" + " " + "\r\n" + ">" + "\0"
//    strcpy(out, "> ");
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
            return write(cfd, "not_alphanumeric\r\n", 18);
        case 2:
            return write(cfd, "command_nonexistent\r\n", 21);
        case 3:
            return write(cfd, "too_many_arguments\r\n", 20);
        case 4:
            return write(cfd, "too_few_arguments\r\n", 19);
        default:
            return write(cfd, "unknown_error\r\n", 15);
    }
    return -1;
}