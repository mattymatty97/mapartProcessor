#include <unistd.h>
#include <getopt.h>
#include <pthread.h>
#include <math.h>
#include <libgen.h>

#include "../../libs/images/stb_image.h"
#include "../../libs/images/stb_image_write.h"
#include "../../libs/globaldefs.h"
#include "../../opencl/gpu.h"

#define PALETTE_SIZE 62
#define RGB_SIZE 3
#define RGBA_SIZE 4
#define MULTIPLIER_SIZE 4

typedef struct {
    char *image_filename;
    char *palette_name;
    char *color_space;
    char *dithering;
} command_options;

typedef struct {
    unsigned char *image_data;
    int x;
    int y;
    int channels;
} image_data;


typedef struct {
    int *image_data;
    int x;
    int y;
    int channels;
} image_int_data;

typedef struct {
    char * palette_name;
    int palette[PALETTE_SIZE][MULTIPLIER_SIZE][RGBA_SIZE];
} mapart_palette;

typedef enum {
  RGB,
  Lab,
  Lhc
} image_colorspace;

typedef enum {
  none,
  Floyd_Steinberg
} dither_algorithm;

void stbi_image_cleanup(image_data *image);

void image_int_cleanup(image_int_data ** image);
void palette_cleanup(mapart_palette ** palette);

int load_image(command_options *options, image_data *image);

int get_palette(main_options *config, command_options *local_config, mapart_palette *palette);

//-------------IMPLEMENTATIONS---------------------

