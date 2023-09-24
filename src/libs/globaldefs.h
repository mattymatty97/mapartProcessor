#ifndef GOBALDEFS_DEF
#define GOBALDEFS_DEF

#include <stdatomic.h>
#include <pthread.h>

#define GPU_CODE_NO_RECURSION

#include "../opencl/gpu.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#define MIN(a, b) (((a)<(b))?(a):(b))
#define MAX(a, b) (((a)>(b))?(a):(b))

typedef struct {
    char *project_name;
    unsigned long threads;
    char *image_filename;
    char *palette_name;
    unsigned int random_seed;
    int maximum_height;
    char *dithering;
    gpu_t gpu;
} main_options;

typedef struct {
    void *image_data;
    int width;
    int height;
    int channels;
} image_data;

typedef image_data image_char_data;

typedef image_data image_uchar_data;

typedef image_data image_int_data;

typedef image_data image_uint_data;

typedef image_data image_float_data;

typedef struct {
    unsigned int palette_size;
    void  *palette;
    char **palette_id_names;
    char **palette_block_ids;
    char *support_block;
    unsigned char *is_supported;
    unsigned char *is_usable;
    unsigned char *is_liquid;
} mapart_palette;

typedef mapart_palette mapart_float_palette;

#endif