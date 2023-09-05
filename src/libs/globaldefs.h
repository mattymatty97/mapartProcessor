#ifndef GOBALDEFS_DEF
#define GOBALDEFS_DEF
#include <mongoc/mongoc.h>

#if defined(__WIN32__) || defined(__WIN64__) || defined(__WINNT__)
#include <windows.h>
unsigned long get_processor_count(){
    SYSTEM_INFO siSysInfo;
    GetSystemInfo(&siSysInfo);
    return siSysInfo.dwNumberOfProcessors;
}
#elif defined(__linux__)
unsigned long get_processor_count(){
    return sysconf(_SC_NPROCESSORS_ONLN);
}
#else
unsigned long get_processor_count(){
    return 1;
}
#endif

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
} main_options;

#endif