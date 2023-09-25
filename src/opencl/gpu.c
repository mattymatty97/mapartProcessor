#include <stdio.h>
#include "gpu.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

#define RGB_SIZE 3
#define RGBA_SIZE 4
#define MULTIPLIER_SIZE 3

cl_program gpu_compile_program(main_options *config, gpu_t *gpu_holder, char *filename, cl_int *ret);

int gpu_init(main_options *config, gpu_t *gpu_holder) {
    cl_platform_id platform_id[3] = {};
    cl_device_id device_id[3][10] = {};
    char platform_names[3][301] = {};
    char device_names[3][10][301] = {};
    cl_uint ret_num_devices[3] = {};
    cl_uint ret_num_platforms = 0;
    cl_int ret = clGetPlatformIDs(3, platform_id, &ret_num_platforms);
    if (ret == CL_SUCCESS) {
        for (int i = 0; i < ret_num_platforms; i++) {
            ret = clGetPlatformInfo(platform_id[i], CL_PLATFORM_NAME, sizeof(char) * 300, platform_names[i], NULL);
            ret = clGetDeviceIDs(platform_id[i], CL_DEVICE_TYPE_GPU, 10,
                                 device_id[i], &ret_num_devices[i]);
            for (int j = 0; j < ret_num_devices[i]; j++) {
                ret = clGetDeviceInfo(device_id[i][j], CL_DEVICE_NAME, sizeof(char) * 300, device_names[i][j],
                                      NULL);
            }
        }
    }

    fprintf(stdout, "found %d devices on %d platforms: \n",
            (ret_num_devices[0] + ret_num_devices[1] + ret_num_devices[2]), ret_num_platforms);
    for (int i = 0; i < ret_num_platforms; i++) {
        fprintf(stdout, "\t%s: \n", platform_names[i]);
        for (int j = 0; j < ret_num_devices[i]; j++) {
            fprintf(stdout, "\t%2d %2d - %s\n", i, j, device_names[i][j]);
        }
    }

    int platform_index = -1;
    int device_index = -1;
    while (platform_index < 0 || platform_index >= ret_num_platforms || device_index < 0 ||
           device_index >= ret_num_devices[platform_index]) {
        fprintf(stdout, "please select a device to use (%%d %%d):\n");
        fflush(stdout);
        fflush(stdin);
        fscanf(stdin, "%d %d", &platform_index, &device_index);
    }

    fprintf(stdout, "Selected %s from %s\n", device_names[platform_index][device_index],
            platform_names[platform_index]);
    fflush(stdout);

    gpu_holder->platformId = platform_id[platform_index];
    gpu_holder->deviceId = device_id[platform_index][device_index];

    ret = clGetDeviceInfo(gpu_holder->deviceId, CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(size_t),
                          &gpu_holder->max_parallelism, NULL);

    cl_context context;
    if (ret == 0)
        context = gpu_holder->context = clCreateContext(NULL, 1, &device_id[platform_index][device_index], NULL, NULL,
                                                        &ret);

    //if all is ok
    if (ret == 0) {
        gpu_holder->commandQueue = clCreateCommandQueueWithProperties(context, device_id[platform_index][device_index],
                                                                      NULL, &ret);
    }

    //compile programs
    if (ret == 0) {

        gpu_program program = {
                "color_conversion",
                gpu_compile_program(config, gpu_holder, "resources/opencl/color/color_conversions.cl", &ret)
        };

        gpu_holder->programs[0] = program;

    }

    if (ret == 0) {

        gpu_program program = {
                "mapart",
                gpu_compile_program(config, gpu_holder, "resources/opencl/mapart/mapart.cl", &ret)
        };

        gpu_holder->programs[1] = program;

    }

    if (ret == 0) {

        gpu_program program = {
                "gen_dithering",
                gpu_compile_program(config, gpu_holder, "resources/opencl/dithering/error_bleeding.cl", &ret)
        };

        gpu_holder->programs[2] = program;

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
    fread(source_str, 1, length, fp);
    fclose(fp);

    if (*ret == 0) {
        cl_program program = clCreateProgramWithSource(gpu_holder->context, 1, (const char **) &source_str, &length,
                                                       ret);
        if (*ret == 0) {
            *ret = clBuildProgram(program, 1, &gpu_holder->deviceId, NULL, NULL, NULL);
        }

        if (*ret == 0) {
            free(source_str);
            return program;
        } else {
            char build_log[5000] = {};
            size_t log_size = 0;
            clGetProgramBuildInfo(program, gpu_holder->deviceId, CL_PROGRAM_BUILD_LOG, sizeof(char) * 5000, build_log,
                                  &log_size);
            fprintf(stderr, "Error compiling %s:\n%s\n\n", filename, build_log);
        }
    }
    free(source_str);
    return NULL;
}

int gpu_rgb_to_xyz(gpu_t *gpu, int *input, float *output, unsigned int width, unsigned int height) {
    size_t buffer_size = (size_t)width * height * 4;
    cl_int ret = 0;
    cl_mem input_mem_obj = NULL;
    cl_mem output_mem_obj = NULL;
    cl_kernel kernel = NULL;

    cl_event event;

    //create memory objects

    input_mem_obj = clCreateBuffer(gpu->context, CL_MEM_READ_ONLY,
                                   buffer_size * sizeof(int), NULL, &ret);
    if (ret == 0)
        output_mem_obj = clCreateBuffer(gpu->context, CL_MEM_WRITE_ONLY,
                                        buffer_size * sizeof(float), NULL, &ret);

    //copy input into the memory object
    if (ret == 0)
        ret = clEnqueueWriteBuffer(gpu->commandQueue, input_mem_obj, CL_TRUE, 0, buffer_size * sizeof(int), input, 0,
                                   NULL, NULL);

    //create kernel
    if (ret == 0)
        kernel = clCreateKernel(gpu->programs[0].program, "rgb_to_XYZ", &ret);

    //set kernel arguments
    if (ret == 0)
        ret = clSetKernelArg(kernel, 0, sizeof(cl_mem), (void *) &input_mem_obj);
    if (ret == 0)
        ret = clSetKernelArg(kernel, 1, sizeof(cl_mem), (void *) &output_mem_obj);

    size_t global_item_size = width * height;
    size_t local_item_size = MIN(height, gpu->max_parallelism);

    //request the gpu process
    if (ret == 0)
        ret = clEnqueueNDRangeKernel(gpu->commandQueue, kernel, 1, NULL, &global_item_size, &local_item_size, 0, NULL,
                                     &event);

    //read the outputs
    if (ret == 0)
        ret = clEnqueueReadBuffer(gpu->commandQueue, output_mem_obj, CL_TRUE, 0, buffer_size * sizeof(float), output, 1,
                                  &event, &event);

    //wait for outputs
    if (ret == 0)
        ret = clWaitForEvents(1, &event);

    //flush remaining tasks
    if (ret == 0)
        ret = clFlush(gpu->commandQueue);

    if (kernel != NULL)
        clReleaseKernel(kernel);

    if (input_mem_obj != NULL)
        clReleaseMemObject(input_mem_obj);
    if (output_mem_obj != NULL)
        clReleaseMemObject(output_mem_obj);

    return ret;
}

int gpu_xyz_to_lab(gpu_t *gpu, float *input, float *output, unsigned int width, unsigned int height) {
    size_t buffer_size = (size_t)width * height * 4;
    cl_int ret = 0;
    cl_mem input_mem_obj = NULL;
    cl_mem output_mem_obj = NULL;
    cl_kernel kernel = NULL;

    cl_event event;

    //create memory objects

    input_mem_obj = clCreateBuffer(gpu->context, CL_MEM_READ_ONLY,
                                   buffer_size * sizeof(float), NULL, &ret);
    if (ret == 0)
        output_mem_obj = clCreateBuffer(gpu->context, CL_MEM_WRITE_ONLY,
                                        buffer_size * sizeof(float), NULL, &ret);

    //copy input into the memory object
    if (ret == 0)
        ret = clEnqueueWriteBuffer(gpu->commandQueue, input_mem_obj, CL_TRUE, 0, buffer_size * sizeof(float), input, 0,
                                   NULL, NULL);

    //create kernel
    if (ret == 0)
        kernel = clCreateKernel(gpu->programs[0].program, "xyz_to_lab", &ret);

    //set kernel arguments
    if (ret == 0)
        ret = clSetKernelArg(kernel, 0, sizeof(cl_mem), (void *) &input_mem_obj);
    if (ret == 0)
        ret = clSetKernelArg(kernel, 1, sizeof(cl_mem), (void *) &output_mem_obj);

    size_t global_item_size = (size_t)width * height;
    size_t local_item_size = MIN(height, gpu->max_parallelism);

    //request the gpu process
    if (ret == 0)
        ret = clEnqueueNDRangeKernel(gpu->commandQueue, kernel, 1, NULL, &global_item_size, &local_item_size, 0, NULL,
                                     &event);

    //read the outputs
    if (ret == 0)
        ret = clEnqueueReadBuffer(gpu->commandQueue, output_mem_obj, CL_TRUE, 0, buffer_size * sizeof(float), output, 1,
                                  &event, &event);

    //wait for outputs
    if (ret == 0)
        ret = clWaitForEvents(1, &event);

    //flush remaining tasks
    if (ret == 0)
        ret = clFlush(gpu->commandQueue);

    if (kernel != NULL)
        clReleaseKernel(kernel);

    if (input_mem_obj != NULL)
        clReleaseMemObject(input_mem_obj);
    if (output_mem_obj != NULL)
        clReleaseMemObject(output_mem_obj);

    return ret;
}

int gpu_xyz_to_luv(gpu_t *gpu, float *input, float *output, unsigned int width, unsigned int height) {
    size_t buffer_size = (size_t)width * height * 4;
    cl_int ret = 0;
    cl_mem input_mem_obj = NULL;
    cl_mem output_mem_obj = NULL;
    cl_kernel kernel = NULL;

    cl_event event;

    //create memory objects

    input_mem_obj = clCreateBuffer(gpu->context, CL_MEM_READ_ONLY,
                                   buffer_size * sizeof(float), NULL, &ret);
    if (ret == 0)
        output_mem_obj = clCreateBuffer(gpu->context, CL_MEM_WRITE_ONLY,
                                        buffer_size * sizeof(float), NULL, &ret);

    //copy input into the memory object
    if (ret == 0)
        ret = clEnqueueWriteBuffer(gpu->commandQueue, input_mem_obj, CL_TRUE, 0, buffer_size * sizeof(float), input, 0,
                                   NULL, NULL);

    //create kernel
    if (ret == 0)
        kernel = clCreateKernel(gpu->programs[0].program, "xyz_to_luv", &ret);

    //set kernel arguments
    if (ret == 0)
        ret = clSetKernelArg(kernel, 0, sizeof(cl_mem), (void *) &input_mem_obj);
    if (ret == 0)
        ret = clSetKernelArg(kernel, 1, sizeof(cl_mem), (void *) &output_mem_obj);

    size_t global_item_size = (size_t)width * height;
    size_t local_item_size = MIN(height, gpu->max_parallelism);

    //request the gpu process
    if (ret == 0)
        ret = clEnqueueNDRangeKernel(gpu->commandQueue, kernel, 1, NULL, &global_item_size, &local_item_size, 0, NULL,
                                     &event);

    //read the outputs
    if (ret == 0)
        ret = clEnqueueReadBuffer(gpu->commandQueue, output_mem_obj, CL_TRUE, 0, buffer_size * sizeof(float), output, 1,
                                  &event, &event);

    //wait for outputs
    if (ret == 0)
        ret = clWaitForEvents(1, &event);

    //flush remaining tasks
    if (ret == 0)
        ret = clFlush(gpu->commandQueue);

    if (kernel != NULL)
        clReleaseKernel(kernel);

    if (input_mem_obj != NULL)
        clReleaseMemObject(input_mem_obj);
    if (output_mem_obj != NULL)
        clReleaseMemObject(output_mem_obj);

    return ret;
}

int gpu_lab_to_lch(gpu_t *gpu, float *input, float *output, unsigned int width, unsigned int height) {
    size_t buffer_size = (size_t)width * height * 4;
    cl_int ret = 0;
    cl_mem input_mem_obj = NULL;
    cl_mem output_mem_obj = NULL;
    cl_kernel kernel = NULL;

    cl_event event;

    //create memory objects

    input_mem_obj = clCreateBuffer(gpu->context, CL_MEM_READ_ONLY,
                                   buffer_size * sizeof(float), NULL, &ret);
    if (ret == 0)
        output_mem_obj = clCreateBuffer(gpu->context, CL_MEM_WRITE_ONLY,
                                        buffer_size * sizeof(float), NULL, &ret);

    //copy input into the memory object
    if (ret == 0)
        ret = clEnqueueWriteBuffer(gpu->commandQueue, input_mem_obj, CL_TRUE, 0, buffer_size * sizeof(float), input, 0,
                                   NULL, NULL);

    //create kernel
    if (ret == 0)
        kernel = clCreateKernel(gpu->programs[0].program, "lab_to_lch", &ret);

    //set kernel arguments
    if (ret == 0)
        ret = clSetKernelArg(kernel, 0, sizeof(cl_mem), (void *) &input_mem_obj);
    if (ret == 0)
        ret = clSetKernelArg(kernel, 1, sizeof(cl_mem), (void *) &output_mem_obj);

    size_t global_item_size = (size_t)width * height;
    size_t local_item_size = MIN(height, gpu->max_parallelism);

    //request the gpu process
    if (ret == 0)
        ret = clEnqueueNDRangeKernel(gpu->commandQueue, kernel, 1, NULL, &global_item_size, &local_item_size, 0, NULL,
                                     &event);

    //read the outputs
    if (ret == 0)
        ret = clEnqueueReadBuffer(gpu->commandQueue, output_mem_obj, CL_TRUE, 0, buffer_size * sizeof(float), output, 1,
                                  &event, &event);

    //wait for outputs
    if (ret == 0)
        ret = clWaitForEvents(1, &event);

    //flush remaining tasks
    if (ret == 0)
        ret = clFlush(gpu->commandQueue);

    if (kernel != NULL)
        clReleaseKernel(kernel);

    if (input_mem_obj != NULL)
        clReleaseMemObject(input_mem_obj);
    if (output_mem_obj != NULL)
        clReleaseMemObject(output_mem_obj);

    return ret;
}

int gpu_lch_to_lab(gpu_t *gpu, float *input, float *output, unsigned int width, unsigned int height) {
    size_t buffer_size = (size_t)width * height * 4;
    cl_int ret = 0;
    cl_mem input_mem_obj = NULL;
    cl_mem output_mem_obj = NULL;
    cl_kernel kernel = NULL;

    cl_event event;

    //create memory objects

    input_mem_obj = clCreateBuffer(gpu->context, CL_MEM_READ_ONLY,
                                   buffer_size * sizeof(float), NULL, &ret);
    if (ret == 0)
        output_mem_obj = clCreateBuffer(gpu->context, CL_MEM_WRITE_ONLY,
                                        buffer_size * sizeof(float), NULL, &ret);

    //copy input into the memory object
    if (ret == 0)
        ret = clEnqueueWriteBuffer(gpu->commandQueue, input_mem_obj, CL_TRUE, 0, buffer_size * sizeof(float), input, 0,
                                   NULL, NULL);

    //create kernel
    if (ret == 0)
        kernel = clCreateKernel(gpu->programs[0].program, "lch_to_lab", &ret);

    //set kernel arguments
    if (ret == 0)
        ret = clSetKernelArg(kernel, 0, sizeof(cl_mem), (void *) &input_mem_obj);
    if (ret == 0)
        ret = clSetKernelArg(kernel, 1, sizeof(cl_mem), (void *) &output_mem_obj);

    size_t global_item_size = width * height;
    size_t local_item_size = MIN(height, gpu->max_parallelism);

    //request the gpu process
    if (ret == 0)
        ret = clEnqueueNDRangeKernel(gpu->commandQueue, kernel, 1, NULL, &global_item_size, &local_item_size, 0, NULL,
                                     &event);

    //read the outputs
    if (ret == 0)
        ret = clEnqueueReadBuffer(gpu->commandQueue, output_mem_obj, CL_TRUE, 0, buffer_size * sizeof(float), output, 1,
                                  &event, &event);

    //wait for outputs
    if (ret == 0)
        ret = clWaitForEvents(1, &event);

    //flush remaining tasks
    if (ret == 0)
        ret = clFlush(gpu->commandQueue);

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
    size_t global_workgroup_size = width;
    size_t local_workgroup_size = MIN(width, gpu->max_parallelism);
    while (global_workgroup_size % local_workgroup_size != 0) { local_workgroup_size--; }

    cl_event event;

    cl_int ret = 0;
    cl_mem input_mem_obj = NULL;
    cl_mem palette_mem_obj = NULL;
    cl_mem palette_id_mem_obj = NULL;
    cl_mem palette_liquid_mem_obj = NULL;
    cl_mem noise_mem_obj = NULL;
    cl_mem error_buf_mem_obj = NULL;
    cl_mem workgroup_rider_mem_obj = NULL;
    cl_mem workgroup_progress_mem_obj = NULL;
    cl_mem output_mem_obj = NULL;
    cl_mem bleeding_mem_obj = NULL;
    cl_kernel kernel = NULL;

    //create memory objects

    input_mem_obj = clCreateBuffer(gpu->context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR,
                                   buffer_size * sizeof(float), input, &ret);
    if (ret == 0)
        output_mem_obj = clCreateBuffer(gpu->context, CL_MEM_WRITE_ONLY,
                                        output_size * sizeof(unsigned char), NULL, &ret);
    if (ret == 0)
        palette_mem_obj = clCreateBuffer(gpu->context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR,
                                         palette_size * sizeof(float), palette, &ret);
    if (ret == 0)
        palette_id_mem_obj = clCreateBuffer(gpu->context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR,
                                            palette_indexes * sizeof(unsigned char), valid_palette_ids, &ret);
    if (ret == 0)
        palette_liquid_mem_obj = clCreateBuffer(gpu->context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR,
                                            palette_indexes * sizeof(unsigned char), liquid_palette_ids, &ret);
    if (ret == 0)
        noise_mem_obj = clCreateBuffer(gpu->context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR,
                                       noise_size * sizeof(float), noise, &ret);
    if (ret == 0)
        error_buf_mem_obj = clCreateBuffer(gpu->context, CL_MEM_READ_WRITE,
                                           buffer_size * sizeof(float), NULL, &ret);
    if (ret == 0)
        workgroup_rider_mem_obj = clCreateBuffer(gpu->context, CL_MEM_READ_WRITE,
                                                 sizeof(unsigned int), NULL, &ret);
    if (ret == 0)
        workgroup_progress_mem_obj = clCreateBuffer(gpu->context, CL_MEM_READ_WRITE,
                                                    global_workgroup_size * sizeof(unsigned int), NULL, &ret);
    if (ret == 0 && bleeding_count > 0)
        bleeding_mem_obj = clCreateBuffer(gpu->context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR,
                                          bleeding_size * sizeof(int), bleeding_params, &ret);

    //request clean the error buffer
    float pattern = 0;

    if (ret == 0)
        ret = clEnqueueFillBuffer(gpu->commandQueue, error_buf_mem_obj, &pattern, sizeof (float), 0, buffer_size * sizeof(float), 0, NULL, &event);

    unsigned int i_pattern = 0;
    if (ret == 0)
        ret = clEnqueueFillBuffer(gpu->commandQueue, workgroup_rider_mem_obj, &i_pattern, sizeof(unsigned int), 0, sizeof(unsigned int), 0, NULL, &event);
    if (ret == 0)
        ret = clEnqueueFillBuffer(gpu->commandQueue, workgroup_progress_mem_obj, &i_pattern, sizeof(unsigned int), 0, global_workgroup_size * sizeof(unsigned int), 0, NULL, &event);

    //create kernel
    if (ret == 0)
        kernel = clCreateKernel(gpu->programs[2].program, "Error_bleed_dither_by_cols", &ret);
    unsigned char arg_index = 0;
    //set kernel arguments
    if (ret == 0)
        ret = clSetKernelArg(kernel, arg_index++, sizeof(cl_mem), (void *) &input_mem_obj);
    if (ret == 0)
        ret = clSetKernelArg(kernel, arg_index++, sizeof(cl_mem), (void *) &output_mem_obj);
    if (ret == 0)
        ret = clSetKernelArg(kernel, arg_index++, sizeof(cl_mem), (void *) &error_buf_mem_obj);
    if (ret == 0)
        ret = clSetKernelArg(kernel, arg_index++, sizeof(cl_mem), (void *) &palette_mem_obj);
    if (ret == 0)
        ret = clSetKernelArg(kernel, arg_index++, sizeof(cl_mem), (void *) &palette_id_mem_obj);
    if (ret == 0)
        ret = clSetKernelArg(kernel, arg_index++, sizeof(cl_mem), (void *) &palette_liquid_mem_obj);
    if (ret == 0)
        ret = clSetKernelArg(kernel, arg_index++, sizeof(cl_mem), (void *) &noise_mem_obj);
    if (ret == 0)
        ret = clSetKernelArg(kernel, arg_index++, sizeof(cl_mem), (void *) &workgroup_rider_mem_obj);
    if (ret == 0)
        ret = clSetKernelArg(kernel, arg_index++, sizeof(cl_mem), (void *) &workgroup_progress_mem_obj);
    if (ret == 0)
        ret = clSetKernelArg(kernel, arg_index++, sizeof(cl_int) * global_workgroup_size, NULL);
    if (ret == 0)
        ret = clSetKernelArg(kernel, arg_index++, sizeof(const unsigned int), (void *) &width);
    if (ret == 0)
        ret = clSetKernelArg(kernel, arg_index++, sizeof(const unsigned int), (void *) &height);
    if (ret == 0)
        ret = clSetKernelArg(kernel, arg_index++, sizeof(const unsigned char), (void *) &palette_indexes);
    if (ret == 0)
        ret = clSetKernelArg(kernel, arg_index++, sizeof(cl_mem), (void *) &bleeding_mem_obj);
    if (ret == 0)
        ret = clSetKernelArg(kernel, arg_index++, sizeof(const unsigned char), (void *) &bleeding_count);
    if (ret == 0)
        ret = clSetKernelArg(kernel, arg_index++, sizeof(const unsigned char), (void *) &min_required_pixels);
    if (ret == 0)
        ret = clSetKernelArg(kernel, arg_index++, sizeof(const int), (void *) &max_minecraft_y);


    //request the gpu process
    if (ret == 0)
        ret = clEnqueueNDRangeKernel(gpu->commandQueue, kernel, 1, NULL, &global_workgroup_size, &local_workgroup_size,
                                     0, NULL, &event);

    //read the outputs
    if (ret == 0)
        ret = clEnqueueReadBuffer(gpu->commandQueue, output_mem_obj, CL_TRUE, 0, output_size * sizeof(unsigned char),
                                  output, 1, &event, &event);

    if (ret == 0)
        ret = clWaitForEvents(1, &event);

    //flush remaining tasks
    if (ret == 0)
        ret = clFlush(gpu->commandQueue);

    if (kernel != NULL)
        clReleaseKernel(kernel);

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
    if (workgroup_rider_mem_obj != NULL)
        clReleaseMemObject(workgroup_rider_mem_obj);
    if (workgroup_progress_mem_obj != NULL)
        clReleaseMemObject(workgroup_progress_mem_obj);
    if (bleeding_mem_obj != NULL)
        clReleaseMemObject(bleeding_mem_obj);

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
            {0, 1,  7, 16},
            {1, -1, 3, 16},
            {1, 0,  5, 16},
            {1, 1,  1, 16}
    };
    return gpu_internal_dither_error_bleed(gpu, input, output, palette, valid_palette_ids, liquid_palette_ids, noise, width, height, palette_indexes,
                                           (int *) bleeding_parameters, 4, 2, max_minecraft_y);
}

