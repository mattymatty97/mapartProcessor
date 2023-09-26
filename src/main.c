#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <math.h>
#include <stdbool.h>

#include "libs/alloc/tracked.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_MALLOC(sz)           t_malloc(sz)
#define STBI_REALLOC(p, newsz)    t_realloc(p, newsz)
#define STBI_FREE(p)              t_free(p)
#include "libs/images/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "libs/images/stb_image_write.h"

#define NBT_IMPLEMENTATION
#include "libs/litematica/nbt.h"

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

void palette_cleanup(mapart_palette *palette);

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
    while ((c = getopt_long(argc, argv, ":i:p:d:r:h:n:t:", long_options, &option_index)) != -1) {
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
                config.project_name = t_strdup(optarg);
                break;

            case 't':
                thread_count = strtol(optarg, NULL, 10);
                if (thread_count > 0)
                    config.threads = thread_count;
                break;

            case 'i':
                config.image_filename = t_strdup(optarg);
                break;

            case 'p':
                config.palette_name = t_strdup(optarg);
                break;

            case 'd':
                config.dithering = t_strdup(optarg);
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

    image_int_data int_image = {};

    dither_algorithm dither = none;

    mapart_palette palette = {};

    image_float_data processed_image = {};
    mapart_float_palette processed_palette = {};

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
        int_image.image_data = t_calloc((size_t)image.width * image.height * image.channels, sizeof(int));
        int_image.width = image.width;
        int_image.height = image.height;
        int_image.channels = image.channels;

        for (size_t i = 0; i < ((size_t)image.width * image.height * image.channels); i++) {
            ((int*)int_image.image_data)[i] = ((unsigned char*)image.image_data)[i];
        }

        //load image palette
        ret = get_palette(&palette);

    }

    //if we're still fine
    if (ret == 0) {
        //convert image to CIE-L*ab values + alpha
        image_float_data *Lab_image = &processed_image;
        Lab_image->image_data = t_calloc((size_t)image.width * image.height * image.channels, sizeof(float));
        Lab_image->width = image.width;
        Lab_image->height = image.height;
        Lab_image->channels = image.channels;

        float *xyz_data = t_calloc((size_t)image.width * image.height * image.channels, sizeof(float));

        fprintf(stdout, "Converting image to XYZ\n");
        fflush(stdout);
        ret = gpu_rgb_to_xyz(&config.gpu, int_image.image_data, xyz_data, image.width, image.height);

        if (ret == 0) {
            fprintf(stdout, "Converting image to CIE-L*ab\n");
            fflush(stdout);
            ret = gpu_xyz_to_lab(&config.gpu, xyz_data, Lab_image->image_data, image.width, image.height);
        }
        t_free(xyz_data);

        image_cleanup(&int_image);

        //convert palette to CIE-L*ab + alpha
        if (ret == 0) {
            mapart_float_palette *Lab_palette = &processed_palette;
            Lab_palette->palette_id_names = palette.palette_id_names;
            Lab_palette->palette_size = palette.palette_size;
            Lab_palette->is_usable = palette.is_usable;
            Lab_palette->is_liquid = palette.is_liquid;
            Lab_palette->palette = t_calloc(palette.palette_size * MULTIPLIER_SIZE * RGBA_SIZE, sizeof(float));


            float *palette_xyz_data = t_calloc(palette.palette_size * MULTIPLIER_SIZE * RGBA_SIZE, sizeof(float));
            fprintf(stdout, "Converting palette to XYZ\n");
            fflush(stdout);
            ret = gpu_rgb_to_xyz(&config.gpu, palette.palette, palette_xyz_data, MULTIPLIER_SIZE,palette.palette_size);

            if (ret == 0) {
                fprintf(stdout, "Converting palette to CIE-L*ab\n");
                fflush(stdout);
                ret = gpu_xyz_to_lab(&config.gpu, palette_xyz_data, Lab_palette->palette,MULTIPLIER_SIZE, palette.palette_size);
            }
            t_free(palette_xyz_data);
        }
    }

    image_uchar_data dithered_image = {
            NULL,
            image.width,
            image.height,
            2
    };
    //do dithering
    if (ret == 0) {
        fprintf(stdout, "Generate noise image\n");
        fflush(stdout);
        float *noise = t_calloc((size_t)image.width * image.height, sizeof(float));

        srand(config.random_seed);

        for (size_t i = 0; i < image.width * image.height; i++)
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

        dithered_image.image_data = t_calloc((size_t)image.width * image.height * 2, sizeof(unsigned char));
        ret = dither_func(&config.gpu, processed_image.image_data, dithered_image.image_data, processed_palette.palette, processed_palette.is_usable, processed_palette.is_liquid, noise, image.width, image.height, palette.palette_size, config.maximum_height);

        t_free(noise);
    }

    //save result
    if (ret == 0) {
        ret = save_image(&palette, &dithered_image);
    }

    //convert from palette to block and height
    image_uint_data mapart_data = {
            t_calloc((size_t)image.width * (image.height + 1) * 3,
                     sizeof (unsigned int)),
                     image.width,
                     image.height + 1,
                     3};
    unsigned int computed_max_height = 0;
    if (ret == 0) {
        fprintf(stdout, "Convert from palette to BlockId and height\n");
        fflush(stdout);
        ret = gpu_palette_to_height(&config.gpu, dithered_image.image_data, palette.is_liquid, mapart_data.image_data, palette.palette_size, image.width, image.height, config.maximum_height, &computed_max_height);
    }

    palette_cleanup(&processed_palette);
    palette_cleanup(&palette);

    image_cleanup(&processed_image);
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
        image->image_data = stbi_load(config.image_filename, &image->width, &image->height, &image->channels, 4);
        if (image->image_data == NULL) {
            fprintf(stderr, "Failed to load image %s:\n%s\n", config.image_filename, stbi_failure_reason());
            return 13;
        }
        image->channels = 4;
        printf("Image loaded: %dx%d(%d)\n\n", image->width, image->height, image->channels);
        return 0;
    } else {
        fprintf(stderr, "Failed to load image %s:\nFile does not exists\n", config.image_filename);
        return 12;
    }
}

