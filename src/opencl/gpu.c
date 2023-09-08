#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "gpu.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

cl_program gpu_compile_program(gpu_t * gpu_holder, char * foldername, cl_int * ret);

int gpu_init(gpu_t* gpu_holder){
    cl_platform_id platform_id[3] = {};
    cl_device_id device_id[3][10] = {};
    char platform_names[3][301] = {};
    char device_names[3][10][301] = {};
    cl_uint ret_num_devices[3] = {};
    cl_uint ret_num_platforms = 0;
    cl_int ret = clGetPlatformIDs(3, platform_id, &ret_num_platforms);
    if (ret == CL_SUCCESS) {
        for (int i = 0; i < ret_num_platforms; i++) {
            ret = clGetPlatformInfo(platform_id[i], CL_PLATFORM_NAME, sizeof(char) * 300, platform_names[i],NULL);
            ret = clGetDeviceIDs(platform_id[i], CL_DEVICE_TYPE_GPU, 10,
                                 device_id[i], &ret_num_devices[i]);
            for (int j = 0; j < ret_num_devices[i]; j++) {
                ret = clGetDeviceInfo(device_id[i][j], CL_DEVICE_NAME, sizeof(char) * 300, device_names[i][j],
                                      NULL);
            }
        }
    }

    fprintf(stdout, "found %d devices on %d platforms: \n",
            (ret_num_devices[0] + ret_num_devices[1] + ret_num_devices[2]),ret_num_platforms);
    for (int i = 0; i < ret_num_platforms; i++) {
        fprintf(stdout, "\t%s: \n", platform_names[i]);
       for (int j = 0; j < ret_num_devices[i]; j++) {
           fprintf(stdout, "\t%2d %2d - %s\n", i, j, device_names[i][j]);
        }
    }

    int platform_index = -1;
    int device_index = -1;
    while (platform_index < 0 || platform_index >= ret_num_platforms || device_index < 0 || device_index >= ret_num_devices[platform_index]){
        fprintf(stdout, "please select a device to use (%%d %%d):\n");
        fflush(stdout);
        fflush(stdin);
        fscanf(stdin, "%d %d", &platform_index, &device_index);
    }

    fprintf(stdout, "Selected %s from %s\n", device_names[platform_index][device_index], platform_names[platform_index]);
    fflush(stdout);

    gpu_holder->platformId = platform_id[platform_index];
    gpu_holder->deviceId = device_id[platform_index][device_index];

    cl_context context = gpu_holder->context = clCreateContext(NULL, 1, &device_id[platform_index][device_index], NULL, NULL, &ret);

    //if all is ok
    if (ret == 0){
        gpu_holder->commandQueue = clCreateCommandQueueWithProperties(context, device_id[platform_index][device_index], NULL, &ret);
    }

    //compile programs
    if (ret == 0){

        gpu_program program = {
                "color_conversion",
                gpu_compile_program(gpu_holder,"resources/opencl/color", &ret)
        };

        gpu_holder->programs[0] = program;

    }

    return ret;

}

void gpu_clear(gpu_t* gpu_holder){
    clFlush(gpu_holder->commandQueue);
    clFinish(gpu_holder->commandQueue);
    for (int i= 0; i< ARRAY_SIZE(gpu_holder->programs); i++)
        clReleaseProgram((gpu_holder->programs)[i].program);
    clReleaseCommandQueue(gpu_holder->commandQueue);
    clReleaseContext(gpu_holder->context);
}

