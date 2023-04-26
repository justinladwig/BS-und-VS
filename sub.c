/*
 * 1. Standard Variable = aus
 * 2. Mit einer Variable kann sich ein Client für den Pub/Sub "anmelden" (Variable gesetzt)
 * 3. Bei Änderungen wird mit IF abgefragt, ob man eine Nachricht bekommen möchte
 * 4. Nachricht wird ausgegeben
 * 5. In diesem Prozess werden die anderen Clients nicht blockiert, sie bekommen nur ne Nachricht
 */

#include <sys/shm.h>
#include <sys/sem.h>
#include <stdlib.h>
#include <string.h>
#include "sub.h"

#define TRUE 1
#define MAX_SUBSCRIBERS 10

int subshmid = -1;
int subsemid = -1;

struct sembuf enter, leave;
struct subscription *subscription_store;

struct subscription{
    char key[KEYSIZE];
    pid_t processid;
    int nextIndex;
};

//Shared Memory mit 500 Elementen initialisieren
void subinitarray() {
    for (int i = 0; i < STORESIZE; i++) {
        subscription_store[i].key[0] = '\0';
        subscription_store[i].processid = 0;
        subscription_store[i].nextIndex = 0;
    }
}

int initSubStore() {
    //Shared Memory anlegen
    int segsize = sizeof(struct subscription) * STORESIZE;
    subshmid = shmget(IPC_PRIVATE, segsize, IPC_CREAT | 0644);
    if (subshmid == -1) {
        return -1;
    }
    subscription_store = (struct subscription *) shmat(subshmid, 0, 0);
    if (subscription_store == (void *) -1) {
        return -1;
    }

    //Semaphore anlegen
    subsemid = semget(IPC_PRIVATE, 1, IPC_CREAT | 0644);
    if (subsemid == -1) {
        return -1;
    }
    semctl(subsemid, 0, SETVAL, 1); //Semaphore auf 1 setzen
    enter.sem_num = leave.sem_num = 0; //Semaphore 0 verwenden
    enter.sem_flg = leave.sem_flg = SEM_UNDO; //Semaphore wieder freigeben, wenn Prozess beendet wird
    enter.sem_op = -1; //Semaphore um 1 verringern
    leave.sem_op = 1; //Semaphore um 1 erhöhen

    //Array für Sub-Value-Paare initialisieren
    subinitarray();
    return 0;
}

//Löschen des Shared Memory
void deinitSubStore() {
    //Shared Memory löschen
    shmdt(subscription_store);
    shmctl(subshmid, IPC_RMID, 0);

    //Semaphore löschen
    semctl(subsemid, 0, IPC_RMID, 0);
}

//Nächstes freies Element in der Liste finden
int subnextFreeListIndex() {
    for (int i = HASHMAPSIZE; i < STORESIZE; i++) {
        if (subscription_store[i].key[0] == '\0') {
            return i;
        }
    }
    return -1;
}

//Hashcode für einen Key generieren
unsigned long subgenerate_hashcode(char *input) {
    unsigned long hash = 5381;
    int c;

    while ((c = *input++)) hash = ((hash << 5) + hash) + c;

    return hash % HASHMAPSIZE;
}

//Funktion zum Einfügen eines Elements
int subPut(char *key, pid_t value) {
    unsigned long index = subgenerate_hashcode(key); //Hashcode generieren
    semop(subsemid, &enter, 1); //Semaphore sperren
    struct subscription *current = &subscription_store[index]; //Pointer auf Element mit Index des Hashcodes.
    //Überprüft ob Hashtabelle an Index des Hash-Werts leer ist
    if (current->key[0] == '\0') {
        strncpy(current->key, key, KEYSIZE); //Nur Keysize Bytes, da sonst Speicher überschrieben wird. Eingabe wird ggf. abgeschnitten.
        current->processid = value;
        current->nextIndex = 0;
        semop(subsemid, &leave, 1); //Semaphore freigeben
        return 0;
    }
    //Falls Index nicht leer, Verkettung anwenden
    while (current->nextIndex != 0) {
        current = &subscription_store[current->nextIndex];
    }
    current->nextIndex = subnextFreeListIndex();
    if (current->nextIndex == -1) {
        semop(subsemid, &leave, 1); //Semaphore freigeben
        return -1; //Kein freier Speicherplatz mehr
    }
    strncpy(subscription_store[current->nextIndex].key, key, KEYSIZE);
    subscription_store[current->nextIndex].processid = value;
    subscription_store[current->nextIndex].nextIndex = 0;
    semop(subsemid, &leave, 1); //Semaphore freigeben
    return 0;
}

