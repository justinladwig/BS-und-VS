/*******************************************************************************
 
  Server für Key-Value-Store
 
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
#include <sys/shm.h>
#include <sys/wait.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/errno.h>
#include "keyValStore.h"
#include "sub.h"
#include "process_list.h"
#include "RestApi.h"

#define TRUE 1
#define BUFSIZE 1024 // Größe des Buffers
#define PORT 5678 // Port, auf dem der Server lauscht

/*******************************************************************************
 *
 * Funktionsdeklerationen
 *
 *******************************************************************************/

//Gibt 0 zurück, wenn keine alphanumerischen Zeichen oder Leerzeichen im String sind, sonst -1
int check_value(char *value);

//Ausgabe im Format "> pfx:key:value\r\n"
char *getoutputString(char *pfx, char *key, char *value);

//Aufzählung von Fehlercodes
enum error_codes {
    not_alphanumeric = 1,
    unknown_command = 2,
    too_many_arguments = 3,
    too_few_arguments = 4,
    no_more_memory_available = 5,
    no_transaction_to_end = 6,
    transaction_already_started = 7,
    unknown_error = 5
};

//Status, ob eine Transaktion ausgeführt wird
enum transaction_states {
    NOT_ACTIVE = 0,
    ACTIVE = 1
};

/**
 * Sendet einen Fehler an den Client
 * @param err_code Fehlercode aus enum error_codes
 * @param cfd Client-File-Descriptor
 * @return  Anzahl der gesendeten Bytes oder -1 bei Fehler
 */
long sendError(enum error_codes err_code, int cfd);

//Auswertung der Kommandos. Gibt die Anzahl der gesendeten Bytes zurück oder -2 bei QUIT oder -1 bei Fehler
long commandInterpreter(char *comm, int cfd);

//Funktion zum Senden von Subscription-Messages an alle Clients die den Key abonniert haben
void sendSubMessage(char *mtext, char *key);

/*******************************************************************************
 *
 *******************************************************************************/

int rfd; // Rendevouz-Descriptor
int msgqueue;
int transsemid;
struct sembuf enter, leave;
enum transaction_states transaction_state = NOT_ACTIVE;
RestApi *api;
pid_t socketChildPID = 0;
pid_t subpid = 0;
pid_t restPid = 0;

struct subscription_msg {
    long mtype;
    char mtext[KEYSIZE + VALUESIZE + 30];
};


void sigSystemHandler(int sig_num){
    fprintf(stderr, "Signal %d recieved.\n", sig_num);

    // Kindprozesse beenden
    if( terminate_all_processes() == -1 ){
        fprintf(stderr, "Error terminating child process: %s\n", strerror(errno));
    } else {
        fprintf(stderr, "All child processes terminated.\n");
    }

    // Shared memory freigeben
    if( deinitKeyValStore() == -1 ){
        fprintf(stderr, "Error releasing KeyVal: %s\n", strerror(errno));
    } else {
        fprintf(stderr, "KeyVal released.\n");
    }

    if( deinitSubStore() == -1 ){
        fprintf(stderr, "Error releasing Substore: %s\n", strerror(errno));
    } else {
        fprintf(stderr, "Substore released.\n");
    }

    // Freigabe Semaphore
    if( semctl(transsemid, 0, IPC_RMID, 0) == -1 ) {
        fprintf(stderr, "Error releasing Transaction Semaphore: %s\n", strerror(errno));
    } else {
        fprintf(stderr, "Transaction Semaphore released.\n");
    }

    // Freigabe Message Queue
    if( msgctl(msgqueue, IPC_RMID, 0) == -1 ){
        fprintf(stderr, "Error releasing Message Queue: %s\n", strerror(errno));
    } else {
        fprintf(stderr, "Message Queue released.\n");
    }

    // Schließen des Sockets
    if(close(rfd) == -1){
        fprintf(stderr, "Error closing socket: %s\n", strerror(errno));
    }
    // Beenden der API
    kill(restPid, SIGTERM);
    // Beenden des Programms
    exit(0);
}