int gpu_dither_JJND(gpu_t *gpu, float *input, unsigned char *output, float *palette, unsigned char *valid_palette_ids, unsigned char *liquid_palette_ids, float *noise, unsigned int width,
                    unsigned int height, unsigned char palette_indexes,
                    int max_minecraft_y) {
    int bleeding_parameters[12][4] = {
            {0, 1,  7, 48},
            {0, 2,  5, 48},
            {1, -2, 3, 48},
            {1, -1, 5, 48},
            {1, 0,  7, 48},
            {1, 1,  5, 48},
            {1, 2,  3, 48},
            {2, -2, 1, 48},
            {2, -1, 3, 48},
            {2, 0,  5, 48},
            {2, 1,  3, 48},
            {2, 2,  1, 48}
    };
    return gpu_internal_dither_error_bleed(gpu, input, output, palette, valid_palette_ids, liquid_palette_ids, noise, width, height, palette_indexes,
                                           (int *) bleeding_parameters, 12, 3, max_minecraft_y);
}

int gpu_dither_Stucki(gpu_t *gpu, float *input, unsigned char *output, float *palette, unsigned char *valid_palette_ids, unsigned char *liquid_palette_ids, float *noise, unsigned int width,
                    unsigned int height, unsigned char palette_indexes,
                    int max_minecraft_y) {
    int bleeding_parameters[12][4] = {
            {0, 1,  8, 42},
            {0, 2,  4, 42},
            {1, -2, 2, 42},
            {1, -1, 4, 42},
            {1, 0,  8, 42},
            {1, 1,  4, 42},
            {1, 2,  2, 42},
            {2, -2, 1, 42},
            {2, -1, 2, 42},
            {2, 0,  4, 42},
            {2, 1,  2, 42},
            {2, 2,  1, 42}
    };
    return gpu_internal_dither_error_bleed(gpu, input, output, palette, valid_palette_ids, liquid_palette_ids, noise, width, height, palette_indexes,
                                           (int *) bleeding_parameters, 12, 3, max_minecraft_y);
}

