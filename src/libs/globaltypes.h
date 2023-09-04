#ifndef GOBALTYPES_DEF
#define GOBALTYPES_DEF
#include <mongoc/mongoc.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

struct mongo_session_struct {
    mongoc_uri_t* uri;
    mongoc_client_pool_t* pool;
};

#define mongo_session_struct struct mongo_session_struct

struct main_options {
    char* mongodb_uri;
    char* mongodb_database;
    char* project_name;
    mongo_session_struct mongo_session;
};

#define main_options struct main_options
#endif