//Funktion zum Auslesen eines Elements
int subGet(char *key, pid_t *pidarr, int size) {
    unsigned int index  = subgenerate_hashcode(key); //Hashcode generieren
    semop(subsemid, &enter, 1); //Semaphore sperren
    struct subscription *current = &subscription_store[index]; //Pointer auf Element mit Index des Hashcodes.

    int count = 0;

    //Falls Index nicht leer, Verkettung anwenden
    while (count < size) {
        if (strcmp(current->key, key) == 0) {
            pidarr[count++] = current->processid; //Element erfolgreich gefunden
        }
        if (current->nextIndex == 0) {
           break;
        }
        current = &subscription_store[current->nextIndex];
    }
    semop(subsemid, &leave, 1); //Semaphore freigeben
    return count;
}

//Funktion zum Löschen eines Elements
int subDelete(char *key, pid_t pid) {
    unsigned int index = subgenerate_hashcode(key); //Hashcode generieren
    semop(subsemid, &enter, 1); //Semaphore sperren
    struct subscription *current = &subscription_store[index]; //Pointer auf Element mit Index des Hashcodes.
    //Überprüft ob Hashtabelle an Index des Hash-Werts leer ist
    if (current->key[0] == '\0') {
        semop(subsemid, &leave, 1); //Semaphore freigeben
        return -1; //Element nicht gefunden
    }
    //Überprüfen, ob erstes Element gelöscht werden soll
    if (strcmp(current->key, key) == 0 && current->processid == pid) {
        if (current->nextIndex == 0) { //Überprüft, ob es das einzige Element ist
            current->key[0] = '\0';
            current->processid = 0;
            current->nextIndex = 0;
            semop(subsemid, &leave, 1); //Semaphore freigeben
            return 0; //Element erfolgreich gelöscht
        } else {
            strncpy(current->key, subscription_store[current->nextIndex].key, KEYSIZE); //Übernimmt den Key des nächsten Elements
            current->processid = subscription_store[current->nextIndex].processid; //Übernimmt den Value des nächsten Elements
            unsigned int temp = current->nextIndex;
            current->nextIndex =subscription_store[current->nextIndex].nextIndex; //Übernimmt den Index des nächsten Elements
            subscription_store[temp].key[0] = '\0'; //Löscht das nächste Element, nachdem es kopiert wurde
            subscription_store[temp].processid = 0;
            subscription_store[temp].nextIndex = 0;
            semop(subsemid, &leave, 1); //Semaphore freigeben
            return 0; //Element erfolgreich gelöscht
        }
    }
    //Falls erstes Element nicht gelöscht werden soll, Verkettung anwenden, um Element zu finden
    while (current->nextIndex != 0) {
        if (strcmp(subscription_store[current->nextIndex].key, key) == 0 && subscription_store[current->nextIndex].processid == pid) {
            unsigned int temp = current->nextIndex;
            if (subscription_store[current->nextIndex].nextIndex == 0) { //Überprüft, ob es das letzte Element ist
                current->nextIndex = 0;
                subscription_store[temp].key[0] = '\0';
                subscription_store[temp].processid = 0;
                subscription_store[temp].nextIndex = 0;
                semop(subsemid, &leave, 1); //Semaphore freigeben
                return 0; //Element erfolgreich gelöscht
            }
            current->nextIndex = subscription_store[current->nextIndex].nextIndex; //Übernimmt den Index des nächsten Elements
            subscription_store[temp].key[0] = '\0'; //Löscht das nächste Element, nachdem der Index kopiert wurde
            subscription_store[temp].processid = 0;
            subscription_store[temp].nextIndex = 0;
            semop(subsemid, &leave, 1); //Semaphore freigeben
            return 0; //Element erfolgreich gelöscht
        }
        current = &subscription_store[current->nextIndex];
    }
    semop(subsemid, &leave, 1); //Semaphore freigeben
    return -1;
}

//TODO: Funktion zum Löschen aller Elemente eines Prozesses, falls ein Prozess beendet wird
int subClearProcess(pid_t pid){
    semop(subsemid, &enter, 1); //Semaphore sperren
    for (int i = 0; i < HASHMAPSIZE; i++) {
        struct subscription *current = &subscription_store[i];
        //Falls leer
        if (current->key[0] == '\0') {
            semop(subsemid, &leave, 1); //Semaphore freigeben
            return -1;
        }
        //Überprüfen, ob erstes Element gelöscht werden soll
        while (current->processid == pid) {
            if (current->nextIndex == 0) { //Überprüft, ob es das einzige Element ist
                current->key[0] = '\0';
                current->processid = 0;
                current->nextIndex = 0;
                break; //Element erfolgreich gelöscht
            } else {
                strncpy(current->key, subscription_store[current->nextIndex].key, KEYSIZE); //Übernimmt den Key des nächsten Elements
                current->processid = subscription_store[current->nextIndex].processid; //Übernimmt den Value des nächsten Elements
                unsigned int temp = current->nextIndex;
                current->nextIndex =subscription_store[current->nextIndex].nextIndex; //Übernimmt den Index des nächsten Elements
                subscription_store[temp].key[0] = '\0'; //Löscht das nächste Element, nachdem es kopiert wurde
                subscription_store[temp].processid = 0;
                subscription_store[temp].nextIndex = 0;
            }
        }
        //Falls erstes Element nicht gelöscht werden soll, Verkettung anwenden, um Element zu finden
        while (current->nextIndex != 0) {
            if (subscription_store[current->nextIndex].processid == pid) {
                unsigned int temp = current->nextIndex;
                if (subscription_store[current->nextIndex].nextIndex == 0) { //Überprüft, ob es das letzte Element ist
                    current->nextIndex = 0;
                    subscription_store[temp].key[0] = '\0';
                    subscription_store[temp].processid = 0;
                    subscription_store[temp].nextIndex = 0;
                    break; //Element erfolgreich gelöscht
                }
                current->nextIndex = subscription_store[current->nextIndex].nextIndex; //Übernimmt den Index des nächsten Elements
                subscription_store[temp].key[0] = '\0'; //Löscht das nächste Element, nachdem der Index kopiert wurde
                subscription_store[temp].processid = 0;
                subscription_store[temp].nextIndex = 0;
            }
            current = &subscription_store[current->nextIndex];
        }
    }
    semop(subsemid, &leave, 1); //Semaphore freigeben
    return 0;
}

