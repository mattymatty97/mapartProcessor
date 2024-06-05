#include <stdio.h>
#include <limits.h>
#include "gpu.h"
#include "../libs/alloc/tracked.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

#define RGB_SIZE 3
#define RGBA_SIZE 4
#define MULTIPLIER_SIZE 3

typedef struct {
    unsigned int * indexes;
    unsigned int * diagonals;
    unsigned int diagonal_count;
} index_holder;

index_holder generate_indexes(unsigned int width, unsigned int height, unsigned int steepness);

cl_program gpu_compile_program(main_options *config, gpu_t *gpu_holder, char *filename, cl_int *ret);

cl_program gpu_compile_embedded_program(main_options *config, gpu_t *gpu_holder, char *filename, char * data, size_t size, cl_int *ret);

int gpu_init(main_options *config, gpu_t *gpu_holder) {
    cl_platform_id platform_id[3] = {};
    cl_device_id device_id[3][10] = {};
    char platform_names[3][301] = {};
    char device_names[3][10][301] = {};
    cl_uint ret_num_devices[3] = {};
    cl_uint ret_num_platforms = 0;

    gpu_holder->verbose = config->verbose;

    unsigned int total_devices = 0;

    cl_int ret = clGetPlatformIDs(3, platform_id, &ret_num_platforms);
    if (ret == CL_SUCCESS) {
        for (int i = 0; i < ret_num_platforms; i++) {
            ret = clGetPlatformInfo(platform_id[i], CL_PLATFORM_NAME, sizeof(char) * 300, platform_names[i], NULL);
            ret = clGetDeviceIDs(platform_id[i], CL_DEVICE_TYPE_ALL, 10,
                                 device_id[i], &ret_num_devices[i]);
            for (int j = 0; j < ret_num_devices[i]; j++) {
                ret = clGetDeviceInfo(device_id[i][j], CL_DEVICE_NAME, sizeof(char) * 300, device_names[i][j],
                                      NULL);
            }
            total_devices += ret_num_devices[i];
        }
    }else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }


    int platform_index = -1;
    int device_index = -1;

    if (total_devices > 0) {
        fprintf(stdout, "found %d devices on %d platforms: \n",
                total_devices, ret_num_platforms);
        for (int i = 0; i < ret_num_platforms; i++) {
            fprintf(stdout, "\t%s: \n", platform_names[i]);
            for (int j = 0; j < ret_num_devices[i]; j++) {
                fprintf(stdout, "\t%2d %2d - %s\n", i, j, device_names[i][j]);
            }
        }
        while (platform_index < 0 || platform_index >= ret_num_platforms || device_index < 0 ||
               device_index >= ret_num_devices[platform_index]) {
            fprintf(stdout, "please select a device to use (%%d %%d):\n");
            fflush(stdout);
            fflush(stdin);
            (void)!fscanf(stdin, "%d %d", &platform_index, &device_index);
        }

        fprintf(stdout, "Selected %s from %s\n", device_names[platform_index][device_index],
                platform_names[platform_index]);
        fflush(stdout);

        gpu_holder->platformId = platform_id[platform_index];
        gpu_holder->deviceId = device_id[platform_index][device_index];

        ret = clGetDeviceInfo(gpu_holder->deviceId, CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(size_t),
                              &gpu_holder->max_parallelism, NULL);
    }else{
        fprintf(stderr, "No OpenCL compatible devices found!\n");
        fflush(stderr);
        ret = EXIT_FAILURE;
    }

    cl_context context;
    if (ret == CL_SUCCESS)
        context = gpu_holder->context = clCreateContext(NULL, 1, &device_id[platform_index][device_index], NULL, NULL,
                                                        &ret);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }

    //if all is ok
    if (ret == CL_SUCCESS) {
        gpu_holder->commandQueue = clCreateCommandQueueWithProperties(context, device_id[platform_index][device_index],NULL, &ret);
    }else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }

    //compile programs
    if (ret == CL_SUCCESS) {

        extern char color_cl_start[] asm("_binary_resources_opencl_color_conversions_cl_start");
        extern char color_cl_end[] asm("_binary_resources_opencl_color_conversions_cl_end");
        size_t file_size = color_cl_end - color_cl_start;

        gpu_program program = {
                "color_conversion",
                gpu_compile_embedded_program(config, gpu_holder, "resources/opencl/color_conversions.cl",
                                             color_cl_start, file_size, &ret)
        };

        gpu_holder->programs[0] = program;

    }else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }

    if (ret == CL_SUCCESS) {

        extern char mapart_cl_start[] asm("_binary_resources_opencl_mapart_cl_start");
        extern char mapart_cl_end[] asm("_binary_resources_opencl_mapart_cl_end");
        size_t file_size = mapart_cl_end - mapart_cl_start;

        gpu_program program = {
                "mapart",
                gpu_compile_embedded_program(config, gpu_holder, "resources/opencl/mapart.cl", mapart_cl_start,
                                             file_size, &ret)
        };

        gpu_holder->programs[1] = program;

    }else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }

    if (ret == CL_SUCCESS) {

        extern char dither_cl_start[] asm("_binary_resources_opencl_dither_cl_start");
        extern char dither_cl_end[] asm("_binary_resources_opencl_dither_cl_end");
        size_t file_size = dither_cl_end - dither_cl_start;

        gpu_program program = {
                "gen_dithering",
                gpu_compile_embedded_program(config, gpu_holder, "resources/opencl/dither.cl", dither_cl_start,
                                             file_size, &ret)
        };

        gpu_holder->programs[2] = program;

    }else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }

    if (ret == CL_SUCCESS) {

        extern char progress_cl_start[] asm("_binary_resources_opencl_progress_cl_start");
        extern char progress_cl_end[] asm("_binary_resources_opencl_progress_cl_end");
        size_t file_size = progress_cl_end - progress_cl_start;

        gpu_program program = {
                "progress",
                gpu_compile_embedded_program(config, gpu_holder, "resources/opencl/progress.cl", progress_cl_start,
                                             file_size, &ret)
        };

        gpu_holder->programs[3] = program;

    }else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }

    return ret;

}

void gpu_clear(gpu_t *gpu_holder) {
    clFlush(gpu_holder->commandQueue);
    clFinish(gpu_holder->commandQueue);
    for (int i = 0; i < ARRAY_SIZE(gpu_holder->programs); i++)
        clReleaseProgram((gpu_holder->programs)[i].program);
    clReleaseCommandQueue(gpu_holder->commandQueue);
    clReleaseContext(gpu_holder->context);
}

