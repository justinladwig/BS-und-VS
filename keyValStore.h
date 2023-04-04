#ifndef PRAKBS23_KEYVALSTORE_H
#define PRAKBS23_KEYVALSTORE_H

//Hashcode generieren
int generate_hashcode(char *input);

//Funktion zum Einfügen eines Elements
int put(char *key, char *value);

//Funktion zum Auslesen eines Elements
char *get(char *key) ;

//Funktion zum Löschen eines Elements
int delete (char *key);

//Funktion zum Löschen aller Elemente
int clear();

//Überprüfen, ob Key bereits vorhanden ist
int contains(char *key);

#endif //PRAKBS23_KEYVALSTORE_H
