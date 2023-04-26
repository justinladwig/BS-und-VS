#include "process_list.h"

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

struct process_node {
    pid_t pid;
    struct process_node* next;
};

typedef struct process_node ProcessNode;

ProcessNode* process_list = NULL; // Anfang der Liste

void add_process(pid_t pid) {
    ProcessNode* new_node = malloc(sizeof(ProcessNode)); // Neues Element erstellen
    new_node->pid = pid; // Prozess-ID speichern

    new_node->next = process_list; // Neues Element am Anfang der Liste hinzufügen
    process_list = new_node;
}

void remove_process(pid_t pid) {
    ProcessNode* current = process_list;
    ProcessNode* previous = NULL;

    while (current != NULL) {
        if (current->pid == pid) {
            if (previous == NULL) {
                // Element am Anfang der Liste löschen
                process_list = current->next;
            } else {
                // Element in der Mitte oder am Ende der Liste löschen
                previous->next = current->next;
            }
            free(current); // Speicher freigeben
            return;
        }
        previous = current;
        current = current->next;
    }
}

void terminate_all_processes() {
    ProcessNode* current = process_list;
    while (current != NULL) {
        //TODO: Problem: Nicht alle Prozesse werden beendet
        kill(current->pid, SIGTERM); // Prozess beenden
        printf("Terminated process with PID %d\n", current->pid);
        current = current->next;
    }
}