int load_image_command(int argc, char **argv, main_options *config) {
    command_options local_config = {};
    int ret = 0;

    printf("\nLoad command start\n\n");
    static struct option long_options[] = {
            {"image",       required_argument, 0, 'i'},
            {"palette",     required_argument, 0, 'p'},
            {"color-space", required_argument, 0, 'c'},
            {"color-space", required_argument, 0, 's'},
            {"dithering",   required_argument, 0, 'd'}
    };

    int c;
    opterr = 0;
    int option_index = 0;
    while ((c = getopt_long(argc, argv, "+:i:p:c:s:d:", long_options, &option_index)) != -1) {
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

            case 'i':
                local_config.image_filename = strdup(optarg);
                break;

            case 'p':
                local_config.palette_name = strdup(optarg);
                break;

            case 'c':
            case 's':
                local_config.color_space = strdup(optarg);
                break;

            case 'd':
                local_config.dithering = strdup(optarg);
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

    if (local_config.image_filename == 0 || local_config.palette_name == 0 || local_config.color_space == 0 || local_config.dithering == 0) {
        printf("missing required options\n");
        return 11;
    }

    image_data image = {};

    image_int_data *int_image = calloc( 1, sizeof (image_int_data ));

    image_colorspace colorspace = RGB;

    dither_algorithm dither = none;

    mapart_palette palette = {};

    image_int_data *processed_image = NULL;
    mapart_palette *processed_palette = NULL;

    if (strcmp(local_config.color_space, "rgb") == 0){
        colorspace = RGB;
    }else if (strcmp(local_config.color_space, "lab") == 0){
        colorspace = Lab;
    }else if (strcmp(local_config.color_space, "lhc") == 0 ){
        colorspace = Lhc;
    }else{
        fprintf(stderr,"Not a valid colorspace %s", local_config.color_space);
        ret = 45;
    }

    if (ret == 0){
        if (strcmp(local_config.dithering, "none") == 0){
             dither = none;
        }else{
            fprintf(stderr,"Not a valid dither algorithm %s", local_config.dithering);
            ret = 46;
        }
    }

    if ( ret == 0) {
        ret = load_image(&local_config, &image);
    }

    //if everything is ok
    if (ret == 0) {
        //convert to int array for GPU compatibility

        int_image->image_data = calloc(image.x * image.y * image.channels, sizeof (int));
        int_image->x = image.x;
        int_image->y = image.y;
        int_image->channels = image.channels;

        for (int i =0 ; i< (image.x * image.y * image.channels); i++){
            int_image->image_data[i] = image.image_data[i];
        }

        //load image palette
        ret = get_palette(config, &local_config, &palette);
    }

    //if we're still fine
    if (ret == 0){
        //do color conversions
        if (colorspace == RGB){
            //if RGB do nothing (we're already rgb)
            processed_image = int_image;
            processed_palette = &palette;
        }else{
            //convert image to CIE-L*ab values + alpha
            image_int_data *Lab_image = calloc(1, sizeof (image_int_data));
            Lab_image->image_data = calloc( image.x * image.y * image.channels, sizeof (int));
            Lab_image->x = image.x;
            Lab_image->y = image.y;
            Lab_image->channels = image.channels;

            float* xyz_data = calloc( image.x * image.y * image.channels, sizeof (float));

            ret = gpu_rgb_to_xyz(&config->gpu, int_image->image_data, xyz_data, image.x, image.y, image.channels);

            if (ret == 0)
                ret = gpu_xyz_to_lab(&config->gpu, xyz_data, Lab_image->image_data, image.x, image.y, image.channels);

            free(xyz_data);

            image_int_cleanup(&int_image);
            processed_image = Lab_image;

            //convert palette to CIE-L*ab + alpha
            if (ret == 0){
                mapart_palette *Lab_palette = calloc( 1,sizeof (mapart_palette));
                Lab_palette->palette_name = palette.palette_name;


                float * palette_xyz_data = calloc( PALETTE_SIZE * MULTIPLIER_SIZE * RGBA_SIZE, sizeof (float));

                ret = gpu_rgb_to_xyz(&config->gpu, &palette.palette[0][0][0], palette_xyz_data, MULTIPLIER_SIZE, PALETTE_SIZE, RGBA_SIZE);

                if (ret == 0)
                    ret = gpu_xyz_to_lab(&config->gpu, palette_xyz_data, &Lab_palette->palette[0][0][0], MULTIPLIER_SIZE, PALETTE_SIZE, RGBA_SIZE);

                free(palette_xyz_data);

                processed_palette = Lab_palette;
            }


            if (ret == 0 && colorspace == Lhc){
                //convert to lhc

                image_int_data *Lhc_image = calloc(1, sizeof (image_int_data));
                Lhc_image->image_data = calloc(image.x * image.y * image.channels, sizeof (int));
                Lhc_image->x = image.x;
                Lhc_image->y = image.y;
                Lhc_image->channels = image.channels;

                ret = gpu_lab_to_lhc(&config->gpu, Lab_image->image_data, Lhc_image->image_data, image.x, image.y, image.channels);

                if (ret == 0){
                    image_int_cleanup(&processed_image);
                    processed_image = Lhc_image;
                }

                if (ret == 0){
                    mapart_palette *Lhc_palette = calloc(1, sizeof (mapart_palette));
                    Lhc_palette->palette_name = palette.palette_name;

                    ret = gpu_lab_to_lhc(&config->gpu, &processed_palette->palette[0][0][0], &Lhc_palette->palette[0][0][0], MULTIPLIER_SIZE, PALETTE_SIZE, RGBA_SIZE);

                    if (ret == 0){
                        palette_cleanup(&processed_palette);
                        processed_palette = Lhc_palette;
                    }
                }
            }

        }
    }
    unsigned char* dithered_image = NULL;
    //do dithering
    if (ret == 0){
        if (dither == none){
            dithered_image = calloc(image.x * image.y * 2,sizeof (unsigned char));
            ret = gpu_dither_none(&config->gpu, processed_image->image_data, &processed_palette->palette[0][0][0], dithered_image, image.x, image.y, image.channels,PALETTE_SIZE, MULTIPLIER_SIZE);
        }
    }

    //save result
    if (ret == 0){

        image_data converted_image = {NULL, image.x, image.y, 4};

        converted_image.image_data = calloc(image.x * image.y * 4,sizeof(unsigned char));

        ret = gpu_palette_to_rgb(&config->gpu, dithered_image, &palette.palette[0][0][0], converted_image.image_data, image.x, image.y,PALETTE_SIZE, MULTIPLIER_SIZE);

        if (ret == 0) {
            char filename[1000] = {};
            sprintf(filename, "%s_%s_%s.png", config->project_name, local_config.color_space, local_config.dithering);
            ret = stbi_write_png(filename, converted_image.x, converted_image.y, converted_image.channels, converted_image.image_data, 0);
            if (ret == 0){
                fprintf(stderr, "Failed to save image %s:\n%s\n", filename, stbi_failure_reason());
                ret = 13;
            }else{
                ret = 0;
            }
        }

        free(converted_image.image_data);
    }

    if (dithered_image != NULL)
        free(dithered_image);
    image_int_cleanup(&processed_image);
    if (processed_palette != &palette)
        palette_cleanup(&processed_palette);
    stbi_image_cleanup(&image);
    return ret;
}

int load_image(command_options *options, image_data *image) {
    printf("Loading image\n");
    //load the image
    if (access(options->image_filename, F_OK) == 0 && access(options->image_filename, R_OK) == 0) {
        image->image_data = stbi_load(options->image_filename, &image->x, &image->y, &image->channels, 4);
        if (image->image_data == NULL) {
            fprintf(stderr, "Failed to load image %s:\n%s\n", options->image_filename, stbi_failure_reason());
            return 13;
        }
        image->channels = 4;
        printf("Image loaded: %dx%d(%d)\n\n", image->x, image->y, image->channels);
        return 0;
    } else {
        fprintf(stderr, "Failed to load image %s:\nFile does not exists\n", options->image_filename);
        return 12;
    }
}

int get_palette(main_options *config, command_options *local_config, mapart_palette *palette){
    int ret = 0;
    mongoc_client_t *client;
    mongoc_collection_t *collection;
    mongoc_cursor_t *cursor;
    const bson_t *doc;
    bson_t *query;
    char* str;

    //init palette
    for (int i = 0; i< PALETTE_SIZE; i++){
        for (int j = 0; j < MULTIPLIER_SIZE; j++) {
            palette->palette[i][j][0] = -255;
            palette->palette[i][j][1] = -255;
            palette->palette[i][j][2] = -255;
            //init transparency
            palette->palette[i][j][3] = 255 * (i != 0);
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
                            palette->palette[color_id][i][0] = (int)floor((double)rgb[0] * multipliers[i] / 255);
                            palette->palette[color_id][i][1] = (int)floor((double)rgb[1] * multipliers[i] / 255);
                            palette->palette[color_id][i][2] = (int)floor((double)rgb[2] * multipliers[i] / 255);
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

void stbi_image_cleanup(image_data *image) {
    if (image->image_data != NULL)
        stbi_image_free(image->image_data);
}

void image_int_cleanup(image_int_data ** image){
    if (*image != NULL) {
        if ((*image)->image_data != NULL)
            free((*image)->image_data);
        free (*image);
        *image = NULL;
    }
}

void palette_cleanup(mapart_palette ** palette){
    if (*palette != NULL) {
        free (*palette);
        *palette = NULL;
    }
}