cl_program gpu_compile_program(main_options *config, gpu_t *gpu_holder, char *filename, cl_int *ret) {
    char *source_str = NULL;
    size_t length = 0;

    FILE *fp;
    fp = fopen(filename, "r");
    if (!fp) {
        *ret = 5;
    }
    fseek(fp, 0, SEEK_END);
    length = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    source_str = calloc(length + 1, sizeof(char));
    (void)!fread(source_str, 1, length, fp);
    fclose(fp);

    if (*ret == CL_SUCCESS) {
        cl_program program = clCreateProgramWithSource(gpu_holder->context, 1, (const char **) &source_str, &length,
                                                       ret);
        if (*ret == CL_SUCCESS) {
            *ret = clBuildProgram(program, 1, &gpu_holder->deviceId, NULL, NULL, NULL);
        }

        if (*ret == CL_SUCCESS) {
            free(source_str);
            return program;
        } else {
            char build_log[5000] = {};
            size_t log_size = 0;
            clGetProgramBuildInfo(program, gpu_holder->deviceId, CL_PROGRAM_BUILD_LOG, sizeof(char) * 5000, build_log,
                                  &log_size);
            fprintf(stderr, "Error compiling %s:\n%s\n\n", filename, build_log);
        }
    }else{
        fprintf(stderr,"Fail at %s:%d code:%d filename:%s\n",__FILE_NAME__,__LINE__, *ret, filename);
        exit(*ret);
    }
    free(source_str);
    return NULL;
}


cl_program gpu_compile_embedded_program(main_options *config, gpu_t *gpu_holder, char *filename, char * data, size_t size, cl_int *ret) {

    if (*ret == CL_SUCCESS) {
        cl_program program = clCreateProgramWithSource(gpu_holder->context, 1, (const char **) &data, &size,
                                                       ret);
        if (*ret == CL_SUCCESS) {
            *ret = clBuildProgram(program, 1, &gpu_holder->deviceId, NULL, NULL, NULL);
        }

        if (*ret == CL_SUCCESS) {
            return program;
        } else {
            char build_log[5000] = {};
            size_t log_size = 0;
            clGetProgramBuildInfo(program, gpu_holder->deviceId, CL_PROGRAM_BUILD_LOG, sizeof(char) * 5000, build_log,
                                  &log_size);
            fprintf(stderr, "Error compiling %s:\n%s\n\n", filename, build_log);
        }
    }else{
        fprintf(stderr,"Fail at %s:%d code:%d filename:%s\n",__FILE_NAME__,__LINE__, *ret, filename);
        exit(*ret);
    }
    return NULL;
}

int gpu_rgba_to_composite(gpu_t *gpu, int *input, int *output, unsigned int width, unsigned int height) {
    size_t buffer_size = (size_t)width * height * 4;
    cl_int ret = 0;
    cl_mem input_mem_obj = NULL;
    cl_mem output_mem_obj = NULL;
    cl_kernel kernel = NULL;

    cl_event event[5];

    //create memory objects

    input_mem_obj = clCreateBuffer(gpu->context, CL_MEM_READ_ONLY,
                                   buffer_size * sizeof(int), NULL, &ret);
    if (ret == CL_SUCCESS)
        output_mem_obj = clCreateBuffer(gpu->context, CL_MEM_WRITE_ONLY,
                                        buffer_size * sizeof(int), NULL, &ret);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }

    //copy input into the memory object
    if (ret == CL_SUCCESS)
        ret = clEnqueueWriteBuffer(gpu->commandQueue, input_mem_obj, CL_TRUE, 0, buffer_size * sizeof(int), input, 0,
                                   NULL,  &event[0]);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }

    //create kernel
    if (ret == CL_SUCCESS)
        kernel = clCreateKernel(gpu->programs[0].program, "rgba_composite", &ret);
    else{
    fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
    exit(ret);
    }

    //set kernel arguments
    if (ret == CL_SUCCESS)
        ret = clSetKernelArg(kernel, 0, sizeof(cl_mem), (void *) &input_mem_obj);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }

    if (ret == CL_SUCCESS)
        ret = clSetKernelArg(kernel, 1, sizeof(cl_mem), (void *) &output_mem_obj);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }

    size_t global_item_size = width * height;
    size_t local_item_size = MIN(height, gpu->max_parallelism);

    //request the gpu process
    if (ret == CL_SUCCESS)
        ret = clEnqueueNDRangeKernel(gpu->commandQueue, kernel, 1, NULL, &global_item_size, &local_item_size, 1,  &event[0],
                                     &event[1]);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }

    //read the outputs
    if (ret == CL_SUCCESS)
        ret = clEnqueueReadBuffer(gpu->commandQueue, output_mem_obj, CL_TRUE, 0, buffer_size * sizeof(int), output, 1,
                                  &event[1], &event[2]);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }

    //wait for outputs
    if (ret == CL_SUCCESS)
        ret = clWaitForEvents(1, &event[2]);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }

    //flush remaining tasks
    if (ret == CL_SUCCESS)
        ret = clFlush(gpu->commandQueue);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }

    if (kernel != NULL)
        clReleaseKernel(kernel);

    if (input_mem_obj != NULL)
        clReleaseMemObject(input_mem_obj);
    if (output_mem_obj != NULL)
        clReleaseMemObject(output_mem_obj);

    return ret;
}

int gpu_rgb_to_ok(gpu_t *gpu, int *input, float *output, unsigned int width, unsigned int height) {
    size_t buffer_size = (size_t)width * height * 4;
    cl_int ret = 0;
    cl_mem input_mem_obj = NULL;
    cl_mem output_mem_obj = NULL;
    cl_kernel kernel = NULL;

    cl_event event[5];

    //create memory objects

    input_mem_obj = clCreateBuffer(gpu->context, CL_MEM_READ_ONLY,
                                   buffer_size * sizeof(int), NULL, &ret);
    if (ret == CL_SUCCESS)
        output_mem_obj = clCreateBuffer(gpu->context, CL_MEM_WRITE_ONLY,
                                        buffer_size * sizeof(float), NULL, &ret);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }

    //copy input into the memory object
    if (ret == CL_SUCCESS)
        ret = clEnqueueWriteBuffer(gpu->commandQueue, input_mem_obj, CL_TRUE, 0, buffer_size * sizeof(int), input, 0,
                                   NULL,  &event[0]);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }
    //create kernel
    if (ret == CL_SUCCESS)
        kernel = clCreateKernel(gpu->programs[0].program, "rgb_to_ok", &ret);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }
    //set kernel arguments
    if (ret == CL_SUCCESS)
        ret = clSetKernelArg(kernel, 0, sizeof(cl_mem), (void *) &input_mem_obj);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }
    if (ret == CL_SUCCESS)
        ret = clSetKernelArg(kernel, 1, sizeof(cl_mem), (void *) &output_mem_obj);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }

    size_t global_item_size = (size_t)width * height;
    size_t local_item_size = MIN(height, gpu->max_parallelism);
    while (global_item_size % local_item_size != 0) { local_item_size--; }

    //request the gpu process
    if (ret == 0)
        ret = clEnqueueNDRangeKernel(gpu->commandQueue, kernel, 1, NULL, &global_item_size, &local_item_size, 1,  &event[0],
                                     &event[1]);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }

    //read the outputs
    if (ret == CL_SUCCESS)
        ret = clEnqueueReadBuffer(gpu->commandQueue, output_mem_obj, CL_TRUE, 0, buffer_size * sizeof(float), output, 1,
                                  &event[1], &event[2]);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }

    //wait for outputs
    if (ret == CL_SUCCESS)
        ret = clWaitForEvents(1, &event[2]);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }

    //flush remaining tasks
    if (ret == CL_SUCCESS)
        ret = clFlush(gpu->commandQueue);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }

    if (kernel != NULL)
        clReleaseKernel(kernel);

    if (input_mem_obj != NULL)
        clReleaseMemObject(input_mem_obj);
    if (output_mem_obj != NULL)
        clReleaseMemObject(output_mem_obj);

    return ret;
}