void sigSIGCHLDHandler(int sig_num) {
    pid_t pid;
    int status;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        remove_process(pid); // Prozess aus der Liste entfernen (bei SubChild nicht relevant)
        if (WIFEXITED(status)) {
            printf("Kindprozess mit PID %d beendet mit Status %d\n", pid, WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            printf("Kindprozess mit PID %d wurde durch Signal %d beendet\n", pid, WTERMSIG(status));
        }
    }
}

void sigSubChildTerminateHandler (int sig_num){
    if (kill(subpid, SIGTERM) == -1) {
        fprintf(stderr, "Error killing child process: %s\n", strerror(errno));
        exit(1);
    }
    exit(0);
}

void sigRestHandler(int sig_num){
    destroy(api);
}

int main() {
    signal(SIGINT, sigSystemHandler); // Wird aufgerufen, wenn SIGINT empfangen wird (z.B. durch Strg+C)
    signal(SIGQUIT, sigSystemHandler); // Wird aufgerufen, wenn SIGQUIT empfangen wird (z.B. durch Strg+\)
    signal(SIGTERM, sigSystemHandler); // Wird aufgerufen, wenn SIGTERM empfangen wird (z.B. durch kill)
    signal(SIGCHLD, sigSIGCHLDHandler); // Wird aufgerufen, wenn ein Kindprozess beendet wird (z.B. durch exit()) und verhindert, dass der Prozess als Zombie-Prozess in der Prozessliste verbleibt
    printf("Programm wurde gestartet. (PID: %d)\n", getpid());

    //Prozessübergreiifende Initialisierung


    //Key-Value-Store initialisieren inkl. Shared Memory und Semaphore
    int init = initKeyValStore();
    if (init == -1) {
        printf("Fehler beim Initialisieren des Shared Memorys.\n");
        exit(-1);
    }
    //SubPub-Store initialisieren inkl. Shared Memory und Semaphore
    int subinit = initSubStore();
    if (subinit == -1) {
        printf("Fehler beim Initialisieren des Shared Memorys.\n");
        exit(-1);
    }

    //Semaphoren für Transaktionen initialisieren
    transsemid = semget(IPC_PRIVATE, 1, IPC_CREAT | 0644);
    if (transsemid == -1) {
        return -1;
    }
    semctl(transsemid, 0, SETVAL, 1); //Semaphore auf 1 setzen
    enter.sem_num = leave.sem_num = 0; //Semaphore 0 verwenden
    enter.sem_flg = leave.sem_flg = SEM_UNDO; //Semaphore wieder freigeben, wenn Prozess beendet wird
    enter.sem_op = -1; //Semaphore um 1 verringern
    leave.sem_op = 1; //Semaphore um 1 erhöhen

    //Message Queue initialisieren für Abbonements
    msgqueue = msgget(IPC_PRIVATE, IPC_CREAT | 0644);
    if (msgqueue == -1) {
        return -1;
    }

    printf("Allgemeine Initialisierung abgeschlossen.\n");

    //REST API starten
    if ( (restPid = fork()) == 0) {
        signal(SIGTERM, sigRestHandler);
        signal(SIGINT, SIG_IGN);

        //REST API initialisieren
        api = create();

        //REST API starten
        run(api);
        return(0);
    }

    //Initialisierung des Socket-Servers

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


    // Socket Optionen setzen für den Fall, dass der Server nach einem Abbruch sofort wieder gestartet werden kann ohne, dass der Port noch freigegeben werden muss
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

    while (TRUE) {

        // Verbindung eines Clients wird entgegengenommen
        cfd = accept(rfd, (struct sockaddr *) &client, &client_len);
        if (cfd < 0) {
            fprintf(stderr, "FEHLER: Verbindung konnte nicht akzeptiert werden\n");
            exit(-1);
        }

        printf("Verbindung von %s:%d\n", inet_ntoa(client.sin_addr), ntohs(client.sin_port));

        pid_t pid = fork(); //Multiclientfähigkeit mithilfe von Elternprozess und Kindprozessen
        if (pid > 0) {
            // Elternprozess
            printf("Kindprozess mit PID %d wurde erzeugt.\n", pid);
            add_process(pid); // Kindprozess zur Liste der Kindprozesse hinzufügen
            close(cfd);
            continue;
        } else if (pid == 0) {
            // Kindprozess
//            signal(SIGCHLD, SIG_IGN);
            signal(SIGTERM, sigSubChildTerminateHandler);
            signal(SIGINT, SIG_IGN);
            socketChildPID = getpid();

            //Subscription-Lauscher Kindprozess erstellen
            subpid = fork();
            if (subpid == 0) {
                //Kindprozess
                signal(SIGTERM, SIG_DFL);
                struct subscription_msg msg;
                while(1) {
                    msgrcv(msgqueue, &msg , sizeof(msg.mtext), socketChildPID, MSG_NOERROR);
                    send(cfd, msg.mtext, strlen(msg.mtext), 0);
                    printf("Subscription Update erhalten: %s gesendet", msg.mtext);
                }
            } else if (subpid > 0) {
                //Elternprozess
                printf("Kindprozess (Sub) mit PID %d wurde erzeugt\n", subpid);

                printf("Warten auf Kommandos von %s:%d..., QUIT beendet die Verbindung!\n", inet_ntoa(client.sin_addr), ntohs(client.sin_port));

                // Lesen von Daten, die der Client schickt
                bytes_read = read(cfd, in, BUFSIZE);

                // Interpretieren von Daten, die der Client schickt
                while (bytes_read > 0) {
                    //Manuelle Nullterminierung des Strings
                    char comm[BUFSIZE + 1]; // +1 für Nullterminierung
                    strncpy(comm, in, bytes_read);
                    comm[bytes_read] = '\0';

                    printf("%ld bytes received from %s:%d\n", bytes_read, inet_ntoa(client.sin_addr), ntohs(client.sin_port));

                    //Auswertung der Kommandos
                    bytes_sent = commandInterpreter(comm, cfd);
                    if (bytes_sent == -2) break; //Wenn der Client QUIT geschickt hat, wird die Verbindung beendet

                    printf("%ld bytes sent to %s:%d, waiting for next command...\n", bytes_sent, inet_ntoa(client.sin_addr), ntohs(client.sin_port));

                    bytes_read = read(cfd, in, BUFSIZE); //Lesen von Daten, die der Client schickt
                }
                close(cfd);
                printf("Client %s:%d disconnected.\n", inet_ntoa(client.sin_addr), ntohs(client.sin_port));

                sigSubChildTerminateHandler(SIGTERM); //Kindprozess beenden
                printf("Subscriber Prozess %i beendet!\n", subpid);
                break;
            } else {
                // Fehler
                fprintf(stderr, "FEHLER: Subscriberprozess konnte nicht erzeugt werden\n");
                exit(-1);
            }
        } else {
            // Fehler
            fprintf(stderr, "FEHLER: Kindprozess konnte nicht erzeugt werden\n");
            exit(-1);
        }
    }
    printf("Prozess %d wird beendet.\n", getpid());
    return(0);
}

