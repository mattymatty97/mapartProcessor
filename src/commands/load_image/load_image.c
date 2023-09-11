#include <unistd.h>
#include <getopt.h>
#include <math.h>

#include "../../libs/images/stb_image.h"
#include "../../libs/images/stb_image_write.h"
#include "../../libs/globaldefs.h"
#include "../../opencl/gpu.h"

#define PALETTE_SIZE 62
#define RGB_SIZE 3
#define RGBA_SIZE 4

#ifdef MAPART_SURVIVAL
#define MULTIPLIER_SIZE 3
#define BUILD_LIMIT 258
#else
#define MULTIPLIER_SIZE 4
#define BUILD_LIMIT 2000
#endif


typedef struct {
    char *image_filename;
    char *palette_name;
    unsigned int random_seed;
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
    float *image_data;
    int x;
    int y;
    int channels;
} image_float_data;

typedef struct {
    char * palette_name;
    int palette[PALETTE_SIZE][MULTIPLIER_SIZE][RGBA_SIZE];
} mapart_palette;

typedef struct {
    char * palette_name;
    float palette[PALETTE_SIZE][MULTIPLIER_SIZE][RGBA_SIZE];
} mapart_float_palette;


typedef enum {
  none,
  Floyd_Steinberg,
  JJND,
  Stucki,
  Atkinson,
  Burkes,
  Sierra,
  Sierra2,
  SierraL
} dither_algorithm;

void stbi_image_cleanup(image_data *image);

void image_int_cleanup(image_int_data ** image);
void palette_cleanup(mapart_palette ** palette);
void image_float_cleanup(image_float_data ** image);
void float_palette_cleanup(mapart_float_palette ** palette);

int load_image(command_options *options, image_data *image);

int get_palette(main_options *config, command_options *local_config, mapart_palette *palette);

//-------------IMPLEMENTATIONS---------------------

unsigned int str_hash(const char* word)
{
    unsigned int hash = 0;
    for (int i = 0 ; word[i] != '\0' ; i++)
    {
        hash = 31*hash + word[i];
    }
    return hash;
}

