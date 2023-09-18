#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <math.h>

#define STB_IMAGE_IMPLEMENTATION

#include "libs/images/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "libs/images/stb_image_write.h"
#include "libs/globaldefs.h"
#include "opencl/gpu.h"


#if defined(__WIN32__) || defined(__WIN64__) || defined(__WINNT__)

#include <windows.h>

unsigned long get_processor_count() {
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

//--------------CONSTANTS-------------------

static struct option long_options[] = {
        {"project-name",   required_argument, 0, 'n'},
        {"threads",        required_argument, 0, 't'},
        {"image",          required_argument, 0, 'i'},
        {"palette",        required_argument, 0, 'p'},
        {"random",         required_argument, 0, 'r'},
        {"random-seed",    required_argument, 0, 'r'},
        {"maximum-height", required_argument, 0, 'h'},
        {"dithering",      required_argument, 0, 'd'}
};

main_options config ={};

#define PALETTE_SIZE 62
#define RGB_SIZE 3
#define RGBA_SIZE 4

#ifdef MAPART_SURVIVAL
#define MULTIPLIER_SIZE 3
#define BUILD_LIMIT -1
#else
#define MULTIPLIER_SIZE 4
#define BUILD_LIMIT -1
#endif

//----------------DEFINITIONS---------------

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
    char *palette_name;
    int palette[PALETTE_SIZE][MULTIPLIER_SIZE][RGBA_SIZE];
} mapart_palette;

