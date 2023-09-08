#include <pthread.h>
#include <math.h>

#include "libs/images/stb_image.h"
#include "libs/globaldefs.h"
#include "commands/load_image/load_image.c"

typedef struct {
    pthread_t thread_id;
    main_options *config;
    unsigned int x[2];
    unsigned int y[2];
    image_data *image;
} upload_worker_t;

int upload_image(main_options *options, image_data *image);

void *upload_worker(void *);

int upload_image(main_options *options, image_data *image) {
    int ret = 0;

    mongoc_client_t *client;
    mongoc_collection_t *config_c;
    mongoc_collection_t *pixels_c;
    mongoc_cursor_t *cursor;
    const bson_t *doc;
    bson_t *query;
    bson_error_t error;

    printf("Uploading image to MongoDB\n");

    client = mongoc_client_pool_pop(options->mongo_session.pool);

    config_c = mongoc_client_get_collection(client, options->mongodb_database, "config");
    pixels_c = mongoc_client_get_collection(client, options->mongodb_database, "pixels");

    query = bson_new();

    BSON_APPEND_UTF8(query, "type", "image");
    BSON_APPEND_UTF8(query, "project", options->project_name);

    cursor = mongoc_collection_find_with_opts(config_c, query, NULL, NULL);
    bson_destroy(query);

    while (mongoc_cursor_next(cursor, &doc)) {
        bson_iter_t iter;
        bson_iter_init(&iter, doc);
        char *status;
        if (bson_iter_find(&iter, "status") && BSON_ITER_HOLDS_UTF8(&iter)) {
            status = bson_iter_dup_utf8(&iter, NULL);
        }
        if (status != 0) {
            if (strcmp(status, "uploading") == 0 && true/*TODO add range check*/) {
                fprintf(stdout, "\n\nthis image is already being uploaded!\n");
                char c = 0;
                while (c != 'y' && c != 'n') {
                    fprintf(stdout, "Should we override the upload?(y/n):\n");
                    fflush(stdout);
                    fflush(stdin);
                    fscanf(stdin, "%c", &c);
                }
                if (c == 'n') {
                    ret = 14;
                }
            } else if (strcmp(status, "complete") == 0) {
                fprintf(stdout, "\n\na previous image was loaded!\n");
                fprintf(stdout, "overwriting will clear all the successive steps (palette, row, maps)!\n");
                char c = 0;
                while (c != 'y' && c != 'n') {
                    fprintf(stdout, "Should we overwrite the upload?(y/n):\n");
                    fflush(stdout);
                    fflush(stdin);
                    fscanf(stdin, "%c", &c);
                }
                if (c == 'n') {
                    ret = 15;
                } else {
                    fprintf(stdout, "deleting all project data!\n");
                    mongoc_collection_t *rows_c;
                    mongoc_collection_t *maps_c;

                    rows_c = mongoc_client_get_collection(client, options->mongodb_database, "rows");
                    maps_c = mongoc_client_get_collection(client, options->mongodb_database, "maps");

                    query = BCON_NEW(
                            "project", BCON_UTF8(options->project_name)
                    );

                    if (!mongoc_collection_delete_many(pixels_c, query, NULL, NULL, &error)) {
                        //TODO add error handling;
                        ret = 16;
                    }

                    if (!mongoc_collection_delete_many(rows_c, query, NULL, NULL, &error)) {
                        //TODO add error handling;
                        ret = 17;
                    }

                    if (!mongoc_collection_delete_many(maps_c, query, NULL, NULL, &error)) {
                        //TODO add error handling;
                        ret = 18;
                    }

                    mongoc_collection_destroy(rows_c);
                    mongoc_collection_destroy(maps_c);

                    bson_t *sub1 = bson_new();

                    BCON_APPEND(
                            query,
                            "type", "{",
                            "$ne", BCON_UTF8("image"),
                            "}"
                    );

                    if (!mongoc_collection_delete_many(config_c, query, NULL, NULL, &error)) {
                        //TODO add error handling;
                        ret = 18;
                    }

                    bson_destroy(sub1);
                    bson_destroy(query);
                }
            }
        }
    }
    mongoc_cursor_destroy(cursor);

    //if everything is fine continue
    if (ret == 0) {
        //set flag entry in config
        //or reset if already exists

        //set as upsert
        bson_t *param = BCON_NEW(
                "upsert", BCON_BOOL(true)
        );

        //prepare query
        query = BCON_NEW(
                "type", BCON_UTF8("image"),
                "project", BCON_UTF8(options->project_name)
        );

        //prepare upsert command
        bson_t *upsert = BCON_NEW(
                "$set", "{",
                "data", BCON_BIN(BSON_SUBTYPE_BINARY, image->image_data, (image->x * image->y * image->channels)),
                "length", BCON_INT64(image->x),
                "height", BCON_INT64(image->y),
                "channels", BCON_INT64(image->channels),
                "status", BCON_UTF8("uploading"),
                "}"
        );

        if (!mongoc_collection_update_one(config_c, query, upsert, param, NULL, &error)) {
            //TODO add error handling;
            ret = 19;
        }

        bson_destroy(param);
        bson_destroy(upsert);
        bson_destroy(query);
    }


    //if everything is fine continue
    if (ret == 0) {
        logger_t logger = {0, &options->count, false, image->x * image->y};

        pthread_create(&logger.thread_id, NULL, logger_worker, &logger);

        upload_worker_t *workers = calloc(options->threads, sizeof(upload_worker_t));

        unsigned long counter = options->threads;

        unsigned int section = ceil((double) image->x / counter);

        options->count = 0;

        for (int i = 0; i < options->threads; i++) {
            upload_worker_t *c_worker = &workers[i];
            c_worker->config = options;
            c_worker->image = image;
            c_worker->x[1] = image->x;
            c_worker->y[0] = i * section;
            c_worker->y[1] = (int) fmin((i + 1) * section, image->y);
            pthread_create(&c_worker->thread_id, NULL, upload_worker, c_worker);
        }

        for (int i = 0; i < options->threads; i++) {
            void *res = 0;
            upload_worker_t *c_worker = &workers[i];
            pthread_join(c_worker->thread_id, &res);
            if (res != 0)
                ret = (int) res;
        }
        free(workers);
        void *res = 0;
        pthread_join(logger.thread_id, &res);
        if (res != 0)
            ret = (int) res;

    }


    //if everything is fine continue
    if (ret == 0) {
        //set flag entry in config

        //set as upsert
        bson_t *param = BCON_NEW(
                "upsert", BCON_BOOL(true)
        );

        //prepare query
        query = BCON_NEW(
                "type", BCON_UTF8("image"),
                "project", BCON_UTF8(options->project_name)
        );

        //prepare upsert command
        bson_t *upsert = BCON_NEW(
                "$set", "{",
                "status", BCON_UTF8("complete"),
                "}"
        );

        if (!mongoc_collection_update_one(config_c, query, upsert, param, NULL, &error)) {
            //TODO add error handling;
            ret = 20;
        }

        bson_destroy(param);
        bson_destroy(upsert);
        bson_destroy(query);
    }

    mongoc_collection_destroy(config_c);
    mongoc_collection_destroy(pixels_c);

    mongoc_client_pool_push(options->mongo_session.pool, client);

    return ret;
}

