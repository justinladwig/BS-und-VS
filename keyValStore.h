#ifndef PRAKBS23_KEYVALSTORE_H
#define PRAKBS23_KEYVALSTORE_H

#define HASHMAPSIZE 500
#define PUFFERSIZE 500
#define STORESIZE (HASHMAPSIZE + PUFFERSIZE)
#define KEYSIZE 20
#define VALUESIZE 50

//Initialisierung des Speichers
int initKeyValStore();

//Löschen des Speichers
void deinitKeyValStore(int id);

//Erzeugung eines Knoten für eine Liste
struct keyval {
    char key[KEYSIZE];      //Schlüssel, um die Daten zu finden
    char value[VALUESIZE];  //Daten, welche mit dem Schlüssel aufgerufen werden können
    unsigned int nextIndex;    //Nächster Knoten einer Liste
};

//Hashcode generieren
int generate_hashcode(char *input);

//Funktion zum Einfügen eines Elements
int put(char *key, char *value);

//Value eines Elements ändern
int change(char *key, char *value);

//Funktion zum Auslesen eines Elements
char *get(char *key);

//Funktion zum Löschen eines Elements
int delete(char *key);

//Funktion zum Löschen aller Elemente
int clear();

//Überprüfen, ob Key bereits vorhanden ist
int contains(char *key);

#endif //PRAKBS23_KEYVALSTORE_H
