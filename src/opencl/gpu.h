#ifndef GPU_DEF
#define GPU_DEF

#define CL_TARGET_OPENCL_VERSION 300
#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
#include <CL/cl.h>
#endif

typedef struct  {
    const char *name;
    cl_program program;
} gpu_program;

typedef struct {
    cl_platform_id platformId;
    cl_device_id deviceId;
    cl_context context;
    cl_command_queue commandQueue;
    gpu_program programs[3];
} gpu_t;

int gpu_init(gpu_t *);
void gpu_clear(gpu_t *);

int gpu_rgb_to_xyz(gpu_t *gpu, int *input, float*output, unsigned int x, unsigned int y, unsigned char channels);
int gpu_xyz_to_lab(gpu_t *gpu, float *input, int*output, unsigned int x, unsigned int y, unsigned char channels);
int gpu_lab_to_lhc(gpu_t *gpu, int *input, int*output, unsigned int x, unsigned int y, unsigned char channels);

#endif