int save_image(mapart_palette *palette, image_data *dither_image) {
    int ret = 0;
    image_data converted_image = {NULL, dither_image->width, dither_image->height, 4};

    converted_image.image_data = t_calloc((size_t)dither_image->width * dither_image->height * 4, sizeof(unsigned char));

    fprintf(stdout, "Convert dithered image back to rgb\n");
    fflush(stdout);

    ret = gpu_palette_to_rgb(&config.gpu, dither_image->image_data, palette->palette,
                             converted_image.image_data, dither_image->width, dither_image->height, palette->palette_size, MULTIPLIER_SIZE);

    char filename[1000] = "images/";
    if (ret == 0) {
        char *appendix = "";

        if (config.maximum_height == 0)
            appendix = "_flat";
        else if (config.maximum_height < 0)
            appendix = "_unlimited";
        else {
            char smallbuff[20] = {};
            sprintf(smallbuff, "_%d", config.maximum_height);
            appendix = smallbuff;
        }

        sprintf(&filename[7], "%s_%s%s.png", config.project_name, config.dithering, appendix);

        fprintf(stdout, "Save image\n");
        fflush(stdout);
        ret = stbi_write_png(filename, converted_image.width, converted_image.height, converted_image.channels,
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

    t_free(converted_image.image_data);
    return ret;
}

#include "libs/json/cJSON.h"

int get_palette(mapart_palette *palette_o) {
    int ret = 0;

    printf("Loading palette\n");

    FILE *palette_f = fopen(config.palette_name, "r");
    if (!palette_f){
        fprintf(stderr, "Error Opening palette file %s: %s\n", config.palette_name, strerror(errno));
        return 100;
    }
    fseek(palette_f,0,SEEK_END);
    size_t lenght = ftell(palette_f);
    char* palette_str = t_calloc(lenght, sizeof (char));
    rewind(palette_f);
    fread(palette_str, sizeof (char), lenght, palette_f);
    fclose(palette_f);

    cJSON_Hooks hooks = {
            t_malloc,
            t_free
    };

    cJSON_InitHooks(&hooks);
    cJSON *palette_json = cJSON_Parse(palette_str);

    t_free(palette_str);

    if (palette_json == NULL)
    {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL)
        {
            fprintf(stderr, "Error before: %s\n", error_ptr);
        }
        ret = 101;
    }

    if (ret == 0){
        cJSON *target;

        int multipliers[MULTIPLIER_SIZE] = {};

        target = cJSON_GetObjectItemCaseSensitive(palette_json, "multipliers");
        if (target != NULL && cJSON_IsArray(target)){
            cJSON *element;
            unsigned char index = 0;
            cJSON_ArrayForEach( element, target){
                if (index < MULTIPLIER_SIZE)
                    multipliers[index++] = element->valueint;
            }
            target = NULL;
        }

        target = cJSON_GetObjectItemCaseSensitive(palette_json, "support_block_id");
        if (target != NULL && cJSON_IsString(target)){
            palette_o->support_block = t_strdup(target->string);
            target = NULL;
        }

        unsigned int   palette_index = 0;
        unsigned int   palette_size = 1;
        int*           palette = t_calloc(1 * MULTIPLIER_SIZE * RGBA_SIZE,sizeof (int));
        unsigned char* is_usable = t_calloc(1, sizeof (unsigned char));
        char** palette_id_names = t_calloc(1,sizeof (char*));
        unsigned char* is_supported = t_calloc(1,sizeof (unsigned char));
        char** palette_block_ids = t_calloc(1,sizeof (char*));
        unsigned char* is_liquid = t_calloc(1, sizeof (unsigned char));

        target = cJSON_GetObjectItemCaseSensitive(palette_json, "colors");
        if (target != NULL && cJSON_IsArray(target)){
            cJSON *element;
            cJSON_ArrayForEach(element, target){
                cJSON *element_target;

                int color_id = 0;
                int rgb[RGBA_SIZE] = {-255, -255, -255, 255};

                element_target = cJSON_GetObjectItemCaseSensitive(element, "id");
                if (element_target != NULL && cJSON_IsNumber(element_target)){
                    color_id = element_target->valueint;
                    element_target = NULL;
                }

                if (palette_size < (color_id + 1)) {
                    unsigned int old_size = palette_size;
                    while (palette_size < (color_id + 1)) {
                        palette_size *= 2;
                    }
                    palette = t_recalloc(palette, palette_size * MULTIPLIER_SIZE * RGBA_SIZE , sizeof(int));
                    is_usable = t_recalloc(is_usable, palette_size , sizeof(unsigned char));
                    palette_id_names = t_recalloc(palette_id_names, palette_size , sizeof(char *));
                    is_supported = t_recalloc(is_supported, palette_size,sizeof (unsigned char));
                    palette_block_ids = t_recalloc(palette_block_ids, palette_size,sizeof (char*));
                    is_liquid = t_recalloc(is_liquid, palette_size, sizeof (unsigned char));
                }

                if (palette_index < (color_id + 1) )
                    palette_index = (color_id + 1);

                element_target = cJSON_GetObjectItemCaseSensitive(element, "name");
                if (element_target != NULL && cJSON_IsString(element_target)){
                    palette_id_names[color_id] = t_strdup(element_target->valuestring);
                    element_target = NULL;
                }

                element_target = cJSON_GetObjectItemCaseSensitive(element, "color");
                if (element_target != NULL && cJSON_IsArray(element_target)){
                    cJSON *rgb_element;
                    int index = 0;
                    cJSON_ArrayForEach(rgb_element, element_target) {
                        if(index < RGB_SIZE)
                            rgb[index++] = rgb_element->valueint;
                    }
                    element_target = NULL;
                }

                // prepare the various multipliers
                for (int i = 0; i < MULTIPLIER_SIZE; i++) {
                    unsigned int p_i = (color_id * MULTIPLIER_SIZE * RGBA_SIZE) + (i * RGBA_SIZE);
                    palette[p_i + 0] = (int) floor((double) rgb[0] * multipliers[i] / 255);
                    palette[p_i + 1] = (int) floor((double) rgb[1] * multipliers[i] / 255);
                    palette[p_i + 2] = (int) floor((double) rgb[2] * multipliers[i] / 255);
                    palette[p_i + 3] = (color_id!=0)?255:0;
                }

                element_target = cJSON_GetObjectItemCaseSensitive(element, "usable");
                if (element_target != NULL && cJSON_IsBool(element_target)){
                    is_usable[color_id] = element_target->valueint;
                    element_target = NULL;
                }

                element_target = cJSON_GetObjectItemCaseSensitive(element, "block_id");
                if (element_target != NULL && cJSON_IsString(element_target)){
                    palette_block_ids[color_id] = t_strdup(element_target->string);
                    element_target = NULL;
                }

                element_target = cJSON_GetObjectItemCaseSensitive(element, "needs_support");
                if (element_target != NULL && cJSON_IsBool(element_target)){
                    is_supported[color_id] = element_target->valueint;
                    element_target = NULL;
                }

                element_target = cJSON_GetObjectItemCaseSensitive(element, "is_liquid");
                if (element_target != NULL && cJSON_IsBool(element_target)){
                    is_liquid[color_id] = element_target->valueint;
                    element_target = NULL;
                }
            }
        }

        palette_size = palette_index;
        palette_o->palette_size = palette_size;
        palette = t_recalloc(palette, palette_size * MULTIPLIER_SIZE * RGBA_SIZE , sizeof(int));
        is_usable = t_recalloc(is_usable, palette_size , sizeof(unsigned char));
        palette_o->palette = palette;
        palette_o->is_usable = is_usable;
        palette_o->is_liquid = is_liquid;
        palette_o->palette_id_names = palette_id_names;
        palette_o->palette_block_ids = palette_block_ids;
        palette_o->is_supported = is_supported;
    }

    cJSON_Delete(palette_json);
    return ret;
}

void image_cleanup(image_data *image) {
    if (image->image_data != NULL)
        t_free(image->image_data);
}

void palette_cleanup(mapart_palette *palette){
    if ( palette != NULL ){
        if ( palette->palette != NULL ) {
            t_free(palette->palette);
            palette->palette = NULL;
        }

        if (palette->is_usable != NULL ) {
            t_free(palette->is_usable);
            palette->is_usable = NULL;
        }

        if ( palette->is_supported != NULL ) {
            t_free(palette->is_supported);
            palette->is_supported = NULL;
        }

        if ( palette->palette_id_names != NULL ) {
            for (int i = 0; i<palette->palette_size; i++)
                t_free(palette->palette_id_names[i]);
            t_free(palette->palette_id_names);
            palette->palette_id_names = NULL;
        }

        if ( palette->palette_id_names != NULL ) {
            for (int i = 0; i<palette->palette_size; i++)
                t_free(palette->palette_id_names[i]);
            t_free(palette->palette_id_names);
            palette->palette_id_names = NULL;
        }
    }
}