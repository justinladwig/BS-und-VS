#include <string.h>
#include "keyValStore.h"
#include "stdlib.h"
#include "sub.h"

#define STORESIZE 500
#define KEYSIZE 20
#define VALUESIZE 50

//Erzeugung eines Knoten für eine Liste
struct keyval {
    char key[KEYSIZE];      //Schlüssel, um die Daten zu finden
    char value[VALUESIZE];  //Daten, welche mit dem Schlüssel aufgerufen werden können
    struct keyval *next;    //Nächster Knoten einer Liste
};

// Erzeugung eines leeren Arrays
struct keyval keyval_store[STORESIZE] = {
        [0 ... 499] = {
                .key = "",
                .value = "",
                .next = NULL
        }
};

//Hashcode für einen Key generieren
//TODO: Überprüfen, dass Hashcode Größe des Arrays nicht übersteigt https://www.digitalocean.com/community/tutorials/hash-table-in-c-plus-plus
int generate_hashcode(char *input) {
    int output = 0;
    for (int i = 0; i < strlen(input); i++) {
        output += (int) input[i];
    }
    return output % STORESIZE; //Damit Arraygröße nicht überschritten wird
}

//Funktion zum Einfügen eines Elements
int put(char *key, char *value) {
    int index = generate_hashcode(key); //Hashcode generieren
    struct keyval *current = &keyval_store[index]; //Pointer auf Element mit Index des Hashcodes.
    //Überprüft ob Hashtabelle an Index des Hash-Werts leer ist
    if (current->key[0] == '\0') {
        strncpy(current->key, key, KEYSIZE); //Nur Keysize Bytes, da sonst Speicher überschrieben wird. Eingabe wird ggf. abgeschnitten.
        strncpy(current->value, value, VALUESIZE); //Nur Valuesize Bytes, da sonst Speicher überschrieben wird. Eingabe wird ggf. abgeschnitten.
        current->next = NULL;
        return 0;
    }
    //Falls Index nicht leer, Verkettung anwenden
    while (current->next != NULL) {
        current = current->next;
    }
    current->next = malloc(sizeof(struct keyval));
    strncpy(current->next->key, key, KEYSIZE);
    strncpy(current->next->value, value, KEYSIZE);
    current->next->next = NULL;
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
    while (current != NULL) {
        if (strcmp(current->key, key) == 0) {
            strncpy(current->value, value, KEYSIZE);
            return 0; //Element erfolgreich geändert
        }
        current = current->next;
    }
    return -1; //Element nicht gefunden
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
    while (current != NULL) {
        if (strcmp(current->key, key) == 0) {
            return current->value; //Element erfolgreich gefunden
        }
        current = current->next;
    }
    return NULL;
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
        if (current->next == NULL) { //Überprüft, ob es das einzige Element ist
            current->key[0] = '\0';
            current->value[0] = '\0';
            current->next = NULL;
            return 0; //Element erfolgreich gelöscht
        } else {
            keyval_store[index] = *current->next; //Übernimmt die Werte des nächsten Elements
            free(current); //Löscht das erste Element vom Speicher
            return 0; //Element erfolgreich gelöscht
        }
    }
    //Falls erstes Element nicht gelöscht werden soll, Verkettung anwenden, um Element zu finden
    while (current->next != NULL) {
        if (strcmp(current->next->key, key) == 0) {
            struct keyval *temp = current->next;
            if (current->next->next == NULL) { //Überprüft, ob es das letzte Element ist
                current->next = NULL;
                free(temp);
                return 0; //Element erfolgreich gelöscht
            }
            current->next = current->next->next; //Überspringt das zu löschende Element
            free(temp); //Löscht das zu löschende Element
            return 0;
        }
        current = current->next;
    }
    return -1;
}

//Funktion zum Löschen aller Elemente
int clear() {
    for (int i = 0; i < STORESIZE; i++) {
        keyval_store[i].key[0] = '\0';
        keyval_store[i].value[0] = '\0';
        keyval_store[i].next = NULL;
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
    while (current != NULL) {
        if (strcmp(current->key, key) == 0) {
            return 0;
        }
        current = current->next;
    }
    return -1;
}
