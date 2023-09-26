#ifndef GPU_DEF
#define GPU_DEF

#define CL_TARGET_OPENCL_VERSION 300
#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else

#include <CL/cl.h>

#endif

typedef struct {
    const char *name;
    cl_program program;
} gpu_program;

typedef struct {
    cl_platform_id platformId;
    cl_device_id deviceId;
    size_t max_parallelism;
    cl_context context;
    cl_command_queue commandQueue;
    gpu_program programs[12];
} gpu_t;

#endif

#ifndef GPU_CODE_NO_RECURSION

#include "../libs/globaldefs.h"

int gpu_init(main_options *config, gpu_t *);

void gpu_clear(gpu_t *);

int gpu_rgb_to_xyz(gpu_t *gpu, int *input, float *output, unsigned int width, unsigned int height);

int gpu_xyz_to_lab(gpu_t *gpu, float *input, float *output, unsigned int width, unsigned int height);

int gpu_xyz_to_luv(gpu_t *gpu, float *input, float *output, unsigned int width, unsigned int height);

int gpu_lab_to_lch(gpu_t *gpu, float *input, float *output, unsigned int width, unsigned int height);

int gpu_lch_to_lab(gpu_t *gpu, float *input, float *output, unsigned int width, unsigned int height);


typedef int (*dither_function)(gpu_t *gpu, float *input, unsigned char *output, float *palette, unsigned char *valid_palette_ids, unsigned char *liquid_palette_ids, float *noise, unsigned int width,
                               unsigned int height, unsigned char palette_indexes,
                               int max_minecraft_y);


int gpu_dither_none(gpu_t *gpu, float *input, unsigned char *output, float *palette, unsigned char *valid_palette_ids, unsigned char *liquid_palette_ids, float *noise, unsigned int width,
                    unsigned int height, unsigned char palette_indexes,
                    int max_minecraft_y);

int gpu_dither_floyd_steinberg(gpu_t *gpu, float *input, unsigned char *output, float *palette, unsigned char *valid_palette_ids, unsigned char *liquid_palette_ids, float *noise, unsigned int width,
                    unsigned int height, unsigned char palette_indexes,
                    int max_minecraft_y);

int gpu_dither_JJND(gpu_t *gpu, float *input, unsigned char *output, float *palette, unsigned char *valid_palette_ids, unsigned char *liquid_palette_ids, float *noise, unsigned int width,
                    unsigned int height, unsigned char palette_indexes,
                    int max_minecraft_y);

int gpu_dither_Stucki(gpu_t *gpu, float *input, unsigned char *output, float *palette, unsigned char *valid_palette_ids, unsigned char *liquid_palette_ids, float *noise, unsigned int width,
                    unsigned int height, unsigned char palette_indexes,
                    int max_minecraft_y);

int gpu_dither_Atkinson(gpu_t *gpu, float *input, unsigned char *output, float *palette, unsigned char *valid_palette_ids, unsigned char *liquid_palette_ids, float *noise, unsigned int width,
                    unsigned int height, unsigned char palette_indexes,
                    int max_minecraft_y);

int gpu_dither_Burkes(gpu_t *gpu, float *input, unsigned char *output, float *palette, unsigned char *valid_palette_ids, unsigned char *liquid_palette_ids, float *noise, unsigned int width,
                    unsigned int height, unsigned char palette_indexes,
                    int max_minecraft_y);

int gpu_dither_Sierra(gpu_t *gpu, float *input, unsigned char *output, float *palette, unsigned char *valid_palette_ids, unsigned char *liquid_palette_ids, float *noise, unsigned int width,
                    unsigned int height, unsigned char palette_indexes,
                    int max_minecraft_y);

int gpu_dither_Sierra2(gpu_t *gpu, float *input, unsigned char *output, float *palette, unsigned char *valid_palette_ids, unsigned char *liquid_palette_ids, float *noise, unsigned int width,
                    unsigned int height, unsigned char palette_indexes,
                    int max_minecraft_y);

int gpu_dither_SierraL(gpu_t *gpu, float *input, unsigned char *output, float *palette, unsigned char *valid_palette_ids, unsigned char *liquid_palette_ids, float *noise, unsigned int width,
                    unsigned int height, unsigned char palette_indexes,
                    int max_minecraft_y);

// de-conversion

int gpu_palette_to_rgb(gpu_t *gpu, unsigned char *input, int *palette, unsigned char *result, unsigned int width,
                       unsigned int height, unsigned char palette_indexes, unsigned char palette_variations);

// mapart methods

int gpu_palette_to_height(gpu_t *gpu, unsigned char *input, unsigned char *is_liquid, unsigned int *output,unsigned char palette_size, unsigned int width,
                          unsigned int height, int max_minecraft_y, unsigned int* computed_max_minecraft_y);

#else
#undef GPU_CODE_NO_RECURSION
#endif