//Dithering

int gpu_internal_dither_error_bleed(gpu_t *gpu, float *input, unsigned char *output, float *palette, unsigned char *valid_palette_ids, unsigned char *liquid_palette_ids, float *noise,
                                    unsigned int width, unsigned int height, unsigned char palette_indexes, int *bleeding_params,
                                    unsigned char bleeding_count, unsigned char min_required_pixels,
                                    int max_minecraft_y) {
    size_t buffer_size = (size_t)width * height * RGBA_SIZE;
    size_t palette_size = palette_indexes * MULTIPLIER_SIZE * RGBA_SIZE;
    size_t output_size = (size_t)width * height * 2;
    size_t noise_size = (size_t)width * height;
    size_t bleeding_size = bleeding_count * RGBA_SIZE;
    //iterate vertically for mc compatibility
    size_t global_workgroup_size = width * height;

    //generate diagonals

    index_holder index_holder = generate_indexes(width, height, min_required_pixels);

    cl_event event[5];

    cl_int ret = CL_SUCCESS;
    cl_mem input_mem_obj = NULL;
    cl_mem output_mem_obj = NULL;
    cl_mem error_buf_mem_obj = NULL;
    cl_mem palette_mem_obj = NULL;
    cl_mem palette_id_mem_obj = NULL;
    cl_mem palette_liquid_mem_obj = NULL;
    cl_mem noise_mem_obj = NULL;
    cl_mem height_mem_obj = NULL;
    cl_mem coord_mem_obj = NULL;
    cl_mem bleeding_mem_obj = NULL;
    cl_kernel kernel = NULL;
    cl_kernel progress_kernel = NULL;

    //create memory objects

    input_mem_obj = clCreateBuffer(gpu->context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                   buffer_size * sizeof(float), input, &ret);
    if (ret == CL_SUCCESS)
        output_mem_obj = clCreateBuffer(gpu->context, CL_MEM_WRITE_ONLY,
                                        output_size * sizeof(unsigned char), NULL, &ret);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }
    if (ret == CL_SUCCESS)
        error_buf_mem_obj = clCreateBuffer(gpu->context, CL_MEM_READ_WRITE,
                                           buffer_size * sizeof(int), NULL, &ret);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }
    if (ret == CL_SUCCESS)
        palette_mem_obj = clCreateBuffer(gpu->context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                         palette_size * sizeof(float), palette, &ret);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }
    if (ret == CL_SUCCESS)
        palette_id_mem_obj = clCreateBuffer(gpu->context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                            palette_indexes * sizeof(unsigned char), valid_palette_ids, &ret);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }
    if (ret == CL_SUCCESS)
        palette_liquid_mem_obj = clCreateBuffer(gpu->context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                            palette_indexes * sizeof(unsigned char), liquid_palette_ids, &ret);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }
    if (ret == CL_SUCCESS)
        noise_mem_obj = clCreateBuffer(gpu->context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                       noise_size * sizeof(float), noise, &ret);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }
    if (ret == CL_SUCCESS)
        height_mem_obj = clCreateBuffer(gpu->context, CL_MEM_READ_WRITE,
                                       width * sizeof(int), NULL, &ret);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }
    if (ret == CL_SUCCESS)
        coord_mem_obj = clCreateBuffer(gpu->context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR,
                                       width * height * 2 * sizeof(unsigned int), index_holder.indexes, &ret);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }
    if (ret == CL_SUCCESS) {
        if (bleeding_count > 0)
            bleeding_mem_obj = clCreateBuffer(gpu->context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                              bleeding_size * sizeof(int), bleeding_params, &ret);
    }else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }

    //request clean the error buffer
    float pattern = 0;
    int i_pattern = 0;

    if (ret == CL_SUCCESS)
        ret = clEnqueueFillBuffer(gpu->commandQueue, error_buf_mem_obj, &i_pattern, sizeof (int), 0, buffer_size * sizeof(int), 0, NULL, NULL);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }
    if (ret == CL_SUCCESS)
            ret = clEnqueueFillBuffer(gpu->commandQueue, height_mem_obj, &i_pattern, sizeof (int), 0, width * sizeof(int), 0, NULL, NULL);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }

    //create kernel
    if (ret == CL_SUCCESS)
        kernel = clCreateKernel(gpu->programs[2].program, "error_bleed", &ret);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }
    if (ret == CL_SUCCESS)
        progress_kernel = clCreateKernel(gpu->programs[3].program, "progress", &ret);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }

    unsigned char arg_index = 0;
    //set kernel arguments
    if (ret == CL_SUCCESS)
        ret = clSetKernelArg(kernel, arg_index++, sizeof(cl_mem), (void *) &input_mem_obj);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }
    if (ret == CL_SUCCESS)
        ret = clSetKernelArg(kernel, arg_index++, sizeof(cl_mem), (void *) &output_mem_obj);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }
    if (ret == CL_SUCCESS)
        ret = clSetKernelArg(kernel, arg_index++, sizeof(cl_mem), (void *) &error_buf_mem_obj);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }
    if (ret == CL_SUCCESS)
        ret = clSetKernelArg(kernel, arg_index++, sizeof(cl_mem), (void *) &palette_mem_obj);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }
    if (ret == CL_SUCCESS)
        ret = clSetKernelArg(kernel, arg_index++, sizeof(cl_mem), (void *) &palette_id_mem_obj);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }
    if (ret == CL_SUCCESS)
        ret = clSetKernelArg(kernel, arg_index++, sizeof(cl_mem), (void *) &palette_liquid_mem_obj);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }
    if (ret == CL_SUCCESS)
        ret = clSetKernelArg(kernel, arg_index++, sizeof(cl_mem), (void *) &noise_mem_obj);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }
    if (ret == CL_SUCCESS)
        ret = clSetKernelArg(kernel, arg_index++, sizeof(cl_mem), (void *) &height_mem_obj);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }
    if (ret == CL_SUCCESS)
        ret = clSetKernelArg(kernel, arg_index++, sizeof(cl_mem), (void *) &coord_mem_obj);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }
    if (ret == CL_SUCCESS)
        ret = clSetKernelArg(kernel, arg_index++, sizeof(const unsigned int), (void *) &width);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }
    if (ret == CL_SUCCESS)
        ret = clSetKernelArg(kernel, arg_index++, sizeof(const unsigned int), (void *) &height);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }
    if (ret == CL_SUCCESS)
        ret = clSetKernelArg(kernel, arg_index++, sizeof(const unsigned char), (void *) &palette_indexes);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }
    if (ret == CL_SUCCESS)
        ret = clSetKernelArg(kernel, arg_index++, sizeof(cl_mem), (void *) &bleeding_mem_obj);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }
    if (ret == CL_SUCCESS)
        ret = clSetKernelArg(kernel, arg_index++, sizeof(const unsigned char), (void *) &bleeding_count);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }
    if (ret == CL_SUCCESS)
        ret = clSetKernelArg(kernel, arg_index++, sizeof(const unsigned char), (void *) &min_required_pixels);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }
    if (ret == CL_SUCCESS)
        ret = clSetKernelArg(kernel, arg_index++, sizeof(const int), (void *) &max_minecraft_y);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }

    //set progress kernel params
    if (ret == CL_SUCCESS)
        ret = clSetKernelArg(progress_kernel, 0, sizeof(const unsigned int), (void *) &width);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }
    if (ret == CL_SUCCESS)
        ret = clSetKernelArg(progress_kernel, 1, sizeof(const unsigned int), (void *) &height);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }

    //let all the fill operations complete first
    if (ret == CL_SUCCESS){
        ret = clEnqueueBarrierWithWaitList(gpu->commandQueue, 0, NULL, NULL);
    }else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }


    //request the gpu process
    if (ret == CL_SUCCESS){
        unsigned int totalOffset = 0;
        for (unsigned int diagonal = 0; diagonal < index_holder.diagonal_count; diagonal++){
            unsigned int diaLen = index_holder.diagonals[diagonal];
            for (size_t local_workgroup_size = 0, offset = 0; offset < diaLen  && ret == CL_SUCCESS; offset += local_workgroup_size){
                local_workgroup_size = MIN(diaLen - offset, gpu->max_parallelism);
                size_t curr_offset = totalOffset + offset;
                ret = clEnqueueNDRangeKernel(gpu->commandQueue, kernel, 1, &curr_offset, &local_workgroup_size, &local_workgroup_size,
                                             0,  NULL,NULL);

            }
            //let all computations for the previous diagonal to complete then continue ( this is all non-blocking for the cpu)
            if (ret == CL_SUCCESS){
                ret = clEnqueueBarrierWithWaitList(gpu->commandQueue, 0, NULL, &event[0]);
            }
            totalOffset += diaLen;
            if (ret == CL_SUCCESS && gpu->verbose){
                size_t unit = 1;
                size_t offset = totalOffset -1;
                ret = clEnqueueNDRangeKernel(gpu->commandQueue, progress_kernel, 1, &offset, &unit, &unit,
                                             0,  NULL, NULL);
            }
        }
    }else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }


    //read the outputs
    if (ret == CL_SUCCESS)
        ret = clEnqueueReadBuffer(gpu->commandQueue, output_mem_obj, CL_TRUE, 0, output_size * sizeof(unsigned char),
                                  output, 1, &event[0], &event[1]);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }

    if (ret == CL_SUCCESS)
        ret = clWaitForEvents(1, &event[1]);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }

    //flush remaining tasks
    if (ret == CL_SUCCESS)
        ret = clFlush(gpu->commandQueue);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }

    if (kernel != NULL)
        clReleaseKernel(kernel);

    if (progress_kernel != NULL)
        clReleaseKernel(progress_kernel);

    if (input_mem_obj != NULL)
        clReleaseMemObject(input_mem_obj);
    if (palette_mem_obj != NULL)
        clReleaseMemObject(palette_mem_obj);
    if (palette_id_mem_obj != NULL)
        clReleaseMemObject(palette_id_mem_obj);
    if (palette_liquid_mem_obj != NULL)
        clReleaseMemObject(palette_liquid_mem_obj);
    if (noise_mem_obj != NULL)
        clReleaseMemObject(noise_mem_obj);
    if (output_mem_obj != NULL)
        clReleaseMemObject(output_mem_obj);
    if (error_buf_mem_obj != NULL)
        clReleaseMemObject(error_buf_mem_obj);
    if (height_mem_obj != NULL)
        clReleaseMemObject(height_mem_obj);
    if (coord_mem_obj != NULL)
        clReleaseMemObject(coord_mem_obj);
    if (bleeding_mem_obj != NULL)
        clReleaseMemObject(bleeding_mem_obj);
    if (index_holder.indexes != NULL)
        t_free(index_holder.indexes);
    if (index_holder.diagonals != NULL)
        t_free(index_holder.diagonals);
    return ret;
}

