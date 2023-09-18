#include <unistd.h>
#include <getopt.h>
#include <math.h>
#include <pthread.h>

#include "../../libs/globaldefs.h"
#include "../../libs/logger.h"

typedef struct {
    char *palette_name;
    int maximum_height;
    char *dithering;
} command_options;

typedef struct {
    unsigned char *image_data;
    int x;
    int y;
    int channels;
} image_data;

typedef enum {
    NONE = -1,
    LOWER = 0,
    SAME = 1,
    HIGHER = 2
} requirement_e;

int get_image(main_options *config, command_options *options, image_data *mapart_image);

int row_threads(main_options *config, command_options *options, image_data *mapart_image);
int clear_rows(main_options *config, command_options *options);

int generate_rows_command(int argc, char **argv, main_options *config) {
    command_options local_config = {};
    local_config.maximum_height = INT_MIN;

    int ret = 0;

    printf("\nLoad command start\n\n");
    static struct option long_options[] = {
            {"palette",        required_argument, 0, 'p'},
            {"maximum-height", required_argument, 0, 'h'},
            {"dithering",      required_argument, 0, 'd'}
    };

    int c;
    opterr = 0;
    int option_index = 0;
    while ((c = getopt_long(argc, argv, "+:p:d:h:", long_options, &option_index)) != -1) {
        switch (c) {
            case 0:
                /* If this option set a flag, do nothing else now. */
                if (long_options[option_index].flag != 0)
                    break;
                printf("option %s", long_options[option_index].name);
                if (optarg)
                    printf(" with arg %s", optarg);
                printf("\n");
                break;

            case 'p':
                local_config.palette_name = strdup(optarg);
                break;

            case 'd':
                local_config.dithering = strdup(optarg);
                break;

            case 'h': {
                unsigned int height = atoi(optarg);
                if (height != 1 && height != 2)
                    local_config.maximum_height = height;
                break;
            }

            case ':':
                printf("option needs a value\n");
                exit(1);
                break;

            default :
                /* unknown flag ignore for now. */
                break;
        }
    }

    if (local_config.palette_name == 0 || local_config.dithering == 0 || local_config.maximum_height == INT_MIN) {
        printf("missing required options\n");
        return 11;
    }

    image_data mapart = {};
    mapart.channels =2;

    ret = get_image(config, &local_config, &mapart);

    if (ret == 0) {
        ret = clear_rows(config, &local_config);
    }

    if (ret == 0) {
        ret = row_threads(config, &local_config, &mapart);
    }

    return ret;
}

int get_image(main_options *config, command_options *options, image_data *mapart_image) {
    int ret = 0;

    mongoc_client_t *client;
    mongoc_database_t *database;
    mongoc_collection_t *images_c;
    mongoc_gridfs_bucket_t *bucket;
    mongoc_stream_t *file_stream;
    mongoc_cursor_t *cursor;
    bson_value_t mapart_id;
    const bson_t *doc;
    bson_t *query;
    bson_error_t error;

    printf("Loading image from MongoDB\n");
    fflush(stdout);

    client = mongoc_client_pool_pop(config->mongo_session.pool);

    database = mongoc_client_get_database(client, config->mongodb_database);

    images_c = mongoc_client_get_collection(client, config->mongodb_database, "images");

    bucket = mongoc_gridfs_bucket_new(database, NULL, NULL, &error);

    if (!bucket) {
        printf("Error creating gridfs bucket: %s\n", error.message);
        ret = EXIT_FAILURE;
    }

    if (ret == 0) {
        //search images
        query = bson_new();

        BSON_APPEND_UTF8(query, "type", "image");
        BSON_APPEND_UTF8(query, "project", config->project_name);
        BSON_APPEND_UTF8(query, "palette", options->palette_name);
        BSON_APPEND_UTF8(query, "dither", options->dithering);
        BSON_APPEND_INT64(query, "height", options->maximum_height);

        cursor = mongoc_collection_find_with_opts(images_c, query, NULL, NULL);

        if (mongoc_cursor_next(cursor, &doc)) {
            bson_iter_t iter;

            if (bson_iter_init_find(&iter, doc, "mc_image")) {
                const bson_value_t *file_id = bson_iter_value(&iter);
                bson_value_copy(file_id, &mapart_id);
            } else {
                ret = -1;
            }

            if (bson_iter_init_find(&iter, doc, "x")) {
                mapart_image->x = bson_iter_int64(&iter);
            } else {
                ret = -2;
            }

            if (bson_iter_init_find(&iter, doc, "y")) {
                mapart_image->y = bson_iter_int64(&iter);
            } else {
                ret = -3;
            }

            printf("image found\n");
            fflush(stdout);
        } else {
            fprintf(stderr, "Image not found\n");
            ret = 90;
        }
        mongoc_cursor_destroy(cursor);
        bson_destroy(query);
    }

    if (ret == 0) {
        long lenght = mapart_image->x * mapart_image->y * 2;

        printf("Downloading image\n");
        fflush(stdout);
        //download the image
        file_stream = mongoc_gridfs_bucket_open_download_stream(bucket, &mapart_id, &error);
        if (!file_stream) {
            printf("Error opening download stream\n");
            ret = EXIT_FAILURE;
        }

        if (ret == 0) {
            mapart_image->image_data = calloc(lenght, sizeof(unsigned char));
            ret = !mongoc_stream_read(file_stream, mapart_image->image_data, lenght, lenght, -1);
        }
        mongoc_stream_close(file_stream);
        mongoc_stream_destroy(file_stream);
        mongoc_gridfs_bucket_destroy(bucket);
    }

    mongoc_collection_destroy(images_c);
    mongoc_database_destroy(database);
    mongoc_client_pool_push(config->mongo_session.pool, client);

    return ret;
}


