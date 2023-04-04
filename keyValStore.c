#include <string.h>
#include "keyValStore.h"
#include "stdlib.h"
#include "sub.h"

struct keyval {
    char key[20];
    char value[20];
    struct keyval *next;
};

struct keyval keyval_store[500] = {
        [0 ... 499] = {
                .key = "",
                .value = "",
                .next = NULL
        }
};

//Hashcode generieren
int generate_hashcode(char *input) {
    int output = 0;
    for (int i = 0; i < strlen(input); i++) {
        output += (int) input[i];
    }
    return output;
}

//Funktion zum Einfügen eines Elements
int put(char *key, char *value) {
    check_key(key);
    int hashcode = generate_hashcode(key);
    int index = hashcode % 500;
    struct keyval *current = &keyval_store[index];
    //Überprüft ob Hashtabelle an Index des Hash-Werts leer ist
    if (current->key[0] == '\0') {
        strcpy(current->key, key);
        strcpy(current->value, value);
        current->next = NULL;
        return 0;
    }
    //Falls Index nicht leer, Verkettung anwenden
    while (current->next != NULL) {
        current = current->next;
    }
    current->next = malloc(sizeof(struct keyval));
    strcpy(current->next->key, key);
    strcpy(current->next->value, value);
    current->next->next = NULL;
    return 0;
}

//Funktion zum Auslesen eines Elements
char *get(char *key) {
    int hashcode = generate_hashcode(key);
    int index = hashcode % 500;
    struct keyval *current = &keyval_store[index];
    //Überprüft ob Hashtabelle an Index des Hash-Werts leer ist
    if (current->key[0] == '\0') {
        return NULL;
    }
    //Falls Index nicht leer, Verkettung anwenden
    while (current != NULL) {
        if (strcmp(current->key, key) == 0) {
            return current->value;
        }
        current = current->next;
    }
    return NULL;
}

//Funktion zum Löschen eines Elements
int delete (char *key) {
    int hashcode = generate_hashcode(key);
    int index = hashcode % 500;
    struct keyval *current = &keyval_store[index];
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
            free(current); //Löscht das erste Element
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
    for (int i = 0; i < 500; i++) {
        keyval_store[i].key[0] = '\0';
        keyval_store[i].value[0] = '\0';
        keyval_store[i].next = NULL;
    }
    return 0;
}

//Überprüfen, ob Key bereits vorhanden ist
int contains(char *key) {
    int hashcode = generate_hashcode(key);
    int index = hashcode % 500;
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