int gpu_dither_none(gpu_t *gpu, float *input, unsigned char *output, float *palette, unsigned char *valid_palette_ids, unsigned char *liquid_palette_ids, float *noise, unsigned int width,
                    unsigned int height, unsigned char palette_indexes,
                    int max_minecraft_y) {
    return gpu_internal_dither_error_bleed(gpu, input, output, palette, valid_palette_ids, liquid_palette_ids, noise, width, height, palette_indexes, NULL,
                                           0, 0, max_minecraft_y);
}

int gpu_dither_floyd_steinberg(gpu_t *gpu, float *input, unsigned char *output, float *palette, unsigned char *valid_palette_ids, unsigned char *liquid_palette_ids, float *noise, unsigned int width,
                    unsigned int height, unsigned char palette_indexes,
                    int max_minecraft_y) {
    int bleeding_parameters[4][4] = {
            {1, 0, 7, 16},
            {-1, 1, 3, 16},
            {0, 1, 5, 16},
            {1, 1, 1, 16}
    };
    return gpu_internal_dither_error_bleed(gpu, input, output, palette, valid_palette_ids, liquid_palette_ids, noise, width, height, palette_indexes,
                                           (int *) bleeding_parameters, 4, 2, max_minecraft_y);
}

int gpu_dither_JJND(gpu_t *gpu, float *input, unsigned char *output, float *palette, unsigned char *valid_palette_ids, unsigned char *liquid_palette_ids, float *noise, unsigned int width,
                    unsigned int height, unsigned char palette_indexes,
                    int max_minecraft_y) {
    int bleeding_parameters[12][4] = {
            {1, 0, 7, 48},
            {2, 0, 5, 48},
            {-2, 1, 3, 48},
            {-1, 1, 5, 48},
            {0, 1, 7, 48},
            {1, 1, 5, 48},
            {2, 1, 3, 48},
            {-2, 2, 1, 48},
            {-1, 2, 3, 48},
            {0, 2, 5, 48},
            {1, 2, 3, 48},
            {2, 2, 1, 48}
    };
    return gpu_internal_dither_error_bleed(gpu, input, output, palette, valid_palette_ids, liquid_palette_ids, noise, width, height, palette_indexes,
                                           (int *) bleeding_parameters, 12, 3, max_minecraft_y);
}

