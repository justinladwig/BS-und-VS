#include "keyValStore.h"

#include <string.h>
#include <sys/shm.h>
#include <sys/sem.h>

#define TRUE 1

int shmid = -1;
int semid = -1;

struct keyval *keyval_store;
struct sembuf enter, leave;


//Shared Memory mit 500 Elementen initialisieren
void initarray() {
    for (int i = 0; i < STORESIZE; i++) {
        keyval_store[i].key[0] = '\0';
        keyval_store[i].value[0] = '\0';
        keyval_store[i].nextIndex = 0;
    }
}

//Anlegen des Shared Memory
int initKeyValStore() {
    //Shared Memory anlegen
    int segsize = sizeof(struct keyval) * STORESIZE;
    shmid = shmget(IPC_PRIVATE, segsize, IPC_CREAT | 0644);
    if (shmid == -1) {
        return -1;
    }
    keyval_store = (struct keyval *) shmat(shmid, 0, 0);
    if (keyval_store == (void *) -1) {
        return -1;
    }

    //Semaphore anlegen
    semid = semget(IPC_PRIVATE, 1, IPC_CREAT | 0644);
    if (semid == -1) {
        return -1;
    }
    semctl(semid, 0, SETVAL, 1); //Semaphore auf 1 setzen
    enter.sem_num = leave.sem_num = 0; //Semaphore 0 verwenden
    enter.sem_flg = leave.sem_flg = SEM_UNDO; //Semaphore wieder freigeben, wenn Prozess beendet wird
    enter.sem_op = -1; //Semaphore um 1 verringern
    leave.sem_op = 1; //Semaphore um 1 erhöhen

    //Array für Key-Value-Paare initialisieren
    initarray();
    return 0;
}

//Löschen des Shared Memory
void deinitKeyValStore() {
    //Shared Memory löschen
    shmdt(keyval_store);
    shmctl(shmid, IPC_RMID, 0);

    //Semaphore löschen
    semctl(semid, 0, IPC_RMID, 0);
}

//Nächstes freies Element in der Liste finden
int nextFreeListIndex() {
    for (int i = HASHMAPSIZE; i < STORESIZE; i++) {
        if (keyval_store[i].key[0] == '\0') {
            return i;
        }
    }
    return -1;
}

//Hashcode für einen Key generieren
//TODO: Überprüfen, dass Hashcode Größe des Arrays nicht übersteigt https://www.digitalocean.com/community/tutorials/hash-table-in-c-plus-plus
unsigned long generate_hashcode(char *input) {
    unsigned long hash = 5381;
    int c;

    while ((c = *input++)) hash = ((hash << 5) + hash) + c;

    return hash % HASHMAPSIZE;
}


//Funktion zum Einfügen eines Elements
int put(char *key, char *value) {
    unsigned long index = generate_hashcode(key); //Hashcode generieren
    semop(semid, &enter, 1); //Semaphore sperren
    struct keyval *current = &keyval_store[index]; //Pointer auf Element mit Index des Hashcodes.
    //Überprüft ob Hashtabelle an Index des Hash-Werts leer ist
    if (current->key[0] == '\0') {
        strncpy(current->key, key, KEYSIZE); //Nur Keysize Bytes, da sonst Speicher überschrieben wird. Eingabe wird ggf. abgeschnitten.
        strncpy(current->value, value, VALUESIZE); //Nur Valuesize Bytes, da sonst Speicher überschrieben wird. Eingabe wird ggf. abgeschnitten.
        current->nextIndex = 0;
        semop(semid, &leave, 1); //Semaphore freigeben
        return 0;
    }
    //Falls Index nicht leer, Verkettung anwenden
    while (current->nextIndex != 0) {
        current = &keyval_store[current->nextIndex];
    }
    current->nextIndex = nextFreeListIndex();
    if (current->nextIndex == -1) {
        semop(semid, &leave, 1); //Semaphore freigeben
        return -1; //Kein freier Speicherplatz mehr
    }
    strncpy(keyval_store[current->nextIndex].key, key, KEYSIZE);
    strncpy(keyval_store[current->nextIndex].value, value, KEYSIZE);
    keyval_store[current->nextIndex].nextIndex = 0;
    semop(semid, &leave, 1); //Semaphore freigeben
    return 0;
}

//Value eines Elements ändern
int change(char *key, char *value) {
    unsigned long index = generate_hashcode(key); //Hashcode generieren
    semop(semid, &enter, 1); //Semaphore sperren
    struct keyval *current = &keyval_store[index]; //Pointer auf Element mit Index des Hashcodes.
    //Überprüft ob Hashtabelle an Index des Hash-Werts leer ist
    if (current->key[0] == '\0') {
        semop(semid, &leave, 1); //Semaphore freigeben
        return -1; //Element nicht gefunden
    }
    //Falls Index nicht leer, Verkettung anwenden
    while (TRUE) {
        if (strcmp(current->key, key) == 0) {
            strncpy(current->value, value, KEYSIZE);
            semop(semid, &leave, 1); //Semaphore freigeben
            return 0; //Element erfolgreich geändert
        }
        if (current->nextIndex == 0) {
            semop(semid, &leave, 1); //Semaphore freigeben
            return -1; //Element nicht gefunden
        }
        current = &keyval_store[current->nextIndex];
    }
}

