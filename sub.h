#ifndef PRAKBS23_SUB_H
#define PRAKBS23_SUB_H

#define HASHMAPSIZE 500
#define PUFFERSIZE 500
#define STORESIZE (HASHMAPSIZE + PUFFERSIZE)
#define KEYSIZE 20

//Initialisierung von Subscribe
int initSubStore();

int deinitSubStore();

int subContains(char *key, pid_t pid);

int subGet(char *key, pid_t *pidarr, int size);

int subPut(char *key, pid_t value);

int subClearKey(char *key);

int subClearProcess(pid_t pid);

#endif //PRAKBS23_SUB_H