int gpu_dither_Stucki(gpu_t *gpu, float *input, unsigned char *output, float *palette, unsigned char *valid_palette_ids, unsigned char *liquid_palette_ids, float *noise, unsigned int width,
                    unsigned int height, unsigned char palette_indexes,
                    int max_minecraft_y) {
    int bleeding_parameters[12][4] = {
            {1, 0, 8, 42},
            {2, 0, 4, 42},
            {-2, 1, 2, 42},
            {-1, 1, 4, 42},
            {0, 1, 8, 42},
            {1, 1, 4, 42},
            {2, 1, 2, 42},
            {-2, 2, 1, 42},
            {-1, 2, 2, 42},
            {0, 2, 4, 42},
            {1, 2, 2, 42},
            {2, 2, 1, 42}
    };
    return gpu_internal_dither_error_bleed(gpu, input, output, palette, valid_palette_ids, liquid_palette_ids, noise, width, height, palette_indexes,
                                           (int *) bleeding_parameters, 12, 3, max_minecraft_y);
}

int gpu_dither_Atkinson(gpu_t *gpu, float *input, unsigned char *output, float *palette, unsigned char *valid_palette_ids, unsigned char *liquid_palette_ids, float *noise, unsigned int width,
                    unsigned int height, unsigned char palette_indexes,
                    int max_minecraft_y) {
    int bleeding_parameters[6][4] = {
            {1, 0, 1, 8},
            {2, 0, 1, 8},
            {-1, 1, 1, 8},
            {0, 1, 1, 8},
            {1, 1, 1, 8},
            {0, 2, 1, 8}
    };
    return gpu_internal_dither_error_bleed(gpu, input, output, palette, valid_palette_ids, liquid_palette_ids, noise, width, height, palette_indexes,
                                           (int *) bleeding_parameters, 6, 3, max_minecraft_y);
}

int gpu_dither_Burkes(gpu_t *gpu, float *input, unsigned char *output, float *palette, unsigned char *valid_palette_ids, unsigned char *liquid_palette_ids, float *noise, unsigned int width,
                    unsigned int height, unsigned char palette_indexes,
                    int max_minecraft_y) {
    int bleeding_parameters[7][4] = {
            {1, 0, 8, 32},
            {2, 0, 4, 32},
            {-2, 1, 2, 32},
            {-1, 1, 4, 32},
            {0, 1, 8, 32},
            {1, 1, 4, 32},
            {2, 1, 2, 32}
    };
    return gpu_internal_dither_error_bleed(gpu, input, output, palette, valid_palette_ids, liquid_palette_ids, noise, width, height, palette_indexes,
                                           (int *) bleeding_parameters, 7, 3, max_minecraft_y);
}

int gpu_dither_Sierra(gpu_t *gpu, float *input, unsigned char *output, float *palette, unsigned char *valid_palette_ids, unsigned char *liquid_palette_ids, float *noise, unsigned int width,
                    unsigned int height, unsigned char palette_indexes,
                    int max_minecraft_y) {
    int bleeding_parameters[10][4] = {
            {1, 0, 5, 32},
            {2, 0, 3, 32},
            {-2, 1, 2, 32},
            {-1, 1, 4, 32},
            {0, 1, 5, 32},
            {1, 1, 4, 32},
            {2, 1, 2, 32},
            {-1, 2, 2, 32},
            {0, 2, 3, 32},
            {1, 2, 2, 32}
    };
    return gpu_internal_dither_error_bleed(gpu, input, output, palette, valid_palette_ids, liquid_palette_ids, noise, width, height, palette_indexes,
                                           (int *) bleeding_parameters, 10, 3, max_minecraft_y);
}

int gpu_dither_Sierra2(gpu_t *gpu, float *input, unsigned char *output, float *palette, unsigned char *valid_palette_ids, unsigned char *liquid_palette_ids, float *noise, unsigned int width,
                    unsigned int height, unsigned char palette_indexes,
                    int max_minecraft_y) {
    int bleeding_parameters[7][4] = {
            {1, 0, 4, 16},
            {2, 0, 3, 16},
            {-2, 1, 1, 16},
            {-1, 1, 2, 16},
            {0, 1, 3, 16},
            {1, 1, 2, 16},
            {2, 1, 1, 16}
    };
    return gpu_internal_dither_error_bleed(gpu, input, output, palette, valid_palette_ids, liquid_palette_ids, noise, width, height, palette_indexes,
                                           (int *) bleeding_parameters, 7, 3, max_minecraft_y);
}

int gpu_dither_SierraL(gpu_t *gpu, float *input, unsigned char *output, float *palette, unsigned char *valid_palette_ids, unsigned char *liquid_palette_ids, float *noise, unsigned int width,
                    unsigned int height, unsigned char palette_indexes,
                    int max_minecraft_y) {
    int bleeding_parameters[3][4] = {
            {1, 0, 2, 4},
            {-1, 1, 1, 4},
            {0, 1, 1, 4}
    };
    return gpu_internal_dither_error_bleed(gpu, input, output, palette, valid_palette_ids, liquid_palette_ids, noise, width, height, palette_indexes,
                                           (int *) bleeding_parameters, 3, 2, max_minecraft_y);
}

// de-conversion