void gpu_read_folder_recursive(char * foldername, char ***sources, size_t ** lenghts, unsigned int* count, cl_int* ret){
    struct dirent *de;
    DIR *dr = opendir(foldername);

    if (dr == NULL)  // opendir returns NULL if it couldn't open directory
    {
        printf("Could not open current directory" );
        *ret = 3;
        return;
    }

    char filename_qfd[100] = {} ;

    while ((de = readdir(dr)) != NULL){
        //skip system entries
        if (strcmp(de->d_name, ".")==0 || strcmp(de->d_name, "..")==0)
            continue;

        struct stat stbuf ;
        sprintf( filename_qfd , "%s/%s", foldername ,de->d_name) ;
        if( stat(filename_qfd,&stbuf ) == -1 )
        {
            printf("Unable to stat file: %s\n", filename_qfd) ;
            continue ;
        }
        if ( ( stbuf.st_mode & S_IFMT ) == S_IFDIR )
        {
            gpu_read_folder_recursive(filename_qfd, sources, lenghts, count, ret);
            if (ret != 0)
                return;
        }
        else
        {
            size_t filename_len = strlen(filename_qfd);
            if (strncmp(filename_qfd + (filename_len -3), ".cl", 3)==0) {
                FILE *fp;
                fp = fopen(filename_qfd, "r");
                if (!fp) {
                    fprintf(stderr, "Failed to load file %s.\n", filename_qfd);
                    *ret = 5;
                    return;
                }
                fseek(fp, 0, SEEK_END);
                long length = ftell(fp);
                fseek(fp, 0, SEEK_SET);
                unsigned int index = *count;
                *count = index + 1;
                *sources = realloc(*sources, sizeof(char *) * (*count));
                *lenghts = realloc(*lenghts, sizeof(size_t) * (*count));
                (*lenghts)[index] = length;
                char * source_str = (*sources)[index] = calloc(length + 1, sizeof(char));
                fread(source_str, 1, length, fp);
                fclose(fp);
            }
        }
    }
    fflush(stdout);
    closedir(dr);

}

cl_program gpu_compile_program(gpu_t * gpu_holder, char * foldername, cl_int * ret) {
    unsigned int count = 0;
    char **source_str = malloc(1);
    size_t *length = malloc(1);

    gpu_read_folder_recursive(foldername, &source_str, &length, &count, ret);

    if (*ret == 0) {
        if (count > 0) {
            cl_program program = clCreateProgramWithSource(gpu_holder->context, count, (const char **) source_str, length,
                                                           ret);
            if (*ret == 0) {
                *ret = clBuildProgram(program, 1, &gpu_holder->deviceId, NULL, NULL, NULL);
            }

            if (*ret == 0) {
                return program;
            }
        }
    }
    return NULL;
}