int gpu_dither_Atkinson(gpu_t *gpu, float *input, unsigned char *output, float *palette, unsigned char *valid_palette_ids, unsigned char *liquid_palette_ids, float *noise, unsigned int width,
                    unsigned int height, unsigned char palette_indexes,
                    int max_minecraft_y) {
    int bleeding_parameters[6][4] = {
            {0, 1,  1, 8},
            {0, 2,  1, 8},
            {1, -1, 1, 8},
            {1, 0,  1, 8},
            {1, 1,  1, 8},
            {2, 0,  1, 8}
    };
    return gpu_internal_dither_error_bleed(gpu, input, output, palette, valid_palette_ids, liquid_palette_ids, noise, width, height, palette_indexes,
                                           (int *) bleeding_parameters, 6, 3, max_minecraft_y);
}

int gpu_dither_Burkes(gpu_t *gpu, float *input, unsigned char *output, float *palette, unsigned char *valid_palette_ids, unsigned char *liquid_palette_ids, float *noise, unsigned int width,
                    unsigned int height, unsigned char palette_indexes,
                    int max_minecraft_y) {
    int bleeding_parameters[7][4] = {
            {0, 1,  8, 32},
            {0, 2,  4, 32},
            {1, -2, 2, 32},
            {1, -1, 4, 32},
            {1, 0,  8, 32},
            {1, 1,  4, 32},
            {1, 2,  2, 32}
    };
    return gpu_internal_dither_error_bleed(gpu, input, output, palette, valid_palette_ids, liquid_palette_ids, noise, width, height, palette_indexes,
                                           (int *) bleeding_parameters, 7, 3, max_minecraft_y);
}

