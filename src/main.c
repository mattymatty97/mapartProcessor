#include <stdio.h>
#include <unistd.h>
#include <getopt.h>

#include <mongoc/mongoc.h>

#define STB_IMAGE_IMPLEMENTATION
#include "libs/images/stb_image.h"
#include "libs/globaldefs.h"
#include "commands/load/load.h"

static main_options config;

struct cmd_struct {
    const char *cmd;
    int (*fn) (int, char **, main_options*);
};

void connect_mongodb();
void cleanup();

static struct option long_options[] = {
        {"mongo-uri", required_argument, 0, 'u'},
        {"mongo-database", required_argument, 0, 'd'},
        {"project-name", required_argument, 0, 'p'},
        {"threads", required_argument, 0, 't'}
};

static struct cmd_struct commands[] = {
        {"load", load_command}
};

int main(int argc, char** argv) {
    int ret;
    int c;
    opterr = 0;

    config.threads = get_processor_count();

    int option_index = 0;
    unsigned long thread_count;
    while ((c = getopt_long (argc, argv, "+:u:d:p:t:", long_options, &option_index)) != -1){
        switch (c)
        {
            case 0:
                /* If this option set a flag, do nothing else now. */
                if (long_options[option_index].flag != 0)
                    break;
                printf ("option %s", long_options[option_index].name);
                if (optarg)
                    printf (" with arg %s", optarg);
                printf ("\n");
                break;

            case 'u':
                config.mongodb_uri = strdup(optarg);
                break;

            case 'd':
                config.mongodb_database = strdup(optarg);
                break;

            case 'p':
                config.project_name = strdup(optarg);
                break;

            case 't':
                thread_count = strtol(optarg, NULL, 10);
                if (thread_count>0)
                    config.threads = thread_count;
                break;

            case ':':
                printf("option needs a value\n");
                exit(1);
                break;

            default :
                /* unknown flag ignore for now. */
                break;
        }
    }

    if (config.project_name == 0 || config.mongodb_uri == 0 || config.mongodb_database == 0){
        printf("missing required options\n");
        exit(2);
    }

    connect_mongodb();

    struct cmd_struct *cmd = NULL;
    for (int i = 0; i < ARRAY_SIZE(commands) && optind < argc; i++) {
        if (!strcmp(commands[i].cmd, argv[optind])) {
            cmd = &commands[i];
        }
    }
    if (cmd) {
        optind++;
        ret = cmd->fn(argc, argv, &config);
    }else {
        fprintf (stderr,
                 "Missing Command\n");
        ret = 3;
    }

    cleanup();

    return ret;
}

void connect_mongodb(){
    mongoc_uri_t *uri;
    mongoc_client_pool_t *pool;
    mongoc_client_t *client;
    mongoc_database_t *database;
    mongoc_collection_t *collection;
    bson_t *command, reply;
    bson_error_t error;

    printf("initializing mongodb driver\n\n");

    mongoc_init();

    printf("preparing mongo uri with %s\n",config.mongodb_uri);

    uri = mongoc_uri_new_with_error (config.mongodb_uri, &error);

    if (!uri) {
        fprintf (stderr,
                 "failed to parse URI: %s\n"
                 "error message:       %s\n",
                 config.mongodb_uri,
                 error.message);
        exit(EXIT_FAILURE);
    }


    printf("creating mongodb connection pool\n");

    pool = mongoc_client_pool_new (uri);
    if (!pool) {
        fprintf (stderr,
                 "failed to create client pool:\n"
                 "error message:       %s\n",
                 error.message);
        exit( EXIT_FAILURE);
    }
    mongoc_client_pool_set_error_api (pool, 2);
    mongoc_client_pool_set_appname(pool, config.project_name);

    config.mongo_session.pool = pool;


    printf("obtaining test mongodb client\n");

    client = mongoc_client_pool_pop(pool);

    if (!client) {
        fprintf (stderr,
                 "failed to obtain clinet from pool:\n"
                 "error message:       %s\n",
                 error.message);
        exit( EXIT_FAILURE);
    }

    printf("\ntrying pinging mongodb\n");

    command = BCON_NEW ("ping", BCON_INT32 (1));

    int retval = mongoc_client_command_simple (
            client, config.mongodb_database, command, NULL, &reply, &error);

    if (!retval) {
        fprintf (stderr, "mongodb ping returned: %s\n", error.message);
        exit(EXIT_FAILURE);
    }

    char* str = bson_as_json (&reply, NULL);
    printf ("mongodb ping returned: %s\n", str);

    printf("\nfetching target database\n");

    database = mongoc_client_get_database (client, config.mongodb_database);

    if (!database) {
        fprintf (stderr,
                 "failed to find database %s:\n"
                 "error message:       %s\n",
                 config.mongodb_database,
                 error.message);
        exit( EXIT_FAILURE);
    }

    mongoc_database_destroy(database);

    printf("fetching config collection\n");

    collection = mongoc_client_get_collection (client, config.mongodb_database, "config");

    if (!collection) {
        fprintf (stderr,
                 "failed to find collection config:\n"
                 "error message:       %s\n",
                 error.message);
        exit( EXIT_FAILURE);
    }

    mongoc_collection_destroy(collection);

    printf("fetching pixels collection\n");

    collection = mongoc_client_get_collection (client, config.mongodb_database, "pixels");

    if (!collection) {
        fprintf (stderr,
                 "failed to find collection pixels:\n"
                 "error message:       %s\n",
                 error.message);
        exit( EXIT_FAILURE);
    }

    mongoc_collection_destroy(collection);

    printf("fetching rows collection\n");

    collection = mongoc_client_get_collection (client, config.mongodb_database, "rows");

    if (!collection) {
        fprintf (stderr,
                 "failed to find collection rows:\n"
                 "error message:       %s\n",
                 error.message);
        exit( EXIT_FAILURE);
    }

    mongoc_collection_destroy(collection);

    printf("fetching maps collection\n");

    collection = mongoc_client_get_collection (client, config.mongodb_database, "maps");

    if (!collection) {
        fprintf (stderr,
                 "failed to find collection maps:\n"
                 "error message:       %s\n",
                 error.message);
        exit( EXIT_FAILURE);
    }

    mongoc_collection_destroy(collection);

    printf("mongodb structure is valid!\n");
    printf("\ncleaning up test client\n");

    mongoc_client_pool_push(pool, client);

    bson_destroy (&reply);
    bson_destroy (command);
    bson_free (str);

    printf("\nmongodb init done!\n\n");

}

void cleanup(){
    mongoc_uri_destroy (config.mongo_session.uri);
    mongoc_client_pool_destroy (config.mongo_session.pool);
    mongoc_cleanup ();
}