//Funktion zum Löschen aller Elemente eines Keys, falls ein Key gelöscht wird
int subClearKey(char *key){
    unsigned int index = subgenerate_hashcode(key); //Hashcode generieren
    semop(subsemid, &enter, 1); //Semaphore sperren
    struct subscription *current = &subscription_store[index];
    //Falls leer
    if (current->key[0] == '\0') {
        semop(subsemid, &leave, 1); //Semaphore freigeben
        return -1;
    }
    //Überprüfen, ob erstes Element gelöscht werden soll
    while (strcmp(current->key, key) == 0) {
        if (current->nextIndex == 0) { //Überprüft, ob es das einzige Element ist
            current->key[0] = '\0';
            current->processid = 0;
            current->nextIndex = 0;
            break; //Element erfolgreich gelöscht
        } else {
            strncpy(current->key, subscription_store[current->nextIndex].key, KEYSIZE); //Übernimmt den Key des nächsten Elements
            current->processid = subscription_store[current->nextIndex].processid; //Übernimmt den Value des nächsten Elements
            unsigned int temp = current->nextIndex;
            current->nextIndex =subscription_store[current->nextIndex].nextIndex; //Übernimmt den Index des nächsten Elements
            subscription_store[temp].key[0] = '\0'; //Löscht das nächste Element, nachdem es kopiert wurde
            subscription_store[temp].processid = 0;
            subscription_store[temp].nextIndex = 0;
        }
    }
    //Falls erstes Element nicht gelöscht werden soll, Verkettung anwenden, um Element zu finden
    while (current->nextIndex != 0) {
        if (strcmp(subscription_store[current->nextIndex].key, key) == 0) {
            unsigned int temp = current->nextIndex;
            if (subscription_store[current->nextIndex].nextIndex == 0) { //Überprüft, ob es das letzte Element ist
                current->nextIndex = 0;
                subscription_store[temp].key[0] = '\0';
                subscription_store[temp].processid = 0;
                subscription_store[temp].nextIndex = 0;
                break; //Element erfolgreich gelöscht
            }
            current->nextIndex = subscription_store[current->nextIndex].nextIndex; //Übernimmt den Index des nächsten Elements
            subscription_store[temp].key[0] = '\0'; //Löscht das nächste Element, nachdem der Index kopiert wurde
            subscription_store[temp].processid = 0;
            subscription_store[temp].nextIndex = 0;
        }
        current = &subscription_store[current->nextIndex];
    }
    semop(subsemid, &leave, 1); //Semaphore freigeben
    return 0;
}

//Funktion zum Löschen aller Elemente
int subClear() {
    semop(subsemid, &enter, 1); //Semaphore sperren
    for (int i = 0; i < STORESIZE; i++) {
        subscription_store[i].key[0] = '\0';
        subscription_store[i].processid = 0;
        subscription_store[i].nextIndex = 0;
    }
    semop(subsemid, &leave, 1); //Semaphore freigeben
    return 0;
}

//Überprüfen, ob Key bereits vorhanden ist
int subContains(char *key, pid_t pid) {
    unsigned int hashcode = subgenerate_hashcode(key);
    semop(subsemid, &enter, 1); //Semaphore sperren
    struct subscription *current = &subscription_store[hashcode];
    //Überprüft ob Hashtabelle an Index des Hash-Werts leer ist
    if (current->key[0] == '\0') {
        semop(subsemid, &leave, 1); //Semaphore freigeben
        return 0;
    }
    //Falls Index nicht leer, Verkettung anwenden
    while (TRUE) {
        if (strcmp(current->key, key) == 0 && current->processid == pid) {
            semop(subsemid, &leave, 1); //Semaphore freigeben
            return 1;
        }
        if (current->nextIndex == 0) {
            semop(subsemid, &leave, 1); //Semaphore freigeben
            return 0;
        }
        current = &subscription_store[current->nextIndex];
    }
}