int gpu_palette_to_rgb(gpu_t *gpu, unsigned char *input, int *palette, unsigned char *output, unsigned int width,
                       unsigned int height, unsigned char palette_indexes, unsigned char palette_variations) {
    size_t buffer_size = (size_t)width * height * 2;
    size_t palette_size = palette_indexes * palette_variations * 4;
    size_t output_size = (size_t)width * height * 4;
    cl_int ret = 0;
    cl_mem input_mem_obj = NULL;
    cl_mem palette_mem_obj = NULL;
    cl_mem output_mem_obj = NULL;
    cl_kernel kernel = NULL;

    cl_event event[5];

    //create memory objects

    input_mem_obj = clCreateBuffer(gpu->context, CL_MEM_READ_ONLY,
                                   buffer_size * sizeof(unsigned char), NULL, &ret);
    if (ret == CL_SUCCESS)
        palette_mem_obj = clCreateBuffer(gpu->context, CL_MEM_READ_ONLY,
                                         palette_size * sizeof(int), NULL, &ret);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }
    if (ret == CL_SUCCESS)
        output_mem_obj = clCreateBuffer(gpu->context, CL_MEM_WRITE_ONLY,
                                        output_size * sizeof(unsigned char), NULL, &ret);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }

    //copy input into the memory object
    if (ret == CL_SUCCESS)
        ret = clEnqueueWriteBuffer(gpu->commandQueue, input_mem_obj, CL_TRUE, 0, buffer_size * sizeof(unsigned char),
                                   input, 0, NULL, NULL);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }
    if (ret == CL_SUCCESS)
        ret = clEnqueueWriteBuffer(gpu->commandQueue, palette_mem_obj, CL_TRUE, 0, palette_size * sizeof(int), palette,
                                   0, NULL, NULL);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }

    //create kernel
    if (ret == CL_SUCCESS)
        kernel = clCreateKernel(gpu->programs[1].program, "palette_to_rgb", &ret);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }

    //set kernel arguments
    if (ret == CL_SUCCESS)
        ret = clSetKernelArg(kernel, 0, sizeof(cl_mem), (void *) &input_mem_obj);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }
    if (ret == CL_SUCCESS)
        ret = clSetKernelArg(kernel, 1, sizeof(cl_mem), (void *) &palette_mem_obj);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }
    if (ret == CL_SUCCESS)
        ret = clSetKernelArg(kernel, 2, sizeof(cl_mem), (void *) &output_mem_obj);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }
    if (ret == CL_SUCCESS)
        ret = clSetKernelArg(kernel, 3, sizeof(const unsigned char), (void *) &palette_indexes);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }
    if (ret == CL_SUCCESS)
        ret = clSetKernelArg(kernel, 4, sizeof(const unsigned char), (void *) &palette_variations);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }

    size_t global_item_size = width * height;
    size_t local_item_size = MIN(height, gpu->max_parallelism);

    //request the gpu process
    if (ret == CL_SUCCESS)
        ret = clEnqueueNDRangeKernel(gpu->commandQueue, kernel, 1, NULL, &global_item_size, &local_item_size, 0, NULL,
                                     NULL);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }

    //read the outputs
    if (ret == CL_SUCCESS)
        ret = clEnqueueReadBuffer(gpu->commandQueue, output_mem_obj, CL_TRUE, 0, output_size * sizeof(unsigned char),
                                  output, 0, NULL, &event[0]);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }

    if (ret == CL_SUCCESS)
        ret = clWaitForEvents(1, &event[0]);
    else{
        fprintf(stderr,"Fail while waiting %s:%d", __FILE_NAME__,__LINE__);
        exit(ret);
    }

    //flush remaining tasks
    if (ret == CL_SUCCESS)
        ret = clFlush(gpu->commandQueue);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }

    if (kernel != NULL)
        clReleaseKernel(kernel);

    if (input_mem_obj != NULL)
        clReleaseMemObject(input_mem_obj);
    if (palette_mem_obj != NULL)
        clReleaseMemObject(palette_mem_obj);
    if (output_mem_obj != NULL)
        clReleaseMemObject(output_mem_obj);

    return ret;
}

// mapart