long commandInterpreter(char *comm, int cfd) {
    long bytes_sent = 0; // Anzahl der Bytes, die der Server an den Client geschickt hat

    // Befehl in einzelne Teile zerlegen
    char delimiter[] = " \r\n"; //Telnet schickt \r\n als Zeilenumbruch
    char *pfx = strtok(comm, delimiter);
    char *key = strtok(NULL, delimiter);
    char *value = strtok(NULL, "\r\n");

    if (pfx == NULL) {
        return bytes_sent;
    }

    //TODO: Schöner machen!? Wie?
    if (transaction_state == NOT_ACTIVE) {
        while (semctl(transsemid, 0, GETVAL) == 0) {
            sleep(1);
        }
    }
    // Überprüfen, ob es sich bei dem Befehl um einen PUT, GET oder DEL handelt
    if (strcmp(pfx, "PUT") == 0) { // Befehl PUT
        if (key == NULL || value == NULL) { //Überprüfen, ob Key und Value angegeben wurden
            printf("PUT: Zu wenig Argumente angegeben.\n");
            bytes_sent = sendError(too_few_arguments, cfd);
        } else {
            if (check_value(key) != 0 || check_value(value) != 0) { //Überprüfen, dass Key nur alphanumerische Zeichen und keine Leerzeichen enthält und Value nur alphanumerische Zeichen enthält
                printf("PUT: Key oder Value enthält nicht alphanumerische Zeichen.\n");
                bytes_sent = sendError(not_alphanumeric, cfd);
            } else {
                if (contains(key) == 0) { //Überprüfen, ob Key bereits vorhanden ist, dann soll der neue Wert in der Hashmap gespeichert werden und der alte Wert zurückgegeben werden
                    char *OldGet = get(key);
                    char *old_ = malloc(strlen(OldGet) + 1);
                    strcpy(old_,OldGet); //Alten Wert in einen neuen String kopieren, da dieser sonst überschrieben wird
                    change(key, value); //Value wird geändert
                    printf("PUT: Key %s wurde geändert. Alter Wert: %s, neuer Wert: %s\n", key, old_, value);
                    char *out = getoutputString(pfx, key, old_);
                    bytes_sent = write(cfd, out, strlen(out));
                    sendSubMessage(getoutputString(pfx, key, value), key); //Subscription-Message wird gesendet
                } else {
                    if (put(key, value) == -1) { //Key und Value werden in die Hashmap gespeichert
                        printf("PUT: Key %s konnte nicht hinzugefügt werden: Kein Speicherplatz verfügbar\n", key);
                        bytes_sent = sendError(no_more_memory_available, cfd);
                    } else {
                        printf("PUT: Key %s wurde hinzugefügt. Wert: %s\n", key, value);
                        value = get(key); //Value wird erneut aus der Hashmap geholt, um den tatsächlichen Wert zu erhalten (kann gekürzt werden)
                        char *out = getoutputString(pfx, key, value);
                        bytes_sent = write(cfd, out, strlen(out));
                    }
                }
            }
        }
    } else if (strcmp(pfx, "GET") == 0) { // Befehl GET
        if (value != NULL) { //Überprüfen, dass kein Value angegeben wurde
            printf("GET: Zu viele Argumente angegeben\n");
            bytes_sent = sendError(too_many_arguments, cfd);
        } else if (key == NULL) { //Überprüfen, dass ein Key angegeben wurde
            printf("GET: Zu wenige Argumente angegeben\n");
            bytes_sent = sendError(too_few_arguments, cfd);
        } else {
            if (check_value(key) != 0) { //Überprüfen, dass Key nur alphanumerische Zeichen und keine Leerzeichen enthält
                printf("GET: Key enthält nicht alphanumerische Zeichen\n");
                bytes_sent = sendError(not_alphanumeric, cfd);
            } else {
                value = get(key); //Wert wird aus der Hashmap geholt
                if (value == NULL) { //Wenn der Key nicht existiert
                    printf("GET: Key existiert nicht\n");
                    value = "key_nonexistent";
                }
                char *out = getoutputString(pfx, key, value);
                printf("GET: Key %s, Wert: %s\n", key, value);
                bytes_sent = write(cfd, out, strlen(out));
            }
        }
    } else if (strcmp(pfx, "DEL") == 0) { // Befehl DEL
        if (value != NULL) { //Überprüfen, dass kein Value angegeben wurde
            printf("DEL: Zu viele Argumente angegeben\n");
            bytes_sent = sendError(too_many_arguments, cfd);
        } else if (key == NULL) { //Überprüfen, dass ein Key angegeben wurde
            printf("DEL: Zu wenige Argumente angegeben\n");
            bytes_sent = sendError(too_few_arguments, cfd);
        } else {
            if (check_value(key) != 0) { //Überprüfen, dass Key nur alphanumerische Zeichen und keine Leerzeichen enthält
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
                bytes_sent = write(cfd, out, strlen(out));
                sendSubMessage(out, key); //Subscription-Message wird gesendet
                subClearKey(key);
            }
        }
    } else if (strcmp(pfx, "BEG") == 0) { // Befehl BEG; Beginn einer Transaktion
        if (value != NULL || key != NULL) { //Überprüfen, dass kein Value angegeben wurde
            printf("BEG: Zu viele Argumente angegeben\n");
            bytes_sent = sendError(too_many_arguments, cfd);
        } else if (transaction_state == ACTIVE) {
            printf("BEG: Transaktion bereits gestartet");
            bytes_sent = sendError(transaction_already_started, cfd);
        } else {
            printf("BEG: Transaktion beginnt\n");
            semop(transsemid, &enter, 1);
            transaction_state = ACTIVE;
            char *out = "transaction_begins\r\n";
            bytes_sent = write(cfd, out, strlen(out));
        }
    } else if (strcmp(pfx, "END") == 0) { // Befehl END; Beenden einer Transaktion
        if (value != NULL || key != NULL) { //Überprüfen, dass kein Value angegeben wurde
            printf("END: Zu viele Argumente angegeben\n");
            bytes_sent = sendError(too_many_arguments, cfd);
        } else if (transaction_state == NOT_ACTIVE) {
            printf("END: Transaktion nicht gestartet!\n");
            bytes_sent = sendError(no_transaction_to_end, cfd);
        } else {
            printf("END: Transaktion wird beendet\n");
            semop(transsemid, &leave, 1);
            transaction_state = NOT_ACTIVE;
            char *out = "transaction_ends\r\n";
            bytes_sent = write(cfd, out, strlen(out));
        }
    } else if (strcmp(pfx, "QUIT") == 0) { // Befehl QUIT; Schließen der Verbindung
        if (value != NULL || key != NULL) { //Überprüfen, dass kein Value angegeben wurde
            printf("QUIT: Zu viele Argumente angegeben\n");
            bytes_sent = sendError(too_many_arguments, cfd);
        } else {
            printf("QUIT: Verbindung wird beendet\n");
            return -2;
        }
    } else if (strcmp(pfx, "SUB") == 0) { // Befehl SUB; Subscription für einen Schlüssel gestartet
        if (value != NULL) { //Überprüfen, dass kein Value angegeben wurde
            printf("SUB: Zu viele Argumente angegeben\n");
            bytes_sent = sendError(too_many_arguments, cfd);
        } else if (key == NULL) { //Überprüfen, dass ein Key angegeben wurde
            printf("SUB: Zu wenige Argumente angegeben\n");
            bytes_sent = sendError(too_few_arguments, cfd);
        } else {
            if (check_value(key) != 0) { //Überprüfen, dass Key nur alphanumerische Zeichen und keine Leerzeichen enthält
                printf("SUB: Key enthält nicht alphanumerische Zeichen\n");
                bytes_sent = sendError(not_alphanumeric, cfd);
            } else {
                value = get(key);
                if (value == NULL) { //Key wird mit PID zu der SUB-Hashmap hinzugefügt;
                    printf("SUB: Key nicht vorhanden\n");
                    value = "key_nonexistent";
                } else if (subContains(key, socketChildPID) == 1) {
                    printf("SUB: Key %s und PID %i bereits Abboniert\n", key, socketChildPID);
                } else {
                    printf("SUB: Key %s, PID: %i\n", key, socketChildPID);
                    subPut(key, socketChildPID);
                }
                char *out = getoutputString(pfx, key, value);
                bytes_sent = write(cfd, out, strlen(out));
            }
        }
    } else { // Befehl unbekannt
        printf("Kommando nicht vorhanden: %s %s %s\n", pfx, key, value);
        bytes_sent = sendError(unknown_command, cfd);
    }
    return bytes_sent;
}

