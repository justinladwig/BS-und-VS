#include <string.h>
#include <ctype.h>
#include "sub.h"


int check_key(char *key) {
    for (int i = 0; i < strlen(key); i++) {
        if (!isalnum(key[i])) { //isalnum prüft, ob Zeichen alphanumerisch ist
            return -1;
        }
    }
    return 0;
}