typedef struct {
    pthread_t thread_id;
    char * worker_id;
    main_options *config;
    command_options *options;
    log_list_t *logList;
    image_data *image;
} row_worker_t;

int upload_row(row_worker_t *thread_args, mongoc_client_t *client, unsigned int index[2], requirement_e requirement,
               unsigned int *row_buffer) {
    main_options *config = thread_args->config;
    command_options *options = thread_args->options;
    int ret = 0;
    mongoc_collection_t *rows_c;
    bson_t *query;
    bson_t *req;
    bson_error_t error;

    char buffer[30];
    rows_c = mongoc_client_get_collection(client, config->mongodb_database, "rows");

    query = bson_new();
    req = bson_new();
    sprintf(buffer, "-");

    if (requirement == LOWER) {
        BSON_APPEND_INT64(req, "lower", row_buffer[1]);
        sprintf(buffer, "<%d", row_buffer[1]);
    }
    if (requirement == SAME) {
        BSON_APPEND_INT64(req, "same", row_buffer[1]);
        sprintf(buffer, "=%d", row_buffer[1]);
    }
    if (requirement == HIGHER) {
        BSON_APPEND_INT64(req, "higher", row_buffer[1]);
        sprintf(buffer, ">%d", row_buffer[1]);
    }

    BSON_APPEND_UTF8(query, "type", "row");
    BSON_APPEND_UTF8(query, "project", config->project_name);
    BSON_APPEND_UTF8(query, "palette", options->palette_name);
    BSON_APPEND_UTF8(query, "dither", options->dithering);
    BSON_APPEND_INT64(query, "height", options->maximum_height);
    BSON_APPEND_UTF8(query, "name", buffer);
    BSON_APPEND_INT64(query, "x", index[0]);
    BSON_APPEND_INT64(query, "y", index[1]);
    BSON_APPEND_DOCUMENT(query, "requirement", req);
    BSON_APPEND_INT64(query, "end_y", row_buffer[(2 * 128) - 1]);
    BSON_APPEND_BINARY(query, "buffer", BSON_SUBTYPE_BINARY, (unsigned char *) row_buffer,
                       2 * 128 * sizeof(unsigned int));

    char logline[30];
    sprintf(logline, "%s : saving row \"%s\" for section ( %d, %d )", thread_args->worker_id, buffer, index[0], index[1]);
    loglist_append(thread_args->logList, logline);

    ret = !mongoc_collection_insert_one(rows_c, query, NULL, NULL, &error);

    bson_destroy(query);
    bson_destroy(req);
    mongoc_collection_destroy(rows_c);
    return ret;
}


void iter_row_recursive(mongoc_client_t *client, row_worker_t *thread_args, requirement_e requirement, unsigned int x, unsigned int height_limit,
                        unsigned char section, unsigned char section_y, unsigned int* row_buffer){
    if (section_y < 128) {
        unsigned int image_y = section * 128 + section_y;
        unsigned int image_index = (image_y * thread_args->image->x * 2) + x * 2;
        requirement_e curr_requirement = thread_args->image->image_data[image_index + 1];
        if (section_y == 0 || thread_args->image->image_data[image_index] == 0){
            curr_requirement = NONE;
        }
        row_buffer[(section_y)*2] = thread_args->image->image_data[image_index];

        unsigned int min_height = 0;
        unsigned int max_height = height_limit;
        unsigned int last_height = (section_y == 0) ? 0 : row_buffer[(section_y - 1)*2 + 1];

        switch (curr_requirement) {
            case SAME:
                min_height = last_height;
                max_height = last_height;
                break;
            case LOWER:
                max_height = last_height;
                break;
            case HIGHER:
                min_height = last_height;
                break;
            case NONE:
                break;
        }

        for (unsigned int block_height = min_height; block_height <= max_height; block_height++){
            row_buffer[(section_y)*2 + 1] = block_height;
            iter_row_recursive(client, thread_args, requirement, x, height_limit, section, section_y + 1, row_buffer);
        }
    }else{
        unsigned int index[2] = {x, section};
        upload_row(thread_args, client, index, requirement, row_buffer);
    }
}

