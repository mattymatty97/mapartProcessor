#include "../../libs/globaldefs.h"

int list_images_command(int argc, char **argv, main_options *config) {
    int ret = 0;
    printf("\nList command start\n\n");

    mongoc_client_t *client;
    mongoc_collection_t *collection;
    mongoc_cursor_t *cursor;
    const bson_t *doc;
    bson_t *query;
    char *str;

    printf("Searching images\n");
    fflush(stdout);

    client = mongoc_client_pool_pop(config->mongo_session.pool);

    collection = mongoc_client_get_collection(client, config->mongodb_database, "images");

    query = bson_new();

    BSON_APPEND_UTF8(query, "type", "image");
    BSON_APPEND_UTF8(query, "project", config->project_name);

    cursor = mongoc_collection_find_with_opts(collection, query, NULL, NULL);

    int count = 0;
    while (mongoc_cursor_next(cursor, &doc)) {
        if (count++ == 0) {
            printf("Found Images:\n");
        }
        fprintf(stdout, "\t- %d:\n", count);
        bson_iter_t iter;
        if (bson_iter_init_find(&iter, doc, "x") && BSON_ITER_HOLDS_INT64(&iter)) {
            fprintf(stdout, "\t  size: %dx", bson_iter_int64(&iter));
        }
        if (bson_iter_init_find(&iter, doc, "y") && BSON_ITER_HOLDS_INT64(&iter)) {
            fprintf(stdout, "%d\n", bson_iter_int64(&iter));
        }
        if (bson_iter_init_find(&iter, doc, "palette") && BSON_ITER_HOLDS_UTF8(&iter)) {
            fprintf(stdout, "\t  palette: %s\n", bson_iter_utf8(&iter, NULL));
        }
        if (bson_iter_init_find(&iter, doc, "dither") && BSON_ITER_HOLDS_UTF8(&iter)) {
            fprintf(stdout, "\t  dither: %s\n", bson_iter_utf8(&iter, NULL));
        }
        if (bson_iter_init_find(&iter, doc, "height") && BSON_ITER_HOLDS_INT64(&iter)) {
            fprintf(stdout, "\t  max_height: %d\n", bson_iter_int64(&iter));
        }
    }

    if (count == 0) {
        fprintf(stdout, "No Image was found`n");
    }

    bson_destroy(query);
    mongoc_cursor_destroy(cursor);
    mongoc_collection_destroy(collection);

    mongoc_client_pool_push(config->mongo_session.pool, client);

    return ret;
}