int gpu_palette_to_height(gpu_t *gpu, unsigned char *input, unsigned char *is_liquid, unsigned int *output,unsigned char palette_size, unsigned int width,
                          unsigned int height, int max_minecraft_y, unsigned int* computed_max_minecraft_y) {
    size_t buffer_size = (size_t)width * height * 2;
    size_t output_size = (size_t)width * (height + 1) * 3;

    cl_event event[5];

    cl_int ret = 0;
    cl_mem input_mem_obj = NULL;
    cl_mem liquid_mem_obj = NULL;
    cl_mem output_mem_obj = NULL;
    cl_mem error_mem_obj = NULL;
    cl_mem height_mem_obj = NULL;
    cl_mem index_mem_obj = NULL;
    cl_mem padding_mem_obj = NULL;
    cl_mem flat_mem_obj = NULL;
    cl_mem max_mem_obj = NULL;
    cl_kernel kernel = NULL;
    cl_kernel progress_kernel = NULL;

    //create memory objects

    input_mem_obj = clCreateBuffer(gpu->context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                   buffer_size * sizeof(unsigned char), input, &ret);
    if (ret == CL_SUCCESS){
        liquid_mem_obj = clCreateBuffer(gpu->context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                       palette_size * sizeof(unsigned char), is_liquid, &ret);
    }else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }

    if (ret == CL_SUCCESS)
        output_mem_obj = clCreateBuffer(gpu->context, CL_MEM_READ_WRITE,
                                        output_size * sizeof(unsigned int), NULL, &ret);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }
    if (ret == CL_SUCCESS)
        error_mem_obj = clCreateBuffer(gpu->context, CL_MEM_READ_WRITE,
                                                 sizeof(unsigned int), NULL, &ret);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }
    if (ret == CL_SUCCESS)
        height_mem_obj = clCreateBuffer(gpu->context, CL_MEM_READ_WRITE,width * sizeof(int), NULL, &ret);

    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }
    if (ret == CL_SUCCESS)
        index_mem_obj = clCreateBuffer(gpu->context, CL_MEM_READ_WRITE,width * sizeof(unsigned int), NULL, &ret);

    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }
    if (ret == CL_SUCCESS)
        padding_mem_obj = clCreateBuffer(gpu->context, CL_MEM_READ_WRITE,width * sizeof(int), NULL, &ret);

    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }
    if (ret == CL_SUCCESS)
        flat_mem_obj = clCreateBuffer(gpu->context, CL_MEM_READ_WRITE,width * sizeof(unsigned int), NULL, &ret);

    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }
    if (ret == CL_SUCCESS)
        max_mem_obj = clCreateBuffer(gpu->context, CL_MEM_READ_WRITE,sizeof(unsigned int), NULL, &ret);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }

    //request clean the error indicator
    unsigned int pattern = 0;
    int pattern2 = 1;

    if (ret == CL_SUCCESS)
        ret = clEnqueueFillBuffer(gpu->commandQueue, error_mem_obj, &pattern, sizeof(unsigned int), 0, sizeof(unsigned int), 0, NULL, NULL);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }
    if (ret == CL_SUCCESS)
        ret = clEnqueueFillBuffer(gpu->commandQueue, max_mem_obj, &pattern, sizeof(unsigned int), 0, sizeof(unsigned int), 0,  NULL, NULL);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }
    if (ret == CL_SUCCESS)
        ret = clEnqueueFillBuffer(gpu->commandQueue, height_mem_obj, &pattern, sizeof (int), 0, width * sizeof(int), 0, NULL, NULL);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }
    if (ret == CL_SUCCESS)
            ret = clEnqueueFillBuffer(gpu->commandQueue, index_mem_obj, &pattern, sizeof (int), 0, width * sizeof(int), 0, NULL, NULL);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }
    if (ret == CL_SUCCESS)
            ret = clEnqueueFillBuffer(gpu->commandQueue, padding_mem_obj, &pattern2, sizeof (int), 0, width * sizeof(int), 0, NULL, NULL);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }
    if (ret == CL_SUCCESS)
            ret = clEnqueueFillBuffer(gpu->commandQueue, flat_mem_obj, &pattern, sizeof (int), 0, width * sizeof(int), 0, NULL, NULL);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }

    //let all the fill operations complete first
    if (ret == CL_SUCCESS){
        ret = clEnqueueBarrierWithWaitList(gpu->commandQueue, 0, NULL, NULL);
    }    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }


    //create kernel
    if (ret == CL_SUCCESS)
        kernel = clCreateKernel(gpu->programs[1].program, "palette_to_height", &ret);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }
    if (ret == CL_SUCCESS)
        progress_kernel = clCreateKernel(gpu->programs[3].program, "progress", &ret);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }

    unsigned char arg_index = 0;
    //set kernel arguments
    if (ret == CL_SUCCESS)
        ret = clSetKernelArg(kernel, arg_index++, sizeof(cl_mem), (void *) &input_mem_obj);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }
    if (ret == CL_SUCCESS)
        ret = clSetKernelArg(kernel, arg_index++, sizeof(cl_mem), (void *) &liquid_mem_obj);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }
    if (ret == CL_SUCCESS)
        ret = clSetKernelArg(kernel, arg_index++, sizeof(cl_mem), (void *) &output_mem_obj);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }
    if (ret == CL_SUCCESS)
        ret = clSetKernelArg(kernel, arg_index++, sizeof(cl_mem), (void *) &error_mem_obj);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }
    if (ret == CL_SUCCESS)
        ret = clSetKernelArg(kernel, arg_index++, sizeof(cl_mem), (void *) &height_mem_obj);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }
    if (ret == CL_SUCCESS)
        ret = clSetKernelArg(kernel, arg_index++, sizeof(cl_mem), (void *) &index_mem_obj);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }
    if (ret == CL_SUCCESS)
        ret = clSetKernelArg(kernel, arg_index++, sizeof(cl_mem), (void *) &padding_mem_obj);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }
    if (ret == CL_SUCCESS)
        ret = clSetKernelArg(kernel, arg_index++, sizeof(cl_mem), (void *) &flat_mem_obj);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }
    if (ret == CL_SUCCESS)
        ret = clSetKernelArg(kernel, arg_index++, sizeof(const unsigned int), (void *) &width);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }
    if (ret == CL_SUCCESS)
        ret = clSetKernelArg(kernel, arg_index++, sizeof(const unsigned int), (void *) &height);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }
    if (ret == CL_SUCCESS)
        ret = clSetKernelArg(kernel, arg_index++, sizeof(const int), (void *) &max_minecraft_y);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }
    if (ret == CL_SUCCESS)
        ret = clSetKernelArg(kernel, arg_index++, sizeof(cl_mem), (void *) &max_mem_obj);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }


    //set progress kernel params
    if (ret == CL_SUCCESS)
        ret = clSetKernelArg(progress_kernel, 0, sizeof(const unsigned int), (void *) &width);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }
    if (ret == CL_SUCCESS)
        ret = clSetKernelArg(progress_kernel, 1, sizeof(const unsigned int), (void *) &height);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }

    //request the gpu process
    if (ret == CL_SUCCESS){
        size_t unit = 1;
        unsigned int totalOffset = 0;
        for (unsigned int row = 0; row < height; row++){
            for (size_t local_workgroup_size = 0, offset = 0; offset < width  && ret == CL_SUCCESS; offset += local_workgroup_size){
                local_workgroup_size = MIN(width - offset, gpu->max_parallelism);
                size_t curr_offset = totalOffset + offset;
                ret = clEnqueueNDRangeKernel(gpu->commandQueue, kernel, 1, &curr_offset, &local_workgroup_size, &local_workgroup_size,
                                             0,  NULL,NULL);

            }
            //let all computations for the previous row to complete then continue ( this is all non-blocking for the cpu)
            if (ret == CL_SUCCESS){
                ret = clEnqueueBarrierWithWaitList(gpu->commandQueue, 0, NULL, &event[0]);
            }

            totalOffset += width;
            if (ret == CL_SUCCESS && gpu->verbose){
                size_t offset = totalOffset -1;
                ret = clEnqueueNDRangeKernel(gpu->commandQueue, progress_kernel, 1, &offset, &unit, &unit,
                                             0,  NULL, NULL);
            }
        }
    }    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }


    //read the outputs
    unsigned int error_status = 0;
    if (ret == CL_SUCCESS)
        ret = clEnqueueReadBuffer(gpu->commandQueue, error_mem_obj, CL_TRUE, 0, sizeof(unsigned int),
                                  &error_status, 1, &event[0], &event[1]);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }

    if (ret == CL_SUCCESS && error_status > 0) {
        fprintf(stderr, "Kernel returned error!\n");
        ret = error_status;
    }

    if (ret == CL_SUCCESS)
        ret = clEnqueueReadBuffer(gpu->commandQueue, max_mem_obj, CL_TRUE, 0, sizeof(unsigned int),
                                  computed_max_minecraft_y, 1, &event[1], &event[2]);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }

    if (ret == CL_SUCCESS)
        ret = clEnqueueReadBuffer(gpu->commandQueue, output_mem_obj, CL_TRUE, 0, output_size * sizeof(unsigned int),
                                  output, 1, &event[2], &event[3]);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }

    if (ret == CL_SUCCESS)
        ret = clWaitForEvents(1, &event[3]);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }

    //flush remaining tasks
    if (ret == CL_SUCCESS)
        ret = clFlush(gpu->commandQueue);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }

    if (kernel != NULL)
        clReleaseKernel(kernel);
    if (progress_kernel != NULL)
        clReleaseKernel(progress_kernel);

    if (input_mem_obj != NULL)
        clReleaseMemObject(input_mem_obj);
    if (liquid_mem_obj != NULL)
        clReleaseMemObject(liquid_mem_obj);
    if (output_mem_obj != NULL)
        clReleaseMemObject(output_mem_obj);
    if (height_mem_obj != NULL)
        clReleaseMemObject(height_mem_obj);
    if (index_mem_obj != NULL)
        clReleaseMemObject(index_mem_obj);
    if (padding_mem_obj != NULL)
        clReleaseMemObject(padding_mem_obj);
    if (error_mem_obj != NULL)
        clReleaseMemObject(error_mem_obj);
    if (max_mem_obj != NULL)
        clReleaseMemObject(max_mem_obj);

    return ret;
}


