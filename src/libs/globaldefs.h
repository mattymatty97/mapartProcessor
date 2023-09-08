#ifndef GOBALDEFS_DEF
#define GOBALDEFS_DEF

#include <mongoc/mongoc.h>
#include <stdatomic.h>
#include <pthread.h>
#include "../opencl/gpu.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

typedef struct {
    mongoc_uri_t* uri;
    mongoc_client_pool_t* pool;
} mongo_session_struct;


typedef struct  {
    char* mongodb_uri;
    char* mongodb_database;
    char* project_name;
    unsigned long threads;
    mongo_session_struct mongo_session;
    gpu_t gpu;
    _Atomic unsigned long count;
} main_options;

typedef struct {
    pthread_t thread_id;
    _Atomic unsigned long *count;
    _Atomic bool stopped;
    unsigned long total;
} logger_t;

void * logger_worker(void *);

#endif