//Funktion zum Auslesen eines Elements
char *get(char *key) {
    unsigned int index  = generate_hashcode(key); //Hashcode generieren
    semop(semid, &enter, 1); //Semaphore sperren
    struct keyval *current = &keyval_store[index]; //Pointer auf Element mit Index des Hashcodes.
    //Überprüft ob Hashtabelle an Index des Hash-Werts leer ist
    if (current->key[0] == '\0') {
        semop(semid, &leave, 1); //Semaphore freigeben
        return NULL;
    }
    //Falls Index nicht leer, Verkettung anwenden
    while (TRUE) {
        if (strcmp(current->key, key) == 0) {
            semop(semid, &leave, 1); //Semaphore freigeben
            return current->value; //Element erfolgreich gefunden
        }
        if (current->nextIndex == 0) {
            semop(semid, &leave, 1); //Semaphore freigeben
            return NULL; //Element nicht gefunden
        }
        current = &keyval_store[current->nextIndex];
    }
}

//Funktion zum Löschen eines Elements
int delete(char *key) {
    unsigned int index = generate_hashcode(key); //Hashcode generieren
    semop(semid, &enter, 1); //Semaphore sperren
    struct keyval *current = &keyval_store[index]; //Pointer auf Element mit Index des Hashcodes.
    //Überprüft ob Hashtabelle an Index des Hash-Werts leer ist
    if (current->key[0] == '\0') {
        semop(semid, &leave, 1); //Semaphore freigeben
        return -1; //Element nicht gefunden
    }
    //Überprüfen, ob erstes Element gelöscht werden soll
    if (strcmp(current->key, key) == 0) {
        if (current->nextIndex == 0) { //Überprüft, ob es das einzige Element ist
            current->key[0] = '\0';
            current->value[0] = '\0';
            current->nextIndex = 0;
            semop(semid, &leave, 1); //Semaphore freigeben
            return 0; //Element erfolgreich gelöscht
        } else {
            strncpy(current->key, keyval_store[current->nextIndex].key, KEYSIZE); //Übernimmt den Key des nächsten Elements
            strncpy(current->value, keyval_store[current->nextIndex].value, VALUESIZE); //Übernimmt den Value des nächsten Elements
            unsigned int temp = current->nextIndex;
            current->nextIndex = keyval_store[current->nextIndex].nextIndex; //Übernimmt den Index des nächsten Elements
            keyval_store[temp].key[0] = '\0'; //Löscht das nächste Element, nachdem es kopiert wurde
            keyval_store[temp].value[0] = '\0';
            keyval_store[temp].nextIndex = 0;
            semop(semid, &leave, 1); //Semaphore freigeben
            return 0; //Element erfolgreich gelöscht
        }
    }
    //Falls erstes Element nicht gelöscht werden soll, Verkettung anwenden, um Element zu finden
    while (current->nextIndex != 0) {
        if (strcmp(keyval_store[current->nextIndex].key, key) == 0) {
            unsigned int temp = current->nextIndex;
            if (keyval_store[current->nextIndex].nextIndex == 0) { //Überprüft, ob es das letzte Element ist
                current->nextIndex = 0;
                keyval_store[temp].key[0] = '\0';
                keyval_store[temp].value[0] = '\0';
                keyval_store[temp].nextIndex = 0;
                semop(semid, &leave, 1); //Semaphore freigeben
                return 0; //Element erfolgreich gelöscht
            }
            current->nextIndex = keyval_store[current->nextIndex].nextIndex; //Übernimmt den Index des nächsten Elements
            keyval_store[temp].key[0] = '\0'; //Löscht das nächste Element, nachdem der Index kopiert wurde
            keyval_store[temp].value[0] = '\0';
            keyval_store[temp].nextIndex = 0;
            semop(semid, &leave, 1); //Semaphore freigeben
            return 0; //Element erfolgreich gelöscht
        }
        current = &keyval_store[current->nextIndex];
    }
    semop(semid, &leave, 1); //Semaphore freigeben
    return -1;
}

//Funktion zum Löschen aller Elemente
int clear() {
    semop(semid, &enter, 1); //Semaphore sperren
    for (int i = 0; i < STORESIZE; i++) {
        keyval_store[i].key[0] = '\0';
        keyval_store[i].value[0] = '\0';
        keyval_store[i].nextIndex = 0;
    }
    semop(semid, &leave, 1); //Semaphore freigeben
    return 0;
}

//Überprüfen, ob Key bereits vorhanden ist
int contains(char *key) {
    unsigned int hashcode = generate_hashcode(key);
    semop(semid, &enter, 1); //Semaphore sperren
    struct keyval *current = &keyval_store[hashcode];
    //Überprüft ob Hashtabelle an Index des Hash-Werts leer ist
    if (current->key[0] == '\0') {
        semop(semid, &leave, 1); //Semaphore freigeben
        return -1;
    }
    //Falls Index nicht leer, Verkettung anwenden
    while (TRUE) {
        if (strcmp(current->key, key) == 0) {
            semop(semid, &leave, 1); //Semaphore freigeben
            return 0;
        }
        if (current->nextIndex == 0) {
            semop(semid, &leave, 1); //Semaphore freigeben
            return -1;
        }
        current = &keyval_store[current->nextIndex];
    }
}