int gpu_height_to_stats(gpu_t *gpu, unsigned int *input, unsigned int *layer_count, unsigned int *layer_id_count, unsigned int *id_count, unsigned int width, unsigned int height, unsigned int layers) {
    size_t buffer_size = (size_t)width * height * 3;
    size_t layer_size = layers + 1;
    size_t layer_id_size = (layers + 1) * (UCHAR_MAX + 1);
    size_t id_size = (UCHAR_MAX + 1);
    cl_int ret = 0;
    cl_mem input_mem_obj = NULL;
    cl_mem layer_mem_obj = NULL;
    cl_mem layer_id_mem_obj = NULL;
    cl_mem id_mem_obj = NULL;
    cl_kernel kernel = NULL;

    cl_event event[4];

    //create memory objects

    input_mem_obj = clCreateBuffer(gpu->context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR,
                                   buffer_size * sizeof(unsigned int), input, &ret);

    if (ret == CL_SUCCESS)
        layer_mem_obj = clCreateBuffer(gpu->context, CL_MEM_READ_WRITE,
                                        layer_size * sizeof(unsigned int), NULL, &ret);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }

    if (ret == CL_SUCCESS)
        layer_id_mem_obj = clCreateBuffer(gpu->context, CL_MEM_READ_WRITE,
                                        layer_id_size * sizeof(unsigned int), NULL, &ret);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }

    if (ret == CL_SUCCESS)
        id_mem_obj = clCreateBuffer(gpu->context, CL_MEM_READ_WRITE,
                                        id_size * sizeof(unsigned int), NULL, &ret);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }

    unsigned int zero = 0;
    if (ret == CL_SUCCESS)
        ret = clEnqueueFillBuffer(gpu->commandQueue, layer_mem_obj, &zero, sizeof (unsigned int), 0, layer_size * sizeof(unsigned int), 0, NULL, NULL);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }
    if (ret == CL_SUCCESS)
        ret = clEnqueueFillBuffer(gpu->commandQueue, layer_id_mem_obj, &zero, sizeof (unsigned int), 0, layer_id_size * sizeof(unsigned int), 0, NULL, NULL);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }
    if (ret == CL_SUCCESS)
        ret = clEnqueueFillBuffer(gpu->commandQueue, id_mem_obj, &zero, sizeof (unsigned int), 0, id_size * sizeof(unsigned int), 0, NULL, NULL);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }

    //create kernel
    if (ret == CL_SUCCESS)
        kernel = clCreateKernel(gpu->programs[1].program, "height_to_stats", &ret);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }

    //set kernel arguments
    unsigned char arg_index = 0;
    if (ret == CL_SUCCESS)
        ret = clSetKernelArg(kernel, arg_index++, sizeof(cl_mem), (void *) &input_mem_obj);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }
    if (ret == CL_SUCCESS)
        ret = clSetKernelArg(kernel, arg_index++, sizeof(cl_mem), (void *) &layer_mem_obj);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }
    if (ret == CL_SUCCESS)
        ret = clSetKernelArg(kernel, arg_index++, sizeof(cl_mem), (void *) &layer_id_mem_obj);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }
    if (ret == CL_SUCCESS)
        ret = clSetKernelArg(kernel, arg_index++, sizeof(cl_mem), (void *) &id_mem_obj);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }

    size_t global_item_size = width * height;
    size_t local_item_size = MIN(height, gpu->max_parallelism);
    while (global_item_size % local_item_size != 0) { local_item_size--; }

    if (ret == CL_SUCCESS)
        ret = clEnqueueBarrierWithWaitList(gpu->commandQueue,0, NULL, NULL);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }

    //request the gpu process
    if (ret == CL_SUCCESS)
        ret = clEnqueueNDRangeKernel(gpu->commandQueue, kernel, 1, NULL, &global_item_size, &local_item_size, 0,  NULL,
                                     &event[0]);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }

    //read the outputs
    if (ret == CL_SUCCESS)
        ret = clEnqueueReadBuffer(gpu->commandQueue, layer_mem_obj, CL_TRUE, 0, layer_size * sizeof(unsigned int), layer_count, 0,
                                  NULL, &event[1]);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }

    if (ret == CL_SUCCESS)
        ret = clEnqueueReadBuffer(gpu->commandQueue, layer_id_mem_obj, CL_TRUE, 0, layer_id_size * sizeof(unsigned int), layer_id_count, 0,
    NULL, &event[2]);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }
    if (ret == CL_SUCCESS)
        ret = clEnqueueReadBuffer(gpu->commandQueue, id_mem_obj, CL_TRUE, 0, id_size * sizeof(unsigned int), id_count, 0,
                                  NULL, &event[3]);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }

    //wait for outputs
    if (ret == CL_SUCCESS)
        ret = clWaitForEvents(4, event);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }

    //flush remaining tasks
    if (ret == CL_SUCCESS)
        ret = clFlush(gpu->commandQueue);
    else{
        fprintf(stderr,"Fail at %s:%d code:%d\n",__FILE_NAME__,__LINE__, ret);
        exit(ret);
    }

    if (kernel != NULL)
        clReleaseKernel(kernel);

    if (input_mem_obj != NULL)
        clReleaseMemObject(input_mem_obj);
    if (layer_mem_obj != NULL)
        clReleaseMemObject(layer_mem_obj);
    if (layer_id_mem_obj != NULL)
        clReleaseMemObject(layer_id_mem_obj);
    if (id_mem_obj != NULL)
        clReleaseMemObject(id_mem_obj);

    return ret;
}
// diagonals

index_holder generate_indexes(unsigned int width, unsigned int height, unsigned int steepness){
    index_holder holder = {};
    holder.indexes = t_calloc(width*height*2, sizeof (unsigned int));

    if (steepness == 0){
        holder.diagonal_count = height;
        holder.diagonals = t_calloc(height, sizeof (unsigned int));

        for (unsigned int y = 0; y < height; y++)
            holder.diagonals[y] = width;

        for (unsigned int i = 0; i < width * height; i++){
            holder.indexes[i*2] = i % width;
            holder.indexes[(i*2) + 1] = i / width;
        }

    }else{
        unsigned int i = 0;
        holder.diagonals = t_calloc(width + height * steepness, sizeof (unsigned int));

        for (unsigned int x = 0; x < width; x++){
            unsigned int count = 0;
            for (long c_x = x, c_y = 0; c_x >= 0 && c_y < height; c_y += 1, c_x -= steepness, i++, count++){
                holder.indexes[i*2] = c_x;
                holder.indexes[(i*2) + 1] = c_y;
            }
            holder.diagonals[holder.diagonal_count++] = count;
        }

        for (unsigned int y = 1; y < height; y++) {
            for (long offset = steepness - 1; offset >= 0; offset--) {
                unsigned int count = 0;
                for (long c_x = width - 1 - offset, c_y = y; c_x >= 0 && c_y < height; c_y += 1, c_x -= steepness, i++, count++){
                    holder.indexes[i*2] = c_x;
                    holder.indexes[(i*2) + 1] = c_y;
                }
                holder.diagonals[holder.diagonal_count++] = count;
            }
        }
    }

    return holder;

}