int gpu_dither_Sierra(gpu_t *gpu, float *input, unsigned char *output, float *palette, unsigned char *valid_palette_ids, unsigned char *liquid_palette_ids, float *noise, unsigned int width,
                    unsigned int height, unsigned char palette_indexes,
                    int max_minecraft_y) {
    int bleeding_parameters[10][4] = {
            {0, 1,  5, 32},
            {0, 2,  3, 32},
            {1, -2, 2, 32},
            {1, -1, 4, 32},
            {1, 0,  5, 32},
            {1, 1,  4, 32},
            {1, 2,  2, 32},
            {2, -1, 2, 32},
            {2, 0,  3, 32},
            {2, 1,  2, 32}
    };
    return gpu_internal_dither_error_bleed(gpu, input, output, palette, valid_palette_ids, liquid_palette_ids, noise, width, height, palette_indexes,
                                           (int *) bleeding_parameters, 10, 3, max_minecraft_y);
}

int gpu_dither_Sierra2(gpu_t *gpu, float *input, unsigned char *output, float *palette, unsigned char *valid_palette_ids, unsigned char *liquid_palette_ids, float *noise, unsigned int width,
                    unsigned int height, unsigned char palette_indexes,
                    int max_minecraft_y) {
    int bleeding_parameters[7][4] = {
            {0, 1,  4, 16},
            {0, 2,  3, 16},
            {1, -2, 1, 16},
            {1, -1, 2, 16},
            {1, 0,  3, 16},
            {1, 1,  2, 16},
            {1, 2,  1, 16}
    };
    return gpu_internal_dither_error_bleed(gpu, input, output, palette, valid_palette_ids, liquid_palette_ids, noise, width, height, palette_indexes,
                                           (int *) bleeding_parameters, 7, 3, max_minecraft_y);
}