void *upload_worker(void *arg) {
    upload_worker_t *worker_options = arg;
    int ret = 0;

    mongoc_client_t *client;
    mongoc_collection_t *pixels_c;
    bson_error_t error;
    bson_t *query;
    bson_t *upsert;
    bson_t *color;

    //set as upsert
    bson_t *param = bson_new();
    BSON_APPEND_BOOL(param, "upsert", true);

    client = mongoc_client_pool_pop(worker_options->config->mongo_session.pool);

    pixels_c = mongoc_client_get_collection(client, worker_options->config->mongodb_database, "pixels");

    for (unsigned int x = worker_options->x[0]; x < worker_options->x[1] && ret == 0; x++) {
        for (unsigned int y = worker_options->y[0]; y < worker_options->y[1] && ret == 0; y++) {
            unsigned char *curr = (worker_options->image->image_data) +
                                  (x * worker_options->image->y * worker_options->image->channels) +
                                  (y * worker_options->image->channels);

            unsigned char rgba[4] = {*curr, *(curr + 1), *(curr + 2)};
            if (worker_options->image->channels >= 4) {
                rgba[3] = *(curr + 3);
            } else {
                rgba[3] = 255;
            }

            //prepare query
            query = BCON_NEW(
                    "type", BCON_UTF8("pixel"),
                    "project", BCON_UTF8(worker_options->config->project_name),
                    "x", BCON_INT64(x),
                    "y", BCON_INT64(y),
                    "palette", BCON_NULL,
                    "color_id", BCON_NULL,
                    "color_state", BCON_NULL
            );

            color = BCON_NEW(
                    "0", BCON_INT32(rgba[0]),
                    "1", BCON_INT32(rgba[1]),
                    "2", BCON_INT32(rgba[2]),
                    "3", BCON_INT32(rgba[3])
            );

            upsert = BCON_NEW(
                    "$set", "{",
                    "color", BCON_ARRAY(color),
                    "}"
            );

            if (!mongoc_collection_update_one(pixels_c, query, upsert, param, NULL, &error)) {
                //TODO add error handling;
                ret = 21;
            }

            worker_options->config->count++;

            bson_destroy(upsert);
            bson_destroy(color);
            bson_destroy(query);
        }
    }
    bson_destroy(param);

    mongoc_collection_destroy(pixels_c);

    mongoc_client_pool_push(worker_options->config->mongo_session.pool, client);

    return (void *) ret;
}