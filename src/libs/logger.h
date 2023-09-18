#include <unistd.h>
#include <pthread.h>

typedef struct {
    struct node *head;
    struct node *tail;
    pthread_mutex_t mutex;
    _Atomic unsigned int size;
} log_list_t;

typedef struct {
    pthread_t thread_id;
    log_list_t *logList;
    _Atomic char stopped;
} logger_t;

char loglist_init(log_list_t *logList);

char loglist_append(log_list_t *logList, char *logline);

char *loglist_pop(log_list_t *logList);

void *logger_worker(void *);