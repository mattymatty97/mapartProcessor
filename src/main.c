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

main_options config = {};

#define RGB_SIZE 3
#define RGBA_SIZE 4
#define MULTIPLIER_SIZE 3

//----------------DEFINITIONS---------------

typedef struct {
    void *image_data;
    int x;
    int y;
    int channels;
} image_data;

typedef image_data image_int_data;

typedef image_data image_uint_data;

typedef image_data image_float_data;

typedef struct {
    char *palette_name;
    unsigned int palette_size;
    void  *palette;
    unsigned char *valid_ids;
} mapart_palette;

typedef mapart_palette mapart_float_palette;

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

void image_cleanup(void *image);
void imagep_cleanup(void *image);
void palette_cleanup(void *image);
void imagep_cleanup(void *image);

int load_image(image_data *image);

int save_image(mapart_palette *palette, image_data *dither_image);

int get_palette(mapart_palette *palette_o);

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
    config.maximum_height = -1;

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
            ((int*)int_image->image_data)[i] = ((unsigned char*)image.image_data)[i];
        }

        //convert to float array for GPU compatibility
        float_image->image_data = calloc(image.x * image.y * image.channels, sizeof(float));
        float_image->x = image.x;
        float_image->y = image.y;
        float_image->channels = image.channels;

        for (int i = 0; i < (image.x * image.y * image.channels); i++) {
            ((int*)int_image->image_data)[i] = ((unsigned char*)image.image_data)[i];
        }

        for (int i = 0; i < (image.x * image.y * image.channels); i++) {
            ((float*)float_image->image_data)[i] = ((unsigned char*)image.image_data)[i];
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

        imagep_cleanup(&int_image);
        processed_image = Lab_image;

        //convert palette to CIE-L*ab + alpha
        if (ret == 0) {
            mapart_float_palette *Lab_palette = calloc(1, sizeof(mapart_float_palette));
            Lab_palette->palette_name = strdup(palette.palette_name);
            Lab_palette->palette_size = palette.palette_size;
            Lab_palette->valid_ids = palette.valid_ids;
            Lab_palette->palette = calloc(palette.palette_size * MULTIPLIER_SIZE * RGBA_SIZE, sizeof(float));


            float *palette_xyz_data = calloc(palette.palette_size * MULTIPLIER_SIZE * RGBA_SIZE, sizeof(float));
            fprintf(stdout, "Converting palette to XYZ\n");
            fflush(stdout);
            ret = gpu_rgb_to_xyz(&config.gpu, palette.palette, palette_xyz_data, MULTIPLIER_SIZE,palette.palette_size);

            if (ret == 0) {
                fprintf(stdout, "Converting palette to CIE-L*ab\n");
                fflush(stdout);
                ret = gpu_xyz_to_lab(&config.gpu, palette_xyz_data, Lab_palette->palette,MULTIPLIER_SIZE, palette.palette_size);
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

        dither_function dither_func = &gpu_dither_none;

        if (dither == Floyd_Steinberg) {
            dither_func = &gpu_dither_floyd_steinberg;
        } else if (dither == JJND) {
            dither_func = &gpu_dither_JJND;
        } else if (dither == Stucki) {
            dither_func = &gpu_dither_Stucki;
        } else if (dither == Atkinson) {
            dither_func = &gpu_dither_Atkinson;
        } else if (dither == Burkes) {
            dither_func = &gpu_dither_Burkes;
        } else if (dither == Sierra) {
            dither_func = &gpu_dither_Sierra;
        } else if (dither == Sierra2) {
            dither_func = &gpu_dither_Sierra2;
        } else if (dither == SierraL) {
            dither_func = &gpu_dither_SierraL;
        }

        dithered_image.image_data = calloc(image.x * image.y * 2, sizeof(unsigned char));
        ret = dither_func(&config.gpu, processed_image->image_data, dithered_image.image_data, processed_palette->palette, processed_palette->valid_ids, noise, image.x, image.y, palette.palette_size, config.maximum_height);

        free(noise);
    }

    //save result
    if (ret == 0) {
        ret = save_image(&palette, &dithered_image);
    }

    //convert from palette to block and height
    image_uint_data mapart_data = {calloc(image.x * image.y * 2, sizeof (unsigned int)), image.x, image.y, 2};
    if (ret == 0) {
        fprintf(stdout, "Convert from palette to BlockId and height\n");
        fflush(stdout);
        ret = gpu_palette_to_height(&config.gpu, dithered_image.image_data, mapart_data.image_data, image.x, image.y, config.maximum_height);
    }

    imagep_cleanup(&processed_image);
    image_cleanup(&mapart_data);
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

    ret = gpu_palette_to_rgb(&config.gpu, dither_image->image_data, palette->palette,
                             converted_image.image_data, dither_image->x, dither_image->y, palette->palette_size, MULTIPLIER_SIZE);

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

int get_palette(mapart_palette *palette_o) {
    int ret = 0;
    bson_error_t error = {};

    printf("Loading palette\n");

    bson_json_reader_t *reader = bson_json_reader_new_from_file(config.palette_name, &error);
    if (reader != NULL) {
        bool eof = 0;
        bson_t doc = BSON_INITIALIZER;
        ret = bson_json_reader_read(reader, &doc, &error) <= 0;
        if (ret != 0) {
            fprintf(stderr, "Error reading bson: \n%s\n", error.message);
            ret = 2;
        }

        if (ret == 0) {
            printf("Palette found, processing...\n");
            bson_iter_t iter;
            bson_iter_t colors_iter;
            bson_iter_t multiplier_iter;

            int multipliers[MULTIPLIER_SIZE] = {};

            if (bson_iter_init_find(&iter, &doc, "multipliers") &&
                BSON_ITER_HOLDS_ARRAY(&iter) && bson_iter_recurse(&iter, &multiplier_iter)) {
                unsigned char index = 0;
                while (bson_iter_next(&multiplier_iter) && index < MULTIPLIER_SIZE) {
                    multipliers[index++] = bson_iter_int32(&multiplier_iter);
                }
            }

            unsigned int   palette_index = 0;
            unsigned int   palette_size = 1;
            int*           palette = calloc(1 * MULTIPLIER_SIZE * RGBA_SIZE,sizeof (int));
            unsigned char* valid_id = calloc(1,sizeof (unsigned char));

            //if object has color list
            if (bson_iter_init_find(&iter, &doc, "colors") &&
                BSON_ITER_HOLDS_ARRAY(&iter) && bson_iter_recurse(&iter, &colors_iter)) {
                while (bson_iter_next(&colors_iter)) {
                    bson_iter_t entry;
                    bson_iter_recurse(&colors_iter, &entry);
                    int color_id = 0;
                    int rgb[RGBA_SIZE] = {-255, -255, -255, 255};

                    if (bson_iter_find(&entry, "id") && BSON_ITER_HOLDS_INT32(&entry)) {
                        color_id = bson_iter_int32(&entry);
                    }

                    if (palette_size < (color_id + 1)) {
                        unsigned int old_size = palette_size;
                        while (palette_size < (color_id + 1)) {
                            palette_size *= 2;
                        }
                        palette = realloc(palette, palette_size * MULTIPLIER_SIZE * RGBA_SIZE * sizeof(int));
                        valid_id = realloc(valid_id, palette_size * sizeof(unsigned char));
                        for (old_size; old_size < palette_size; old_size++){
                            valid_id[old_size] = 0;
                        }
                    }

                    if (palette_index < (color_id + 1) )
                        palette_index = (color_id + 1);


                    bson_iter_t rgb_iter;
                    if (bson_iter_recurse(&colors_iter, &entry) &&
                        bson_iter_find(&entry, "color") && BSON_ITER_HOLDS_ARRAY(&entry) &&
                        bson_iter_recurse(&entry, &rgb_iter)) {
                        int index = 0;
                        while (bson_iter_next(&rgb_iter) && index < RGB_SIZE) {
                            rgb[index++] = bson_iter_int32(&rgb_iter);
                        }
                    }

                    // prepare the various multipliers
                    for (int i = 0; i < MULTIPLIER_SIZE; i++) {
                        unsigned int p_i = (color_id * MULTIPLIER_SIZE * RGBA_SIZE) + (i * RGBA_SIZE);
                        palette[p_i + 0] = (int) floor((double) rgb[0] * multipliers[i] / 255);
                        palette[p_i + 1] = (int) floor((double) rgb[1] * multipliers[i] / 255);
                        palette[p_i + 2] = (int) floor((double) rgb[2] * multipliers[i] / 255);
                        palette[p_i + 3] = (color_id!=0)?255:0;
                    }

                    if (bson_iter_recurse(&colors_iter, &entry) &&
                        bson_iter_find(&entry, "usable") && BSON_ITER_HOLDS_BOOL(&entry)) {
                        valid_id[color_id] = bson_iter_bool(&entry);
                    }
                }
                palette_size = palette_index;
                palette_o->palette_size = palette_size;
                palette = realloc(palette, palette_size * MULTIPLIER_SIZE * RGBA_SIZE * sizeof(int));
                valid_id = realloc(valid_id, palette_size * sizeof(unsigned char));
                palette_o->palette = palette;
                palette_o->valid_ids = valid_id;
            }
            printf("Palette loaded\n\n");
        }
        bson_json_reader_destroy(reader);
    } else {
        fprintf(stderr, "Error opening palette file %s:\n%s\n", config.palette_name, error.message);
        ret = 1;
    }
    return ret;
}

void image_cleanup(void *image) {
    if (((image_data *)image)->image_data != NULL)
        free(((image_data *)image)->image_data);
}

void imagep_cleanup(void *image) {
    if (*((image_data **)image) != NULL) {
        if ((*((image_data **)image))->image_data != NULL)
            free((*((image_data **)image))->image_data);
        free(*((image_data **)image));
        *((image_data **)image) = NULL;
    }
}

void palette_cleanup(void *palette){
    if ( *((mapart_palette **)palette) != NULL ){
        free((*((mapart_palette **)palette))->palette);
        free((*((mapart_palette **)palette))->palette_name);
        free((*((mapart_palette **)palette))->valid_ids);
    }
}