#include <string.h>
#include <sys/shm.h>
#include <stdio.h>
#include "keyValStore.h"
#include "stdlib.h"
#include "sub.h"

#define TRUE 1

struct keyval *keyval_store;

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
    int id;
    int segsize = sizeof(struct keyval) * STORESIZE;
    id = shmget(IPC_PRIVATE, segsize, IPC_CREAT | 0644);
    if (id == -1) {
        return -1;
    }
    keyval_store = (struct keyval *) shmat(id, 0, 0);
    if (keyval_store == (void *) -1) {
        return -1;
    }
    initarray();
    return id;
}

//Löschen des Shared Memory
void deinitKeyValStore(int id) {
    shmdt(keyval_store);
    shmctl(id, IPC_RMID, 0);
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
int generate_hashcode(char *input) {
    int output = 0;
    for (int i = 0; i < strlen(input); i++) {
        output += (int) input[i];
    }
    return output % HASHMAPSIZE; //Damit Hashmap nicht überschritten wird
}

//Funktion zum Einfügen eines Elements
int put(char *key, char *value) {
    int index = generate_hashcode(key); //Hashcode generieren
    struct keyval *current = &keyval_store[index]; //Pointer auf Element mit Index des Hashcodes.
    //Überprüft ob Hashtabelle an Index des Hash-Werts leer ist
    if (current->key[0] == '\0') {
        strncpy(current->key, key, KEYSIZE); //Nur Keysize Bytes, da sonst Speicher überschrieben wird. Eingabe wird ggf. abgeschnitten.
        strncpy(current->value, value, VALUESIZE); //Nur Valuesize Bytes, da sonst Speicher überschrieben wird. Eingabe wird ggf. abgeschnitten.
        current->nextIndex = 0;
        return 0;
    }
    //Falls Index nicht leer, Verkettung anwenden
    while (current->nextIndex != 0) {
        current = &keyval_store[current->nextIndex];
    }
    current->nextIndex = nextFreeListIndex();
    if (current->nextIndex == -1) {
        return -1; //Kein freier Speicherplatz mehr
    }
    strncpy(keyval_store[current->nextIndex].key, key, KEYSIZE);
    strncpy(keyval_store[current->nextIndex].value, value, KEYSIZE);
    keyval_store[current->nextIndex].nextIndex = 0;
    return 0;
}

//Value eines Elements ändern
int change(char *key, char *value) {
    int index = generate_hashcode(key); //Hashcode generieren
    struct keyval *current = &keyval_store[index]; //Pointer auf Element mit Index des Hashcodes.
    //Überprüft ob Hashtabelle an Index des Hash-Werts leer ist
    if (current->key[0] == '\0') {
        return -1; //Element nicht gefunden
    }
    //Falls Index nicht leer, Verkettung anwenden
    while (TRUE) {
        if (strcmp(current->key, key) == 0) {
            strncpy(current->value, value, KEYSIZE);
            return 0; //Element erfolgreich geändert
        }
        if (current->nextIndex == 0) {
            return -1; //Element nicht gefunden
        }
        current = &keyval_store[current->nextIndex];
    }
}

//Funktion zum Auslesen eines Elements
char *get(char *key) {
    int index  = generate_hashcode(key); //Hashcode generieren
    struct keyval *current = &keyval_store[index]; //Pointer auf Element mit Index des Hashcodes.
    //Überprüft ob Hashtabelle an Index des Hash-Werts leer ist
    if (current->key[0] == '\0') {
        return NULL;
    }
    //Falls Index nicht leer, Verkettung anwenden
    while (TRUE) {
        if (strcmp(current->key, key) == 0) {
            return current->value; //Element erfolgreich gefunden
        }
        if (current->nextIndex == 0) {
            return NULL; //Element nicht gefunden
        }
        current = &keyval_store[current->nextIndex];
    }
}

//Funktion zum Löschen eines Elements
int delete(char *key) {
    int index = generate_hashcode(key); //Hashcode generieren
    struct keyval *current = &keyval_store[index]; //Pointer auf Element mit Index des Hashcodes.
    //Überprüft ob Hashtabelle an Index des Hash-Werts leer ist
    if (current->key[0] == '\0') {
        return -1; //Element nicht gefunden
    }
    //Überprüfen, ob erstes Element gelöscht werden soll
    if (strcmp(current->key, key) == 0) {
        if (current->nextIndex == 0) { //Überprüft, ob es das einzige Element ist
            current->key[0] = '\0';
            current->value[0] = '\0';
            current->nextIndex = 0;
            return 0; //Element erfolgreich gelöscht
        } else {
            strncpy(current->key, keyval_store[current->nextIndex].key, KEYSIZE); //Übernimmt den Key des nächsten Elements
            strncpy(current->value, keyval_store[current->nextIndex].value, VALUESIZE); //Übernimmt den Value des nächsten Elements
            unsigned int temp = current->nextIndex;
            current->nextIndex = keyval_store[current->nextIndex].nextIndex; //Übernimmt den Index des nächsten Elements
            keyval_store[temp].key[0] = '\0'; //Löscht das nächste Element, nachdem es kopiert wurde
            keyval_store[temp].value[0] = '\0';
            keyval_store[temp].nextIndex = 0;
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
                return 0; //Element erfolgreich gelöscht
            }
            current->nextIndex = keyval_store[current->nextIndex].nextIndex; //Übernimmt den Index des nächsten Elements
            keyval_store[temp].key[0] = '\0'; //Löscht das nächste Element, nachdem der Index kopiert wurde
            keyval_store[temp].value[0] = '\0';
            keyval_store[temp].nextIndex = 0;
            return 0; //Element erfolgreich gelöscht
        }
        current = &keyval_store[current->nextIndex];
    }
    return -1;
}

//Funktion zum Löschen aller Elemente
int clear() {
    for (int i = 0; i < STORESIZE; i++) {
        keyval_store[i].key[0] = '\0';
        keyval_store[i].value[0] = '\0';
        keyval_store[i].nextIndex = 0;
    }
    return 0;
}

//Überprüfen, ob Key bereits vorhanden ist
int contains(char *key) {
    int hashcode = generate_hashcode(key);
    int index = hashcode;
    struct keyval *current = &keyval_store[index];
    //Überprüft ob Hashtabelle an Index des Hash-Werts leer ist
    if (current->key[0] == '\0') {
        return -1;
    }
    //Falls Index nicht leer, Verkettung anwenden
    while (TRUE) {
        if (strcmp(current->key, key) == 0) {
            return 0;
        }
        if (current->nextIndex == 0) {
            return -1;
        }
        current = &keyval_store[current->nextIndex];
    }
}