int check_value(char *value) {
    for (int i = 0; i < strlen(value); i++) {
        if (!isalnum(value[i])) { //isalnum prüft, ob Zeichen alphanumerisch ist
            if (value[i] == ' ') {
                continue;
            }
            return -1;
        }
    }
    return 0;
}

char *getoutputString(char *pfx, char *key, char *value) {
    char *out = malloc(strlen(pfx) + strlen(key) + strlen(value) + 5); // 7 = 2x ":" + " " + "\r\n" + ">" + "\0"
    strcpy(out, pfx);
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
        case 5:
            return write(cfd, "no_more_memory_available\r\n", 26);
        case 6:
            return write(cfd, "no-transaction_to_end\r\n", 25);
        case 7:
            return write(cfd, "transaction_already_started\r\n", 29);
        default:
            return write(cfd, "unknown_error\r\n", 15);
    }
}

void sendSubMessage(char *mtext, char *key) {
    struct subscription_msg msg;
    strcpy(msg.mtext, mtext);
    pid_t pidarr[100];
    int size = subGet(key, pidarr, 100);
    for (int i = 0; i < size; i++) {
        if (pidarr[i] != socketChildPID) {
            msg.mtype = pidarr[i];
            if (msgsnd(msgqueue, &msg, sizeof(msg.mtext), 0) == -1) {
                perror("msgsnd");
            }
        }
    }
}