_Atomic(unsigned int) row_counter = 0;

void * row_worker(void * args){
    row_worker_t * thread_args = args;
    char logline[1000];
    sprintf(logline,"%s Started", thread_args->worker_id);
    loglist_append(thread_args->logList, logline);
    mongoc_client_t *client = mongoc_client_pool_pop(thread_args->config->mongo_session.pool);
    unsigned int section_count = ceil(thread_args->image->y / 128.0);
    unsigned int curr_row = row_counter++;
    //loop until we have processed the entire image
    while(curr_row < thread_args->image->x) {
        sprintf(logline, "%s uses %d row", thread_args->worker_id, curr_row);
        loglist_append(thread_args->logList, logline);
        for(unsigned char section = 0; section < section_count; section++){
            sprintf(logline, "%s : processing section (%d %d)", thread_args->worker_id, curr_row, (int)section);
            loglist_append(thread_args->logList, logline);
            unsigned int image_y = section * 128;
            unsigned int image_index = (image_y * thread_args->image->x * 2) + curr_row * 2;
            requirement_e curr_requirement = NONE;
            if (thread_args->image->image_data[image_index] != 0)
                curr_requirement = (-(thread_args->image->image_data[image_index + 1] - 1)) + 1;
            unsigned int row_buffer[128 *2] = {};
            iter_row_recursive(client, thread_args, curr_requirement, curr_row, thread_args->options->maximum_height, section, 0, row_buffer);
        }
        //get the next index
        curr_row = row_counter++;
    }
    mongoc_client_pool_push(thread_args->config->mongo_session.pool, client);
    sprintf(logline,"%s Stopped", thread_args->worker_id);
    loglist_append(thread_args->logList, logline);
}

int row_threads(main_options *config, command_options *options, image_data *mapart_image) {
    int ret =0;
    log_list_t logList = {};
    loglist_init(&logList);

    printf("Starting threads\n");
    fflush(stdout);

    logger_t logger = {0, &logList};

    pthread_create(&logger.thread_id, NULL, logger_worker, &logger);

    row_worker_t *workers = calloc(config->threads, sizeof(row_worker_t));

    char worker_name[20];

    for (int i = 0; i < config->threads; i++) {
        sprintf(worker_name,"Worker %d", i);
        row_worker_t *c_worker = &workers[i];
        c_worker->worker_id = strdup(worker_name);
        c_worker->config = config;
        c_worker->options = options;
        c_worker->logList = &logList;
        c_worker->image = mapart_image;
        ret |= pthread_create(&c_worker->thread_id, NULL, row_worker, c_worker);
        //row_worker(c_worker);
    }

    for (int i = 0; i < config->threads; i++) {
        void *res = 0;
        row_worker_t *c_worker = &workers[i];
        pthread_join(c_worker->thread_id, &res);
        if (res != 0)
            ret = (int) res;
    }
    printf("Al thread finished\n");
    fflush(stdout);
    free(workers);
    logger.stopped = 1;
    void *res = 0;
    pthread_join(logger.thread_id, &res);
    printf("Logger thread finished\n");
    fflush(stdout);
    if (res != 0)
        ret = (int) res;
    return ret;
}


int clear_rows(main_options *config, command_options *options){
    int ret = 0;
    mongoc_client_t *client;
    mongoc_collection_t *rows_c;
    bson_t *query;
    bson_error_t error;

    printf("Deleting previous rows from MongoDB\n");
    fflush(stdout);

    client = mongoc_client_pool_pop(config->mongo_session.pool);

    rows_c = mongoc_client_get_collection(client, config->mongodb_database, "rows");

    query = bson_new();

    BSON_APPEND_UTF8(query, "type", "row");
    BSON_APPEND_UTF8(query, "project", config->project_name);
    BSON_APPEND_UTF8(query, "palette", options->palette_name);
    BSON_APPEND_UTF8(query, "dither", options->dithering);
    BSON_APPEND_INT64(query, "height", options->maximum_height);

    ret = !mongoc_collection_delete_many(rows_c, query, NULL, NULL, &error);

    bson_destroy(query);

    mongoc_collection_destroy(rows_c);
    mongoc_client_pool_push(config->mongo_session.pool, client);
    return ret;
}
