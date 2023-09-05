#include <unistd.h>
#include <getopt.h>
#include <math.h>

#include "../../libs/images/stb_image.h"
#include "../../libs/globaldefs.h"

#define PALETTE_SIZE 62
#define RGB_SIZE 3
#define MULTIPLIER_SIZE 4

typedef struct {
    char* image_filename;
    char* palette_name;
} command_options;

typedef struct {
    int r[PALETTE_SIZE][MULTIPLIER_SIZE];
    int g[PALETTE_SIZE][MULTIPLIER_SIZE];
    int b[PALETTE_SIZE][MULTIPLIER_SIZE];
} mapart_palette;

typedef struct {
    unsigned char* image_data;
    int x;
    int y;
    int channels;
} image_data;

void load_cleanup(image_data* image);

int get_palette(main_options *config, command_options *local_config, mapart_palette *palette);

int load_image(command_options *options, image_data* image);

int load_command(int argc, char** argv, main_options *config){
    command_options local_config = {};
    int ret = 0;

    printf("\nLoad command start\n\n");
    static struct option long_options[] = {
            {"image", required_argument, 0, 'i'},
            {"palette", required_argument, 0, 'p'}
    };

    int c;
    opterr = 0;
    int option_index = 0;
    while ((c = getopt_long (argc, argv, "+:i:p:", long_options, &option_index)) != -1){
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

            case 'i':
                local_config.image_filename = strdup(optarg);
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

    if (local_config.palette_name == 0 || local_config.image_filename == 0){
        printf("missing required options\n");
        return 4;
    }

    mapart_palette palette= {};
    image_data image = {};


    ret = get_palette(config, &local_config, &palette);

    //if everything is ok
    if (ret == 0){
        ret = load_image(&local_config, &image);
    }

    load_cleanup(&image);
    return ret;
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

    BSON_APPEND_UTF8(query, "id", "palette");
    BSON_APPEND_UTF8(query, "name", (*local_config).palette_name);

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
        ret = 9;
    }

    bson_destroy (query);
    mongoc_cursor_destroy (cursor);
    mongoc_collection_destroy (collection);

    mongoc_client_pool_push(config->mongo_session.pool, client);

    return ret;
}

int load_image(command_options *options, image_data* image){
    printf("Loading image\n");
    //load the image
    if(access(options->image_filename, F_OK) == 0 && access(options->image_filename, R_OK) == 0) {
        image->image_data = stbi_load(options->image_filename, &image->x, &image->y, &image->channels, 0);
        if (image->image_data == NULL){
            fprintf (stderr,"Failed to load image %s:\n%s\n", options->image_filename, stbi_failure_reason());
            return 10;
        }
        printf("Image loaded: %dx%d(%d)\n\n", image->x, image->y, image->channels);
    }else{
        fprintf (stderr,"Failed to load image %s:\nFile does not exists\n", options->image_filename);
        return 10;
    }
    return 0;
}

void load_cleanup(image_data* image){
    if (image->image_data != NULL)
        stbi_image_free(image->image_data);
}