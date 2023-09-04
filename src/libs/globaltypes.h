#ifndef GOBALTYPES_DEF
#define GOBALTYPES_DEF
#include <mongoc/mongoc.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

typedef struct {
    mongoc_uri_t* uri;
    mongoc_client_pool_t* pool;
} mongo_session_struct;


typedef struct  {
    char* mongodb_uri;
    char* mongodb_database;
    char* project_name;
    mongo_session_struct mongo_session;
} main_options;

#endif