int gpu_dither_SierraL(gpu_t *gpu, float *input, unsigned char *output, float *palette, unsigned char *valid_palette_ids, unsigned char *liquid_palette_ids, float *noise, unsigned int width,
                    unsigned int height, unsigned char palette_indexes,
                    int max_minecraft_y) {
    int bleeding_parameters[3][4] = {
            {0, 1,  2, 4},
            {1, -1, 1, 4},
            {1, 0,  1, 4}
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

    //create memory objects

    input_mem_obj = clCreateBuffer(gpu->context, CL_MEM_READ_ONLY,
                                   buffer_size * sizeof(unsigned char), NULL, &ret);
    if (ret == 0)
        palette_mem_obj = clCreateBuffer(gpu->context, CL_MEM_READ_ONLY,
                                         palette_size * sizeof(int), NULL, &ret);
    if (ret == 0)
        output_mem_obj = clCreateBuffer(gpu->context, CL_MEM_WRITE_ONLY,
                                        output_size * sizeof(unsigned char), NULL, &ret);

    //copy input into the memory object
    if (ret == 0)
        ret = clEnqueueWriteBuffer(gpu->commandQueue, input_mem_obj, CL_TRUE, 0, buffer_size * sizeof(unsigned char),
                                   input, 0, NULL, NULL);
    if (ret == 0)
        ret = clEnqueueWriteBuffer(gpu->commandQueue, palette_mem_obj, CL_TRUE, 0, palette_size * sizeof(int), palette,
                                   0, NULL, NULL);

    //create kernel
    if (ret == 0)
        kernel = clCreateKernel(gpu->programs[1].program, "palette_to_rgb", &ret);

    //set kernel arguments
    if (ret == 0)
        ret = clSetKernelArg(kernel, 0, sizeof(cl_mem), (void *) &input_mem_obj);
    if (ret == 0)
        ret = clSetKernelArg(kernel, 1, sizeof(cl_mem), (void *) &palette_mem_obj);
    if (ret == 0)
        ret = clSetKernelArg(kernel, 2, sizeof(cl_mem), (void *) &output_mem_obj);
    if (ret == 0)
        ret = clSetKernelArg(kernel, 3, sizeof(const unsigned char), (void *) &palette_indexes);
    if (ret == 0)
        ret = clSetKernelArg(kernel, 4, sizeof(const unsigned char), (void *) &palette_variations);

    size_t global_item_size = width * height;
    size_t local_item_size = MIN(height, gpu->max_parallelism);

    //request the gpu process
    if (ret == 0)
        ret = clEnqueueNDRangeKernel(gpu->commandQueue, kernel, 1, NULL, &global_item_size, &local_item_size, 0, NULL,
                                     NULL);

    //read the outputs
    if (ret == 0)
        ret = clEnqueueReadBuffer(gpu->commandQueue, output_mem_obj, CL_TRUE, 0, output_size * sizeof(unsigned char),
                                  output, 0, NULL, NULL);

    //flush remaining tasks
    if (ret == 0)
        ret = clFlush(gpu->commandQueue);

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
    //iterate vertically for mc compatibility
    size_t global_workgroup_size = width;
    size_t local_workgroup_size = MIN(width, gpu->max_parallelism);
    while (global_workgroup_size % local_workgroup_size != 0) { local_workgroup_size--; }

    cl_event event;

    cl_int ret = 0;
    cl_mem input_mem_obj = NULL;
    cl_mem liquid_mem_obj = NULL;
    cl_mem workgroup_rider_mem_obj = NULL;
    cl_mem error_mem_obj = NULL;
    cl_mem output_mem_obj = NULL;
    cl_mem max_mem_obj = NULL;
    cl_kernel kernel = NULL;

    //create memory objects

    input_mem_obj = clCreateBuffer(gpu->context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR,
                                   buffer_size * sizeof(unsigned char), input, &ret);
    if (ret ==0){
        liquid_mem_obj = clCreateBuffer(gpu->context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR,
                                       palette_size * sizeof(unsigned char), is_liquid, &ret);
    }

    if (ret == 0)
        output_mem_obj = clCreateBuffer(gpu->context, CL_MEM_READ_WRITE,
                                        output_size * sizeof(unsigned int), NULL, &ret);
    if (ret == 0)
        workgroup_rider_mem_obj = clCreateBuffer(gpu->context, CL_MEM_READ_WRITE,
                                                 sizeof(unsigned int), NULL, &ret);
    if (ret == 0)
        error_mem_obj = clCreateBuffer(gpu->context, CL_MEM_READ_WRITE,
                                                 sizeof(unsigned int), NULL, &ret);
    if (ret == 0)
        max_mem_obj = clCreateBuffer(gpu->context, CL_MEM_READ_WRITE,sizeof(unsigned int), NULL, &ret);

    //request clean the error indicator
    unsigned int pattern = 0;

    if (ret == 0)
        ret = clEnqueueFillBuffer(gpu->commandQueue, error_mem_obj, &pattern, sizeof(unsigned int), 0, sizeof(unsigned int), 0, NULL, &event);
    if (ret == 0)
        ret = clEnqueueFillBuffer(gpu->commandQueue, workgroup_rider_mem_obj, &pattern, sizeof(unsigned int), 0, sizeof(unsigned int), 0, NULL, &event);
    if (ret == 0)
        ret = clEnqueueFillBuffer(gpu->commandQueue, max_mem_obj, &pattern, sizeof(unsigned int), 0, sizeof(unsigned int), 0, NULL, &event);


    //create kernel
    if (ret == 0)
        kernel = clCreateKernel(gpu->programs[1].program, "palette_to_height", &ret);
    unsigned char arg_index = 0;
    //set kernel arguments
    if (ret == 0)
        ret = clSetKernelArg(kernel, arg_index++, sizeof(cl_mem), (void *) &input_mem_obj);
    if (ret == 0)
        ret = clSetKernelArg(kernel, arg_index++, sizeof(cl_mem), (void *) &liquid_mem_obj);
    if (ret == 0)
        ret = clSetKernelArg(kernel, arg_index++, sizeof(cl_mem), (void *) &output_mem_obj);
    if (ret == 0)
        ret = clSetKernelArg(kernel, arg_index++, sizeof(cl_mem), (void *) &workgroup_rider_mem_obj);
    if (ret == 0)
        ret = clSetKernelArg(kernel, arg_index++, sizeof(cl_mem), (void *) &error_mem_obj);
    if (ret == 0)
        ret = clSetKernelArg(kernel, arg_index++, sizeof(const unsigned int), (void *) &width);
    if (ret == 0)
        ret = clSetKernelArg(kernel, arg_index++, sizeof(const unsigned int), (void *) &height);
    if (ret == 0)
        ret = clSetKernelArg(kernel, arg_index++, sizeof(const int), (void *) &max_minecraft_y);
    if (ret == 0)
        ret = clSetKernelArg(kernel, arg_index++, sizeof(cl_mem), (void *) &max_mem_obj);

    //request the gpu process
    if (ret == 0)
        ret = clEnqueueNDRangeKernel(gpu->commandQueue, kernel, 1, NULL, &global_workgroup_size, &local_workgroup_size,
                                     0, NULL, &event);

    //read the outputs
    unsigned int error_status = 0;
    if (ret == 0)
        ret = clEnqueueReadBuffer(gpu->commandQueue, error_mem_obj, CL_TRUE, 0, sizeof(unsigned int),
                                  &error_status, 1, &event, &event);

    if (error_status > 0) {
        fprintf(stderr, "Kernel returned error!\n");
        ret = error_status;
    }

    if (ret == 0)
        ret = clEnqueueReadBuffer(gpu->commandQueue, max_mem_obj, CL_TRUE, 0, sizeof(unsigned int),
                                  computed_max_minecraft_y, 1, &event, &event);

    if (ret == 0)
        ret = clEnqueueReadBuffer(gpu->commandQueue, output_mem_obj, CL_TRUE, 0, output_size * sizeof(unsigned int),
                                  output, 1, &event, &event);

    if (ret == 0)
        ret = clWaitForEvents(1, &event);

    //flush remaining tasks
    if (ret == 0)
        ret = clFlush(gpu->commandQueue);

    if (kernel != NULL)
        clReleaseKernel(kernel);

    if (input_mem_obj != NULL)
        clReleaseMemObject(input_mem_obj);
    if (liquid_mem_obj != NULL)
        clReleaseMemObject(liquid_mem_obj);
    if (output_mem_obj != NULL)
        clReleaseMemObject(output_mem_obj);
    if (workgroup_rider_mem_obj != NULL)
        clReleaseMemObject(workgroup_rider_mem_obj);
    if (error_mem_obj != NULL)
        clReleaseMemObject(error_mem_obj);
    if (max_mem_obj != NULL)
        clReleaseMemObject(max_mem_obj);

    return ret;
}