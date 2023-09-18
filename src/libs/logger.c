#include <stdlib.h>
#include <stdio.h>
#include "logger.h"

typedef struct node{
    char * val;
    struct node * next;
} node_t;

char loglist_init(log_list_t * logList){
    logList->head = NULL;
    logList->head = NULL;
    logList->mutex = PTHREAD_MUTEX_INITIALIZER;
    logList->size = 0;
    return 0;
}


char loglist_append(log_list_t * logList, char * logline){
    int ret = 0;
    pthread_mutex_lock(&logList->mutex);
    if (logList->size<500) {
        node_t *node = calloc(1, sizeof(node_t));
        if (!node) {
            ret = 1;
        } else {
            node->val = strdup(logline);

            if (!logList->head) {
                logList->head = node;
                logList->tail = node;
            } else {
                node_t *tail = logList->tail;
                tail->next = node;
                logList->tail = node;
            }
            logList->size++;
        }
    }
    pthread_mutex_unlock(&logList->mutex);
    return ret;
}

char * loglist_pop(log_list_t * logList){
    char* ret = NULL;
    pthread_mutex_lock(&logList->mutex);
    if (logList->head){
        node_t * node = logList->head;
        logList->head = node->next;
        if (!logList->head)
            logList->tail = NULL;
        ret = node->val;
        logList->size--;
        free(node);
    }
    pthread_mutex_unlock(&logList->mutex);
    return ret;
}

void *logger_worker(void *arg) {
    logger_t *worker_options = arg;
    char * logline;
    while (!(worker_options->stopped)){
        while ((logline = loglist_pop(worker_options->logList)) != NULL){
            fprintf(stdout,"%s\n", logline);
            free(logline);
        }
        fflush(stdout);
        sleep(7);
    }
    return 0;
}