typedef struct {
    char *palette_name;
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

void image_cleanup(image_data *image);

void image_int_cleanup(image_int_data **image);

void palette_cleanup(mapart_palette **palette);

void image_float_cleanup(image_float_data **image);

void float_palette_cleanup(mapart_float_palette **palette);

int load_image(image_data *image);

int save_image(mapart_palette *palette, image_data *dither_image);

int get_palette(mapart_palette *palette);

//-------------IMPLEMENTATIONS---------------------

unsigned int str_hash(const char *word) {
    unsigned int hash = 0;
    for (int i = 0; word[i] != '\0'; i++) {
        hash = 31 * hash + word[i];
    }
    return hash;
}

int main(int argc, char **argv) {
    config.random_seed = str_hash("seed string");
    config.maximum_height = BUILD_LIMIT;

    int ret = 0;

    int c;
    opterr = 0;
    
    config.threads = get_processor_count();
    unsigned long thread_count;
    
    int option_index = 0;
    while ((c = getopt_long(argc, argv, "+:i:p:d:r:h:n:t:", long_options, &option_index)) != -1) {
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
                
            case 'n':
                config.project_name = strdup(optarg);
                break;

            case 't':
                thread_count = strtol(optarg, NULL, 10);
                if (thread_count > 0)
                    config.threads = thread_count;
                break;

            case 'i':
                config.image_filename = strdup(optarg);
                break;

            case 'p':
                config.palette_name = strdup(optarg);
                break;

            case 'd':
                config.dithering = strdup(optarg);
                break;

            case 'r':
                config.random_seed = str_hash(optarg);
                break;

            case 'h': {
                unsigned int height = atoi(optarg);
                if (height != 1 && height != 2)
                    config.maximum_height = height;
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

    if (config.project_name == 0 || config.image_filename == 0 || config.palette_name == 0 || config.dithering == 0) {
        printf("missing required options\n");
        return 11;
    }

    ret = gpu_init(&config, &config.gpu);

    image_data image = {};

    image_int_data *int_image = calloc(1, sizeof(image_int_data));
    image_float_data *float_image = calloc(1, sizeof(image_float_data));

    dither_algorithm dither = none;

    mapart_palette palette = {};

    image_float_data *processed_image = NULL;
    mapart_float_palette *processed_palette = NULL;

    if (ret == 0) {
        if (strcmp(config.dithering, "none") == 0) {
            dither = none;
        } else if ((strcmp(config.dithering, "floyd") == 0) ||
                   (strcmp(config.dithering, "floyd_steinberg") == 0)) {
            dither = Floyd_Steinberg;
        } else if ((strcmp(config.dithering, "jjnd") == 0)) {
            dither = JJND;
        } else if ((strcmp(config.dithering, "stucki") == 0)) {
            dither = Stucki;
        } else if ((strcmp(config.dithering, "atkinson") == 0)) {
            dither = Atkinson;
        } else if ((strcmp(config.dithering, "burkes") == 0)) {
            dither = Burkes;
        } else if ((strcmp(config.dithering, "sierra") == 0)) {
            dither = Sierra;
        } else if ((strcmp(config.dithering, "sierra2") == 0)) {
            dither = Sierra2;
        } else if ((strcmp(config.dithering, "sierraL") == 0)) {
            dither = SierraL;
        } else {
            fprintf(stderr, "Not a valid dither algorithm %s", config.dithering);
            ret = 46;
        }
    }

    if (ret == 0) {
        ret = load_image(&image);
    }

    //if everything is ok
    if (ret == 0) {
        //convert to int array for GPU compatibility
        int_image->image_data = calloc(image.x * image.y * image.channels, sizeof(int));
        int_image->x = image.x;
        int_image->y = image.y;
        int_image->channels = image.channels;

        for (int i = 0; i < (image.x * image.y * image.channels); i++) {
            int_image->image_data[i] = image.image_data[i];
        }

        //convert to float array for GPU compatibility
        float_image->image_data = calloc(image.x * image.y * image.channels, sizeof(float));
        float_image->x = image.x;
        float_image->y = image.y;
        float_image->channels = image.channels;

        for (int i = 0; i < (image.x * image.y * image.channels); i++) {
            int_image->image_data[i] = image.image_data[i];
        }

        for (int i = 0; i < (image.x * image.y * image.channels); i++) {
            float_image->image_data[i] = image.image_data[i];
        }

        //load image palette
        ret = get_palette(&palette);

    }

    //if we're still fine
    if (ret == 0) {
        //convert image to CIE-L*ab values + alpha
        image_float_data *Lab_image = calloc(1, sizeof(image_float_data));
        Lab_image->image_data = calloc(image.x * image.y * image.channels, sizeof(float));
        Lab_image->x = image.x;
        Lab_image->y = image.y;
        Lab_image->channels = image.channels;

        float *xyz_data = calloc(image.x * image.y * image.channels, sizeof(float));

        fprintf(stdout, "Converting image to XYZ\n");
        fflush(stdout);
        ret = gpu_rgb_to_xyz(&config.gpu, int_image->image_data, xyz_data, image.x, image.y);

        if (ret == 0) {
            fprintf(stdout, "Converting image to CIE-L*ab\n");
            fflush(stdout);
            ret = gpu_xyz_to_lab(&config.gpu, xyz_data, Lab_image->image_data, image.x, image.y);
        }
        free(xyz_data);

        image_int_cleanup(&int_image);
        processed_image = Lab_image;

        //convert palette to CIE-L*ab + alpha
        if (ret == 0) {
            mapart_float_palette *Lab_palette = calloc(1, sizeof(mapart_float_palette));
            Lab_palette->palette_name = palette.palette_name;


            float *palette_xyz_data = calloc(PALETTE_SIZE * MULTIPLIER_SIZE * RGBA_SIZE, sizeof(float));
            fprintf(stdout, "Converting palette to XYZ\n");
            fflush(stdout);
            ret = gpu_rgb_to_xyz(&config.gpu, &palette.palette[0][0][0], palette_xyz_data, MULTIPLIER_SIZE,
                                 PALETTE_SIZE);

            if (ret == 0) {
                fprintf(stdout, "Converting palette to CIE-L*ab\n");
                fflush(stdout);
                ret = gpu_xyz_to_lab(&config.gpu, palette_xyz_data, &Lab_palette->palette[0][0][0],
                                     MULTIPLIER_SIZE, PALETTE_SIZE);
            }
            free(palette_xyz_data);

            processed_palette = Lab_palette;
        }
    }

    image_data dithered_image = {
            NULL,
            image.x,
            image.y,
            2
    };
    //do dithering
    if (ret == 0) {
        fprintf(stdout, "Generate noise image\n");
        fflush(stdout);
        float *noise = calloc(image.x * image.y, sizeof(float));

        srand(config.random_seed);

        for (int i = 0; i < image.x * image.y; i++)
            noise[i] = (float) rand() / (float) RAND_MAX;
        //noise[i] = 1;

        fprintf(stdout, "Do image dithering\n");
        fflush(stdout);
        if (dither == none) {
            dithered_image.image_data = calloc(image.x * image.y * 2, sizeof(unsigned char));
            ret = gpu_dither_none(&config.gpu, processed_image->image_data, dithered_image.image_data,
                                  &processed_palette->palette[0][0][0], noise, image.x, image.y, PALETTE_SIZE,
                                  MULTIPLIER_SIZE, config.maximum_height);
        } else if (dither == Floyd_Steinberg) {
            dithered_image.image_data = calloc(image.x * image.y * 2, sizeof(unsigned char));
            ret = gpu_dither_floyd_steinberg(&config.gpu, processed_image->image_data, dithered_image.image_data,
                                             &processed_palette->palette[0][0][0], noise, image.x, image.y,
                                             PALETTE_SIZE, MULTIPLIER_SIZE, config.maximum_height);
        } else if (dither == JJND) {
            dithered_image.image_data = calloc(image.x * image.y * 2, sizeof(unsigned char));
            ret = gpu_dither_JJND(&config.gpu, processed_image->image_data, dithered_image.image_data,
                                  &processed_palette->palette[0][0][0], noise, image.x, image.y, PALETTE_SIZE,
                                  MULTIPLIER_SIZE, config.maximum_height);
        } else if (dither == Stucki) {
            dithered_image.image_data = calloc(image.x * image.y * 2, sizeof(unsigned char));
            ret = gpu_dither_Stucki(&config.gpu, processed_image->image_data, dithered_image.image_data,
                                    &processed_palette->palette[0][0][0], noise, image.x, image.y, PALETTE_SIZE,
                                    MULTIPLIER_SIZE, config.maximum_height);
        } else if (dither == Atkinson) {
            dithered_image.image_data = calloc(image.x * image.y * 2, sizeof(unsigned char));
            ret = gpu_dither_Atkinson(&config.gpu, processed_image->image_data, dithered_image.image_data,
                                      &processed_palette->palette[0][0][0], noise, image.x, image.y, PALETTE_SIZE,
                                      MULTIPLIER_SIZE, config.maximum_height);
        } else if (dither == Burkes) {
            dithered_image.image_data = calloc(image.x * image.y * 2, sizeof(unsigned char));
            ret = gpu_dither_Burkes(&config.gpu, processed_image->image_data, dithered_image.image_data,
                                    &processed_palette->palette[0][0][0], noise, image.x, image.y, PALETTE_SIZE,
                                    MULTIPLIER_SIZE, config.maximum_height);
        } else if (dither == Sierra) {
            dithered_image.image_data = calloc(image.x * image.y * 2, sizeof(unsigned char));
            ret = gpu_dither_Sierra(&config.gpu, processed_image->image_data, dithered_image.image_data,
                                    &processed_palette->palette[0][0][0], noise, image.x, image.y, PALETTE_SIZE,
                                    MULTIPLIER_SIZE, config.maximum_height);
        } else if (dither == Sierra2) {
            dithered_image.image_data = calloc(image.x * image.y * 2, sizeof(unsigned char));
            ret = gpu_dither_Sierra2(&config.gpu, processed_image->image_data, dithered_image.image_data,
                                     &processed_palette->palette[0][0][0], noise, image.x, image.y, PALETTE_SIZE,
                                     MULTIPLIER_SIZE, config.maximum_height);
        } else if (dither == SierraL) {
            dithered_image.image_data = calloc(image.x * image.y * 2, sizeof(unsigned char));
            ret = gpu_dither_SierraL(&config.gpu, processed_image->image_data, dithered_image.image_data,
                                     &processed_palette->palette[0][0][0], noise, image.x, image.y, PALETTE_SIZE,
                                     MULTIPLIER_SIZE, config.maximum_height);
        }
        free(noise);
    }

    //save result
    if (ret == 0) {
        ret = save_image(&palette, &dithered_image);
    }
    image_float_cleanup(&processed_image);
    if (processed_palette != NULL)
        float_palette_cleanup(&processed_palette);
    image_cleanup(&image);
    image_cleanup(&dithered_image);

    gpu_clear(&config.gpu);
    return ret;
}

int load_image(image_data *image) {
    printf("Loading image\n");
    //load the image
    if (access(config.image_filename, F_OK) == 0 && access(config.image_filename, R_OK) == 0) {
        image->image_data = stbi_load(config.image_filename, &image->x, &image->y, &image->channels, 4);
        if (image->image_data == NULL) {
            fprintf(stderr, "Failed to load image %s:\n%s\n", config.image_filename, stbi_failure_reason());
            return 13;
        }
        image->channels = 4;
        printf("Image loaded: %dx%d(%d)\n\n", image->x, image->y, image->channels);
        return 0;
    } else {
        fprintf(stderr, "Failed to load image %s:\nFile does not exists\n", config.image_filename);
        return 12;
    }
}

int save_image(mapart_palette *palette, image_data *dither_image) {
    int ret = 0;
    image_data converted_image = {NULL, dither_image->x, dither_image->y, 4};

    converted_image.image_data = calloc(dither_image->x * dither_image->y * 4, sizeof(unsigned char));

    fprintf(stdout, "Convert dithered image back to rgb\n");
    fflush(stdout);
    ret = gpu_palette_to_rgb(&config.gpu, dither_image->image_data, &palette->palette[0][0][0],
                             converted_image.image_data, dither_image->x, dither_image->y, PALETTE_SIZE,
                             MULTIPLIER_SIZE);

    char filename[1000] = "images/";
    char filename2[1000] = "images/";
    if (ret == 0) {
        char *appendix = "";

        if (config.maximum_height == 0)
            appendix = strdup("_flat");
        else if (config.maximum_height < 0)
            appendix = strdup("_unlimited");
        else {
            char smallbuff[20] = {};
            sprintf(smallbuff, "_%d", config.maximum_height);
            appendix = strdup(smallbuff);
        }

        sprintf(&filename[7], "%s_%s%s.png", config.project_name, config.dithering, appendix);
        sprintf(&filename2[7], "%s_%s%s.mapart", config.project_name, config.dithering, appendix);

        free(appendix);

        fprintf(stdout, "Save image\n");
        fflush(stdout);
        ret = stbi_write_png(filename, converted_image.x, converted_image.y, converted_image.channels,
                             converted_image.image_data, 0);
        if (ret == 0) {
            fprintf(stderr, "Failed to save image %s:\n%s\n", filename, stbi_failure_reason());
            ret = 13;
        } else {
            fprintf(stdout, "Image saved: %s\n", filename);
            fflush(stdout);
            ret = 0;
        }
    }

    free(converted_image.image_data);
    return ret;
}

#include <bson.h>

int get_palette(mapart_palette *palette) {
    int ret = 0;
    bson_t *query;
    bson_error_t error = {};
    char *str;

    //init palette
    for (int i = 0; i < PALETTE_SIZE; i++) {
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

    bson_json_reader_t *reader = bson_json_reader_new_from_file(config.palette_name, &error);
    if (reader != NULL) {
        bool eof = 0;
        bson_t doc = BSON_INITIALIZER;
        ret = bson_json_reader_read(reader, &doc, &error) <= 0;
        if (ret != 0){
            fprintf(stderr, "Error reading bson: \n%s\n", error.message);
            ret = 2;
        }

        if (ret == 0) {
            printf("Palette found, processing...\n");
            bson_iter_t iter;
            bson_iter_t colors_iter;
            bson_iter_t multiplier_iter;
            if (bson_iter_init_find(&iter, &doc, "multipliers") &&
                BSON_ITER_HOLDS_ARRAY(&iter) && bson_iter_recurse(&iter, &multiplier_iter)) {
                char i = 0;
                while (bson_iter_next(&multiplier_iter) && i < MULTIPLIER_SIZE) {
                    multipliers[i++] = bson_iter_int32_unsafe(&multiplier_iter);
                }
            }
            //if object has color list
            if (bson_iter_init_find(&iter, &doc, "colors") &&
                BSON_ITER_HOLDS_ARRAY(&iter) && bson_iter_recurse(&iter, &colors_iter)) {
                while (bson_iter_next(&colors_iter)) {
                    bson_iter_t entry;
                    bson_iter_recurse(&colors_iter, &entry);
                    int color_id = -1;
                    int rgb[RGB_SIZE] = {-255, -255, -255};
                    if (bson_iter_find(&entry, "id") && BSON_ITER_HOLDS_INT32(&entry)) {
                        color_id = bson_iter_int32_unsafe(&entry);
                    }
                    bson_iter_t rgb_iter;
                    if (bson_iter_find(&entry, "color") && BSON_ITER_HOLDS_ARRAY(&entry) &&
                        bson_iter_recurse(&entry, &rgb_iter)) {
                        int index = 0;
                        while (bson_iter_next(&rgb_iter) && index < RGB_SIZE) {
                            rgb[index++] = bson_iter_int32_unsafe(&rgb_iter);
                        }
                    }
                    if (color_id >= 0 && color_id < PALETTE_SIZE) {
                        //skip transparent id
                        if (color_id != 0) {
                            // prepare the various multipliers
                            for (int i = 0; i < MULTIPLIER_SIZE; i++) {
                                palette->palette[color_id][i][0] = (int) floor((double) rgb[0] * multipliers[i] / 255);
                                palette->palette[color_id][i][1] = (int) floor((double) rgb[1] * multipliers[i] / 255);
                                palette->palette[color_id][i][2] = (int) floor((double) rgb[2] * multipliers[i] / 255);
                            }
                        }
                    }
                }
            }
            printf("Palette loaded\n\n");
        }
        bson_json_reader_destroy(reader);
    }else{
        fprintf(stderr, "Error opening palette file %s:\n%s\n", config.palette_name, error.message);
        ret = 1;
    }
    return ret;
}

void image_cleanup(image_data *image) {
    if (image->image_data != NULL)
        free(image->image_data);
}

void image_int_cleanup(image_int_data **image) {
    if (*image != NULL) {
        if ((*image)->image_data != NULL)
            free((*image)->image_data);
        free(*image);
        *image = NULL;
    }
}

void image_float_cleanup(image_float_data **image) {
    if (*image != NULL) {
        if ((*image)->image_data != NULL)
            free((*image)->image_data);
        free(*image);
        *image = NULL;
    }
}


void palette_cleanup(mapart_palette **palette) {
    if (*palette != NULL) {
        free(*palette);
        *palette = NULL;
    }
}

void float_palette_cleanup(mapart_float_palette **palette) {
    if (*palette != NULL) {
        free(*palette);
        *palette = NULL;
    }
}