int gpu_rgb_to_xyz(gpu_t *gpu, int *input, float*output, unsigned int x, unsigned int y, unsigned char channels){
    unsigned long buffer_size = x * y * channels;
    cl_int ret = 0;
    cl_mem input_mem_obj = NULL;
    cl_mem output_mem_obj = NULL;
    cl_kernel kernel = NULL;

    //create memory objects

    input_mem_obj = clCreateBuffer(gpu->context, CL_MEM_READ_ONLY,
                                   buffer_size * sizeof(int), NULL, &ret);
    if (ret == 0)
         output_mem_obj = clCreateBuffer(gpu->context, CL_MEM_WRITE_ONLY,
                                         buffer_size * sizeof(float), NULL, &ret);

    //copy input into the memory object
    if (ret == 0)
        ret = clEnqueueWriteBuffer(gpu->commandQueue, input_mem_obj, CL_TRUE, 0, buffer_size * sizeof (int), input, 0, NULL, NULL);

    //create kernel
    if (ret == 0)
        kernel = clCreateKernel(gpu->programs[0].program, "rgb_to_XYZ", &ret);

    //set kernel arguments
    if (ret == 0)
        ret = clSetKernelArg(kernel, 0, sizeof(cl_mem), (void*)&input_mem_obj);
    if (ret == 0)
        ret = clSetKernelArg(kernel, 1, sizeof(cl_mem), (void*)&output_mem_obj);
    if (ret == 0)
        ret = clSetKernelArg(kernel, 2, sizeof(const unsigned char), (void *)&channels);

    size_t global_item_size = x * y;
    size_t local_item_size = y;

    //request the gpu process
    if (ret == 0)
        ret = clEnqueueNDRangeKernel(gpu->commandQueue, kernel, 1, NULL, &global_item_size, &local_item_size, 0, NULL, NULL);

    //read the results
    if (ret == 0)
        ret = clEnqueueReadBuffer(gpu->commandQueue, output_mem_obj, CL_TRUE, 0, buffer_size * sizeof (float), output, 0 ,NULL, NULL);

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

int gpu_xyz_to_lab(gpu_t *gpu, float *input, int*output, unsigned int x, unsigned int y, unsigned char channels){
    unsigned long buffer_size = x * y * channels;
    cl_int ret = 0;
    cl_mem input_mem_obj = NULL;
    cl_mem output_mem_obj = NULL;
    cl_kernel kernel = NULL;

    //create memory objects

    input_mem_obj = clCreateBuffer(gpu->context, CL_MEM_READ_ONLY,
                                   buffer_size * sizeof(float), NULL, &ret);
    if (ret == 0)
         output_mem_obj = clCreateBuffer(gpu->context, CL_MEM_WRITE_ONLY,
                                         buffer_size * sizeof(int), NULL, &ret);

    //copy input into the memory object
    if (ret == 0)
        ret = clEnqueueWriteBuffer(gpu->commandQueue, input_mem_obj, CL_TRUE, 0, buffer_size * sizeof (float), input, 0, NULL, NULL);

    //create kernel
    if (ret == 0)
        kernel = clCreateKernel(gpu->programs[0].program, "xyz_to_lab", &ret);

    //set kernel arguments
    if (ret == 0)
        ret = clSetKernelArg(kernel, 0, sizeof(cl_mem), (void*)&input_mem_obj);
    if (ret == 0)
        ret = clSetKernelArg(kernel, 1, sizeof(cl_mem), (void*)&output_mem_obj);
    if (ret == 0)
        ret = clSetKernelArg(kernel, 2, sizeof(const unsigned char), (void *)&channels);

    size_t global_item_size = x * y;
    size_t local_item_size = y;

    //request the gpu process
    if (ret == 0)
        ret = clEnqueueNDRangeKernel(gpu->commandQueue, kernel, 1, NULL, &global_item_size, &local_item_size, 0, NULL, NULL);

    //read the results
    if (ret == 0)
        ret = clEnqueueReadBuffer(gpu->commandQueue, output_mem_obj, CL_TRUE, 0, buffer_size * sizeof (int), output, 0 ,NULL, NULL);

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

int gpu_lab_to_lhc(gpu_t *gpu, int *input, int*output, unsigned int x, unsigned int y, unsigned char channels){
    unsigned long buffer_size = x * y * channels;
    cl_int ret = 0;
    cl_mem input_mem_obj = NULL;
    cl_mem output_mem_obj = NULL;
    cl_kernel kernel = NULL;

    //create memory objects

    input_mem_obj = clCreateBuffer(gpu->context, CL_MEM_READ_ONLY,
                                   buffer_size * sizeof(int), NULL, &ret);
    if (ret == 0)
        output_mem_obj = clCreateBuffer(gpu->context, CL_MEM_WRITE_ONLY,
                                        buffer_size * sizeof(int), NULL, &ret);

    //copy input into the memory object
    if (ret == 0)
        ret = clEnqueueWriteBuffer(gpu->commandQueue, input_mem_obj, CL_TRUE, 0, buffer_size * sizeof (int), input, 0, NULL, NULL);

    //create kernel
    if (ret == 0)
        kernel = clCreateKernel(gpu->programs[0].program, "lab_to_lhc", &ret);

    //set kernel arguments
    if (ret == 0)
        ret = clSetKernelArg(kernel, 0, sizeof(cl_mem), (void*)&input_mem_obj);
    if (ret == 0)
        ret = clSetKernelArg(kernel, 1, sizeof(cl_mem), (void*)&output_mem_obj);
    if (ret == 0)
        ret = clSetKernelArg(kernel, 2, sizeof(const unsigned char), (void *)&channels);

    size_t global_item_size = x * y;
    size_t local_item_size = y;

    //request the gpu process
    if (ret == 0)
        ret = clEnqueueNDRangeKernel(gpu->commandQueue, kernel, 1, NULL, &global_item_size, &local_item_size, 0, NULL, NULL);

    //read the results
    if (ret == 0)
        ret = clEnqueueReadBuffer(gpu->commandQueue, output_mem_obj, CL_TRUE, 0, buffer_size * sizeof (int), output, 0 ,NULL, NULL);

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