int load_image_command(int argc, char **argv, main_options *config) {
    command_options local_config = {};
    local_config.random_seed = str_hash("seed string");

    int ret = 0;

    printf("\nLoad command start\n\n");
    static struct option long_options[] = {
            {"image",       required_argument, 0, 'i'},
            {"palette",     required_argument, 0, 'p'},
            {"random",     required_argument, 0, 'r'},
            {"random-seed",     required_argument, 0, 'r'},
            {"dithering",   required_argument, 0, 'd'}
    };

    int c;
    opterr = 0;
    int option_index = 0;
    while ((c = getopt_long(argc, argv, "+:i:p:d:r:", long_options, &option_index)) != -1) {
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

            case 'd':
                local_config.dithering = strdup(optarg);
                break;

            case 'r':
                local_config.random_seed = str_hash(optarg);
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

    if (local_config.image_filename == 0 || local_config.palette_name == 0 || local_config.dithering == 0) {
        printf("missing required options\n");
        return 11;
    }

    image_data image = {};

    image_int_data *int_image = calloc( 1, sizeof (image_int_data ));
    image_float_data *float_image = calloc( 1, sizeof (image_float_data ));

    dither_algorithm dither = none;

    mapart_palette palette = {};

    image_float_data *processed_image = NULL;
    mapart_float_palette *processed_palette = NULL;

    if (ret == 0){
        if (strcmp(local_config.dithering, "none") == 0){
             dither = none;
        }else if ( ( strcmp(local_config.dithering, "floyd") == 0 ) || ( strcmp(local_config.dithering, "floyd_steinberg") == 0 ) ){
            dither = Floyd_Steinberg;
        } else if ( ( strcmp(local_config.dithering, "jjnd") == 0 ) ){
            dither = JJND;
        } else if ( ( strcmp(local_config.dithering, "stucki") == 0 ) ){
            dither = Stucki;
        } else if ( ( strcmp(local_config.dithering, "atkinson") == 0 ) ){
            dither = Atkinson;
        } else if ( ( strcmp(local_config.dithering, "burkes") == 0 ) ){
            dither = Burkes;
        } else if ( ( strcmp(local_config.dithering, "sierra") == 0 ) ){
            dither = Sierra;
        } else if ( ( strcmp(local_config.dithering, "sierra2") == 0 ) ){
            dither = Sierra2;
        } else if ( ( strcmp(local_config.dithering, "sierraL") == 0 ) ){
            dither = SierraL;
        } else {
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

        //convert to float array for GPU compatibility
        float_image->image_data = calloc(image.x * image.y * image.channels, sizeof (float));
        float_image->x = image.x;
        float_image->y = image.y;
        float_image->channels = image.channels;

        for (int i =0 ; i< (image.x * image.y * image.channels); i++){
            int_image->image_data[i] = image.image_data[i];
        }

        for (int i =0 ; i< (image.x * image.y * image.channels); i++){
            float_image->image_data[i] = image.image_data[i];
        }

        //load image palette
        ret = get_palette(config, &local_config, &palette);

    }

    //if we're still fine
    if (ret == 0){
        //do color conversions
        //convert image to CIE-L*ab values + alpha
        image_float_data *Lab_image = calloc(1, sizeof (image_float_data));
        Lab_image->image_data = calloc( image.x * image.y * image.channels, sizeof (float));
        Lab_image->x = image.x;
        Lab_image->y = image.y;
        Lab_image->channels = image.channels;

        float* xyz_data = calloc( image.x * image.y * image.channels, sizeof (float));

        fprintf(stdout,"Converting image to XYZ\n");
        fflush(stdout);
        ret = gpu_rgb_to_xyz(&config->gpu, int_image->image_data, xyz_data, image.x, image.y);

        if (ret == 0) {
            fprintf(stdout,"Converting image to CIE-L*ab\n");
            fflush(stdout);
            ret = gpu_xyz_to_lab(&config->gpu, xyz_data, Lab_image->image_data, image.x, image.y);
        }
        free(xyz_data);

        image_int_cleanup(&int_image);
        processed_image = Lab_image;

        //convert palette to CIE-L*ab + alpha
        if (ret == 0){
            mapart_float_palette *Lab_palette = calloc( 1,sizeof (mapart_float_palette));
            Lab_palette->palette_name = palette.palette_name;


            float * palette_xyz_data = calloc( PALETTE_SIZE * MULTIPLIER_SIZE * RGBA_SIZE, sizeof (float));
            fprintf(stdout,"Converting palette to XYZ\n");
            fflush(stdout);
            ret = gpu_rgb_to_xyz(&config->gpu, &palette.palette[0][0][0], palette_xyz_data, MULTIPLIER_SIZE, PALETTE_SIZE);

            if (ret == 0) {
                fprintf(stdout,"Converting palette to CIE-L*ab\n");
                fflush(stdout);
                ret = gpu_xyz_to_lab(&config->gpu, palette_xyz_data, &Lab_palette->palette[0][0][0],
                                     MULTIPLIER_SIZE, PALETTE_SIZE);
            }
            free(palette_xyz_data);

            processed_palette = Lab_palette;
        }
    }
    unsigned char* dithered_image = NULL;
    //do dithering
    if (ret == 0){
        fprintf(stdout,"Generate noise image\n");
        fflush(stdout);
        float *noise = calloc( image.x * image.y, sizeof (float));

        srand(local_config.random_seed);

        for (int i = 0; i < image.x * image.y; i++)
            noise[i] = (float)rand()/(float)RAND_MAX;
            //noise[i] = 1;

        fprintf(stdout,"Do image dithering\n");
        fflush(stdout);
        if (dither == none){
            dithered_image = calloc(image.x * image.y * 2,sizeof (unsigned char));
            ret = gpu_dither_none(&config->gpu, processed_image->image_data,  dithered_image, &processed_palette->palette[0][0][0], noise, image.x, image.y,PALETTE_SIZE, MULTIPLIER_SIZE, BUILD_LIMIT);
        } else if (dither == Floyd_Steinberg){
            dithered_image = calloc(image.x * image.y * 2,sizeof (unsigned char));
            ret = gpu_dither_floyd_steinberg(&config->gpu, processed_image->image_data, dithered_image,&processed_palette->palette[0][0][0], noise, image.x, image.y,PALETTE_SIZE, MULTIPLIER_SIZE, BUILD_LIMIT);
        } else if (dither == JJND){
            dithered_image = calloc(image.x * image.y * 2,sizeof (unsigned char));
            ret = gpu_dither_JJND(&config->gpu, processed_image->image_data, dithered_image,&processed_palette->palette[0][0][0], noise, image.x, image.y,PALETTE_SIZE, MULTIPLIER_SIZE, BUILD_LIMIT);
        } else if (dither == Stucki){
            dithered_image = calloc(image.x * image.y * 2,sizeof (unsigned char));
            ret = gpu_dither_Stucki(&config->gpu, processed_image->image_data, dithered_image,&processed_palette->palette[0][0][0], noise, image.x, image.y,PALETTE_SIZE, MULTIPLIER_SIZE, BUILD_LIMIT);
        } else if (dither == Atkinson){
            dithered_image = calloc(image.x * image.y * 2,sizeof (unsigned char));
            ret = gpu_dither_Atkinson(&config->gpu, processed_image->image_data, dithered_image,&processed_palette->palette[0][0][0], noise, image.x, image.y,PALETTE_SIZE, MULTIPLIER_SIZE, BUILD_LIMIT);
        } else if (dither == Burkes){
            dithered_image = calloc(image.x * image.y * 2,sizeof (unsigned char));
            ret = gpu_dither_Burkes(&config->gpu, processed_image->image_data, dithered_image,&processed_palette->palette[0][0][0], noise, image.x, image.y,PALETTE_SIZE, MULTIPLIER_SIZE, BUILD_LIMIT);
        } else if (dither == Sierra){
            dithered_image = calloc(image.x * image.y * 2,sizeof (unsigned char));
            ret = gpu_dither_Sierra(&config->gpu, processed_image->image_data, dithered_image,&processed_palette->palette[0][0][0], noise, image.x, image.y,PALETTE_SIZE, MULTIPLIER_SIZE, BUILD_LIMIT);
        } else if (dither == Sierra2){
            dithered_image = calloc(image.x * image.y * 2,sizeof (unsigned char));
            ret = gpu_dither_Sierra2(&config->gpu, processed_image->image_data, dithered_image,&processed_palette->palette[0][0][0], noise, image.x, image.y,PALETTE_SIZE, MULTIPLIER_SIZE, BUILD_LIMIT);
        } else if (dither == SierraL){
            dithered_image = calloc(image.x * image.y * 2,sizeof (unsigned char));
            ret = gpu_dither_SierraL(&config->gpu, processed_image->image_data, dithered_image,&processed_palette->palette[0][0][0], noise, image.x, image.y,PALETTE_SIZE, MULTIPLIER_SIZE, BUILD_LIMIT);
        }
        free(noise);
    }

    //save result
    if (ret == 0){

        image_data converted_image = {NULL, image.x, image.y, 4};

        converted_image.image_data = calloc(image.x * image.y * 4,sizeof(unsigned char));

        fprintf(stdout,"Convert dithered image back to rgb\n");
        fflush(stdout);
        ret = gpu_palette_to_rgb(&config->gpu, dithered_image, &palette.palette[0][0][0], converted_image.image_data, image.x, image.y,PALETTE_SIZE, MULTIPLIER_SIZE);

        if (ret == 0) {
            char filename[1000] = {};
            sprintf(filename, "images/%s_%s.png", config->project_name, local_config.dithering);
            fprintf(stdout,"Save image\n");
            fflush(stdout);
            ret = stbi_write_png(filename, converted_image.x, converted_image.y, converted_image.channels, converted_image.image_data, 0);
            if (ret == 0){
                fprintf(stderr, "Failed to save image %s:\n%s\n", filename, stbi_failure_reason());
                ret = 13;
            }else{
                fprintf(stdout,"Image saved: %s\n", filename);
                fflush(stdout);
                ret = 0;
            }
        }

        free(converted_image.image_data);
    }

    if (dithered_image != NULL)
        free(dithered_image);
    image_float_cleanup(&processed_image);
    if (processed_palette != NULL)
        float_palette_cleanup(&processed_palette);
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

void image_float_cleanup(image_float_data ** image){
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
void float_palette_cleanup(mapart_float_palette ** palette){
    if (*palette != NULL) {
        free (*palette);
        *palette = NULL;
    }
}