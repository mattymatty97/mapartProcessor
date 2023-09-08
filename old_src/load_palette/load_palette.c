#include <unistd.h>
#include <getopt.h>
#include <math.h>

#include "libs/images/stb_image.h"
#include "libs/globaldefs.h"

#define PALETTE_SIZE 62
#define RGB_SIZE 3
#define MULTIPLIER_SIZE 4

typedef struct {
    char* palette_name;
} command_options;

typedef struct {
    char * palette_name;
    int r[PALETTE_SIZE][MULTIPLIER_SIZE];
    int g[PALETTE_SIZE][MULTIPLIER_SIZE];
    int b[PALETTE_SIZE][MULTIPLIER_SIZE];
} mapart_palette;

typedef struct {
    pthread_t thread_id;
    main_options *config;
    unsigned int x[2];
    unsigned int y[2];
    mapart_palette *palette;
} palette_worker_t;

int get_palette(main_options *config, command_options *local_config, mapart_palette *palette);

int parse_palette(main_options *config, command_options *local_config, mapart_palette * palette);

void *palette_worker(void *);

int load_palette_command(int argc, char** argv, main_options *config){
    command_options local_config = {};
    int ret = 0;

    printf("\nLoad command start\n\n");
    static struct option long_options[] = {
            {"palette", required_argument, 0, 'p'}
    };

    int c;
    opterr = 0;
    int option_index = 0;
    while ((c = getopt_long (argc, argv, "+:p:", long_options, &option_index)) != -1){
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

            case 'p':
                local_config.palette_name = strdup(optarg);
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

    if (local_config.palette_name == 0 ){
        printf("missing required options\n");
        return 41;
    }

    mapart_palette palette= {local_config.palette_name,{},{},{}};

    ret = get_palette(config, &local_config, &palette);

    //if everything is ok
    if (ret == 0){
       ret = parse_palette(config, &local_config, &palette);
    }

    return ret;
}

int parse_palette(main_options *config, command_options *local_config, mapart_palette * palette){
    /*
    int ret = 0;

    mongoc_client_t *client;
    mongoc_collection_t *config_c;
    mongoc_collection_t *pixels_c;
    mongoc_cursor_t *cursor;
    const bson_t *doc;
    bson_t *query;
    bson_error_t error;

    printf("Uploading image to MongoDB\n");

    client = mongoc_client_pool_pop(config->mongo_session.pool);

    config_c = mongoc_client_get_collection(client, config->mongodb_database, "config");

    query = bson_new();

    BSON_APPEND_UTF8(query, "type", "image");
    BSON_APPEND_UTF8(query, "project", config->project_name);

    cursor = mongoc_collection_find_with_opts(config_c, query, NULL, NULL);
    bson_destroy (query);

    while (mongoc_cursor_next (cursor, &doc)) {
        bson_iter_t iter;
        bson_iter_init(&iter,doc);
        char* status;
        if(bson_iter_find(&iter, "status") && BSON_ITER_HOLDS_UTF8(&iter)){
            status = bson_iter_dup_utf8(&iter,NULL);
        }
        if (status != 0){
            if (strcmp(status, "uploading") == 0 && true){
                fprintf(stdout, "\n\nthis image is already being uploaded!\n");
                char c = 0;
                while (c != 'y' && c != 'n') {
                    fprintf(stdout,"Should we override the upload?(y/n):\n");
                    fflush(stdout);
                    fflush(stdin);
                    fscanf(stdin, "%c", &c);
                }
                if (c=='n'){
                    ret = 14;
                }
            } else if (strcmp(status,"complete") == 0) {
                fprintf(stdout, "\n\na previous image was loaded!\n");
                fprintf(stdout,"overwriting will clear all the successive steps (palette, row, maps)!\n");
                char c = 0;
                while (c != 'y' && c != 'n') {
                    fprintf(stdout,"Should we overwrite the upload?(y/n):\n");
                    fflush(stdout);
                    fflush(stdin);
                    fscanf(stdin, "%c", &c);
                }
                if (c=='n'){
                    ret = 15;
                }else{
                    fprintf(stdout,"deleting all project data!\n");
                    mongoc_collection_t *rows_c;
                    mongoc_collection_t *maps_c;

                    rows_c = mongoc_client_get_collection(client, options->mongodb_database, "rows");
                    maps_c = mongoc_client_get_collection(client, options->mongodb_database, "maps");

                    query = BCON_NEW(
                            "project", BCON_UTF8(options->project_name)
                    );

                    if (!mongoc_collection_delete_many(pixels_c, query, NULL, NULL, &error)){
                        //TODO add error handling;
                        ret = 16;
                    }

                    if (!mongoc_collection_delete_many(rows_c, query, NULL, NULL, &error)){
                        //TODO add error handling;
                        ret = 17;
                    }

                    if (!mongoc_collection_delete_many(maps_c, query, NULL, NULL, &error)){
                        //TODO add error handling;
                        ret = 18;
                    }

                    mongoc_collection_destroy (rows_c);
                    mongoc_collection_destroy (maps_c);

                    bson_t * sub1 = bson_new();

                    BCON_APPEND(
                            query,
                            "type" , "{",
                            "$ne", BCON_UTF8("image"),
                            "}"
                    );

                    if (!mongoc_collection_delete_many(config_c, query, NULL, NULL, &error)){
                        //TODO add error handling;
                        ret = 18;
                    }

                    bson_destroy(sub1);
                    bson_destroy(query);
                }
            }
        }
    }
    mongoc_cursor_destroy (cursor);

    //if everything is fine continue
    if (ret == 0){
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
                "data", BCON_BIN(BSON_SUBTYPE_BINARY, image->image_data, (image->x*image->y*image->channels)),
                "length", BCON_INT64(image->x),
                "height", BCON_INT64(image->y),
                "channels", BCON_INT64(image->channels),
                "status", BCON_UTF8("uploading"),
                "}"
        );

        if (!mongoc_collection_update_one(config_c, query, upsert, param, NULL, &error)){
            //TODO add error handling;
            ret = 19;
        }

        bson_destroy(param);
        bson_destroy(upsert);
        bson_destroy(query);
    }


    //if everything is fine continue
    if (ret == 0){
        logger_t logger = {0, &options->count, false,image->x * image->y};

        pthread_create(&logger.thread_id,NULL,logger_worker,&logger);

        upload_worker_t *workers = calloc(options->threads,sizeof (upload_worker_t));

        unsigned long counter = options->threads;

        unsigned int section = ceil((double)image->x / counter);

        options->count = 0;

        for (int i = 0; i < options->threads; i++){
            upload_worker_t *c_worker = &workers[i];
            c_worker->config = options;
            c_worker->image = image;
            c_worker->x[1] =image->x;
            c_worker->y[0] = i * section;
            c_worker->y[1] = (int)fmin((i +1) * section, image->y);
            pthread_create(&c_worker->thread_id,NULL,upload_worker,c_worker);
        }

        for (int i = 0; i < options->threads; i++){
            void * res = 0;
            upload_worker_t *c_worker = &workers[i];
            pthread_join(c_worker->thread_id, &res);
            if (res != 0)
                ret = (int)res;
        }
        free(workers);
        void * res = 0;
        pthread_join(logger.thread_id, &res);
        if (res != 0)
            ret = (int)res;

    }


    //if everything is fine continue
    if (ret == 0){
        //set flag entry in config

        //set as upsert
        bson_t *param = BCON_NEW(
                "upsert", BCON_BOOL(true)
        );

        //prepare query
        query = BCON_NEW(
                "type", BCON_UTF8("image"),
                "project", BCON_UTF8(config->project_name)
        );

        //prepare upsert command
        bson_t *upsert = BCON_NEW(
                "$set", "{",
                "status", BCON_UTF8("complete"),
                "}"
        );

        if (!mongoc_collection_update_one(config_c, query, upsert, param, NULL, &error)){
            //TODO add error handling;
            ret = 20;
        }

        bson_destroy(param);
        bson_destroy(upsert);
        bson_destroy(query);
    }

    mongoc_collection_destroy (config_c);
    mongoc_collection_destroy (pixels_c);

    mongoc_client_pool_push(config->mongo_session.pool, client);

    return ret;
    **/
}

void *palette_worker(void * arg){
    palette_worker_t * worker = arg;
    int ret = 0;

    mongoc_client_t *client;
    mongoc_collection_t *pixels_c;
    bson_error_t error;
    bson_t *query;
    bson_t *upsert;
    bson_t *color;
    mongoc_cursor_t *cursor;
    const bson_t *doc;

    //set as upsert
    bson_t *param = bson_new();
    BSON_APPEND_BOOL(param, "upsert", true);

    client = mongoc_client_pool_pop(worker->config->mongo_session.pool);

    pixels_c = mongoc_client_get_collection(client, worker->config->mongodb_database, "pixels");

    for (unsigned int x = worker->x[0]; x < worker->x[1] && ret == 0; x++){
        for (unsigned int y = worker->y[0]; y < worker->y[1] && ret == 0; y++){

            query = BCON_NEW(
                    "type", BCON_UTF8("pixel"),
                    "project", BCON_UTF8(worker->config->project_name),
                    "x", BCON_INT64(x),
                    "y", BCON_INT64(y),
                    "palette", BCON_NULL,
                    "color_id", BCON_NULL,
                    "color_state", BCON_NULL
            );

            cursor = mongoc_collection_find_with_opts(pixels_c, query, NULL, NULL);

            if (mongoc_cursor_next (cursor, &doc)) {
                unsigned char og_pixel[4] = {};
                bson_iter_t iter;
                bson_iter_t colors_iter;
                if(bson_iter_init_find(&iter,doc,"color") &&
                   BSON_ITER_HOLDS_ARRAY(&iter) && bson_iter_recurse (&iter, &colors_iter)){
                    char i = 0;
                    while (bson_iter_next (&colors_iter) && i < 4) {
                        og_pixel[i++] = bson_iter_int32_unsafe(&colors_iter);
                    }
                }else{
                    ret = 43;
                }

                mongoc_cursor_destroy(cursor);
                bson_destroy(query);

                if (ret==0) {
                    for (unsigned int color_id = 0; color_id < PALETTE_SIZE && ret == 0; color_id++) {
                        for (unsigned int color_shade = 0; color_shade < MULTIPLIER_SIZE && ret == 0; color_shade++) {
                            unsigned char dest_pixel[4] = {
                                    worker->palette->r[color_id][color_shade],
                                    worker->palette->g[color_id][color_shade],
                                    worker->palette->b[color_id][color_shade],
                                    255
                            };
                            if (color_id == 0){
                                dest_pixel[0] = og_pixel[0];
                                dest_pixel[1] = og_pixel[1];
                                dest_pixel[2] = og_pixel[2];
                                dest_pixel[3] = 0;
                            }

                            int dr = (dest_pixel[0] - og_pixel[0]);
                            int dg = (dest_pixel[1] - og_pixel[1]);
                            int db = (dest_pixel[2] - og_pixel[2]);
                            int da = (dest_pixel[3] - og_pixel[3]);

                            long distance = dr * dr + dg * dg + db * db + da * da;

                            //prepare query
                            query = BCON_NEW(
                                    "type", BCON_UTF8("pixel"),
                                    "project", BCON_UTF8(worker->config->project_name),
                                    "x", BCON_INT64(x),
                                    "y", BCON_INT64(y),
                                    "palette", BCON_UTF8(worker->palette->palette_name),
                                    "color_id", BCON_INT32(color_id),
                                    "color_state", BCON_INT32(color_shade)
                            );

                            color = BCON_NEW(
                                    "0", BCON_INT32(dest_pixel[0]),
                                    "1", BCON_INT32(dest_pixel[1]),
                                    "2", BCON_INT32(dest_pixel[2]),
                                    "3", BCON_INT32(dest_pixel[3])
                            );

                            upsert = BCON_NEW(
                                    "$set", "{",
                                        "color", BCON_ARRAY(color),
                                        "delta2", BCON_INT64(distance),
                                    "}"
                            );

                            if (!mongoc_collection_update_one(pixels_c, query, upsert, param, NULL, &error)) {
                                //TODO add error handling;
                                ret = 44;
                            }

                            worker->config->count++;

                            bson_destroy(upsert);
                            bson_destroy(color);
                            bson_destroy(query);
                        }
                    }
                }
            }
        }
    }
    bson_destroy(param);

    mongoc_collection_destroy (pixels_c);

    mongoc_client_pool_push(worker->config->mongo_session.pool, client);

    return (void *)ret;
}

int get_palette(main_options *config, command_options *local_config, mapart_palette *palette){
    int ret = 0;
    mongoc_client_t *client;
    mongoc_collection_t *collection;
    mongoc_cursor_t *cursor;
    const bson_t *doc;
    bson_t *query;
    char* str;

    //init palette to invalid values
    for (int i = 0; i< PALETTE_SIZE; i++){
        for (int j = 0; j < MULTIPLIER_SIZE; j++) {
            palette->r[i][j] = -255;
            palette->g[i][j] = -255;
            palette->b[i][j] = -255;
        }
    }

    int multipliers[MULTIPLIER_SIZE];

    printf("Loading palette\n");

    client = mongoc_client_pool_pop(config->mongo_session.pool);

    collection = mongoc_client_get_collection(client, config->mongodb_database, "config");

    query = bson_new();

    BSON_APPEND_UTF8(query, "type", "palette");
    BSON_APPEND_UTF8(query, "name", local_config->palette_name);

    cursor = mongoc_collection_find_with_opts(collection, query, NULL, NULL);

    if (mongoc_cursor_next (cursor, &doc)) {
        printf("Palette found, processing...\n");
        bson_iter_t iter;
        bson_iter_t colors_iter;
        bson_iter_t multiplier_iter;
        if(bson_iter_init_find(&iter,doc,"multipliers") &&
           BSON_ITER_HOLDS_ARRAY(&iter) && bson_iter_recurse (&iter, &multiplier_iter)){
            char i = 0;
            while (bson_iter_next (&multiplier_iter) && i < MULTIPLIER_SIZE) {
                multipliers[i++] = bson_iter_int32_unsafe(&multiplier_iter);
            }
        }
        //if object has color list
        if(bson_iter_init_find(&iter,doc,"colors") &&
            BSON_ITER_HOLDS_ARRAY(&iter) && bson_iter_recurse (&iter, &colors_iter)) {
            while (bson_iter_next (&colors_iter)) {
                bson_iter_t entry;
                bson_iter_recurse (&colors_iter, &entry);
                int color_id = -1;
                int rgb[RGB_SIZE] = {-255,-255,-255};
                if (bson_iter_find(&entry, "id") && BSON_ITER_HOLDS_INT32(&entry)){
                    color_id = bson_iter_int32_unsafe(&entry);
                }
                bson_iter_t rgb_iter;
                if (bson_iter_find(&entry, "color") && BSON_ITER_HOLDS_ARRAY(&entry) && bson_iter_recurse(&entry,&rgb_iter)) {
                    int index = 0;
                    while (bson_iter_next(&rgb_iter) && index < RGB_SIZE) {
                        rgb[index++] = bson_iter_int32_unsafe(&rgb_iter);
                    }
                }
                if (color_id >= 0 && color_id < PALETTE_SIZE){
                    //skip transparent id
                    if (color_id != 0){
                        // prepare the various multipliers
                        for (int i = 0; i < MULTIPLIER_SIZE; i++) {
                            palette->r[color_id][i] = (int)floor((double)rgb[0] * multipliers[i] / 255);
                            palette->g[color_id][i] = (int)floor((double)rgb[1] * multipliers[i] / 255);
                            palette->b[color_id][i] = (int)floor((double)rgb[2] * multipliers[i] / 255);
                        }
                    }
                }
            }
        }
        printf("Palette loaded\n\n");
    }else{
        fprintf (stderr,"Palette %s not Found!\n", local_config->palette_name);
        ret = 41;
    }

    bson_destroy (query);
    mongoc_cursor_destroy (cursor);
    mongoc_collection_destroy (collection);

    mongoc_client_pool_push(config->mongo_session.pool, client);

    return ret;
}