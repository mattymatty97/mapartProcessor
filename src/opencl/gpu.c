#include <stdio.h>
#include <sys/stat.h>
#include "gpu.h"

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

cl_program gpu_compile_program(main_options *config, gpu_t * gpu_holder, char * filename, cl_int * ret);

int gpu_get_program_source_from_mongo(main_options *config, char * program_name, size_t * source_size, char **program_source);
int gpu_write_program_source_to_mongo(main_options *config, char * program_name, char * program_source);

int gpu_init(main_options *config, gpu_t* gpu_holder){
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

    ret = clGetDeviceInfo(gpu_holder->deviceId, CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof (size_t), &gpu_holder->max_parallelism, NULL);

    cl_context context;
    if (ret == 0)
        context = gpu_holder->context = clCreateContext(NULL, 1, &device_id[platform_index][device_index], NULL, NULL, &ret);

    //if all is ok
    if (ret == 0){
        gpu_holder->commandQueue = clCreateCommandQueueWithProperties(context, device_id[platform_index][device_index], NULL, &ret);
    }

    //compile programs
    if (ret == 0){

        gpu_program program = {
                "color_conversion",
                gpu_compile_program(config, gpu_holder,"resources/opencl/color/color_conversions.cl", &ret)
        };

        gpu_holder->programs[0] = program;

    }

    if (ret == 0){

        gpu_program program = {
                "no_dithering",
                gpu_compile_program(config, gpu_holder,"resources/opencl/dithering/none.cl", &ret)
        };

        gpu_holder->programs[1] = program;

    }

    if (ret == 0){

        gpu_program program = {
                "floyd_dithering",
                gpu_compile_program(config, gpu_holder,"resources/opencl/dithering/floyd.cl", &ret)
        };

        gpu_holder->programs[2] = program;

    }

    if (ret == 0){

        gpu_program program = {
                "JJND_dithering",
                gpu_compile_program(config, gpu_holder,"resources/opencl/dithering/JJND.cl", &ret)
        };

        gpu_holder->programs[3] = program;

    }

    if (ret == 0){

        gpu_program program = {
                "stucki_dithering",
                gpu_compile_program(config, gpu_holder,"resources/opencl/dithering/stucki.cl", &ret)
        };

        gpu_holder->programs[4] = program;

    }

    if (ret == 0){

        gpu_program program = {
                "atkinson_dithering",
                gpu_compile_program(config, gpu_holder,"resources/opencl/dithering/atkinson.cl", &ret)
        };

        gpu_holder->programs[5] = program;

    }

    if (ret == 0){

        gpu_program program = {
                "burkes_dithering",
                gpu_compile_program(config, gpu_holder,"resources/opencl/dithering/burkes.cl", &ret)
        };

        gpu_holder->programs[6] = program;

    }

    if (ret == 0){

        gpu_program program = {
                "sierra_dithering",
                gpu_compile_program(config, gpu_holder,"resources/opencl/dithering/sierra.cl", &ret)
        };

        gpu_holder->programs[7] = program;

    }

    if (ret == 0){

        gpu_program program = {
                "sierra2_dithering",
                gpu_compile_program(config, gpu_holder,"resources/opencl/dithering/sierra2.cl", &ret)
        };

        gpu_holder->programs[8] = program;

    }

    if (ret == 0){

        gpu_program program = {
                "sierraL_dithering",
                gpu_compile_program(config, gpu_holder,"resources/opencl/dithering/sierraL.cl", &ret)
        };

        gpu_holder->programs[9] = program;

    }

    if (ret == 0){

        gpu_program program = {
                "mapart",
                gpu_compile_program(config, gpu_holder,"resources/opencl/mapart/mapart.cl", &ret)
        };

        gpu_holder->programs[10] = program;

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

int gpu_get_program_source_from_mongo(main_options *config, char * program_name, size_t * source_size, char **program_source){
    int ret = 0;
    mongoc_client_t *client;
    mongoc_collection_t *collection;
    mongoc_cursor_t *cursor;
    const bson_t *doc;
    bson_t *query;
    char* str;

    printf("Loading OpenCL program %s\n", program_name);

    client = mongoc_client_pool_pop(config->mongo_session.pool);

    collection = mongoc_client_get_collection(client, config->mongodb_database, "config");

    query = bson_new();

    BSON_APPEND_UTF8(query, "type", "opencl_src");
    BSON_APPEND_UTF8(query, "name", program_name);

    cursor = mongoc_collection_find_with_opts(collection, query, NULL, NULL);

    if (mongoc_cursor_next (cursor, &doc)) {
        printf("Program found, reading...\n");
        bson_iter_t iter;
        bson_iter_t colors_iter;
        bson_iter_t multiplier_iter;
        if(bson_iter_init_find(&iter,doc,"source") &&
           BSON_ITER_HOLDS_CODE(&iter)){
            char i = 0;
            *program_source = strdup((char *)bson_iter_code(&iter, (uint32_t *) &i));
            *source_size = strlen(*program_source);
        }
        printf("Program read\n\n");
    }else{
        fprintf (stderr,"Program %s not Found!\n", program_name);
        ret = 41;
    }

    bson_destroy (query);
    mongoc_cursor_destroy (cursor);
    mongoc_collection_destroy (collection);

    mongoc_client_pool_push(config->mongo_session.pool, client);

    return ret;
}

cl_int gpu_write_program_source_to_mongo(main_options *config, char * program_name, char * program_source){
    int ret = 0;

    mongoc_client_t *client;
    mongoc_collection_t *config_c;
    mongoc_collection_t *pixels_c;
    mongoc_cursor_t *cursor;
    const bson_t *doc;
    bson_t *query;
    bson_error_t error;

    printf("Uploading OpenCL program to MongoDB %s\n", program_name);

    client = mongoc_client_pool_pop(config->mongo_session.pool);

    config_c = mongoc_client_get_collection(client, config->mongodb_database, "config");

    query = bson_new();

    BSON_APPEND_UTF8(query, "type", "opencl_src");
    BSON_APPEND_UTF8(query, "name", program_name);


    //if everything is fine continue
    if (ret == 0) {
        //set flag entry in config
        //or reset if already exists

        //set as upsert
        bson_t *param = BCON_NEW(
                "upsert", BCON_BOOL(true)
        );

        //prepare upsert command
        bson_t *upsert = BCON_NEW(
                "$set", "{",
                "source", BCON_CODE(program_source),
                "}"
        );

        if (!mongoc_collection_update_one(config_c, query, upsert, param, NULL, &error)) {
            //TODO add error handling;
            ret = 19;
        }

        bson_destroy(param);
        bson_destroy(upsert);
        bson_destroy(query);
    }
    mongoc_collection_destroy(config_c);

    mongoc_client_pool_push(config->mongo_session.pool, client);

    return ret;
}

cl_program gpu_compile_program(main_options *config, gpu_t * gpu_holder, char * filename, cl_int * ret) {
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

    if (*ret == 5){
        *ret = gpu_get_program_source_from_mongo(config, filename, &length, &source_str);
    }else{
        *ret = gpu_write_program_source_to_mongo(config, filename, source_str);
    }

    if (*ret == 0) {
        cl_program program = clCreateProgramWithSource(gpu_holder->context, 1, (const char **)&source_str, &length,
                                                       ret);
        if (*ret == 0) {
            *ret = clBuildProgram(program, 1, &gpu_holder->deviceId, NULL, NULL, NULL);
        }

        if (*ret == 0) {
            free(source_str);
            return program;
        }else{
            char build_log[5000] = {};
            size_t log_size = 0;
            clGetProgramBuildInfo(program, gpu_holder->deviceId, CL_PROGRAM_BUILD_LOG, sizeof (char) * 5000, build_log, &log_size);
            fprintf(stderr, "Error compiling %s:\n%s\n\n", filename, build_log);
        }
    }
    free(source_str);
    return NULL;
}

int gpu_rgb_to_xyz(gpu_t *gpu, int *input, float*output, unsigned int x, unsigned int y, unsigned char channels){
    unsigned long buffer_size = x * y * channels;
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
    size_t local_item_size = MIN(y, gpu->max_parallelism);

    //request the gpu process
    if (ret == 0)
        ret = clEnqueueNDRangeKernel(gpu->commandQueue, kernel, 1, NULL, &global_item_size, &local_item_size, 0, NULL, &event);

    //read the results
    if (ret == 0)
        ret = clEnqueueReadBuffer(gpu->commandQueue, output_mem_obj, CL_TRUE, 0, buffer_size * sizeof (float), output, 1 ,&event, &event);

    //wait for results
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

int gpu_xyz_to_lab(gpu_t *gpu, float *input, int*output, unsigned int x, unsigned int y, unsigned char channels){
    unsigned long buffer_size = x * y * channels;
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
    size_t local_item_size = MIN(y, gpu->max_parallelism);

    //request the gpu process
    if (ret == 0)
        ret = clEnqueueNDRangeKernel(gpu->commandQueue, kernel, 1, NULL, &global_item_size, &local_item_size, 0, NULL, &event);

    //read the results
    if (ret == 0)
        ret = clEnqueueReadBuffer(gpu->commandQueue, output_mem_obj, CL_TRUE, 0, buffer_size * sizeof (int), output, 1 ,&event, &event);

    //wait for results
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

int gpu_lab_to_lch(gpu_t *gpu, int *input, int*output, unsigned int x, unsigned int y, unsigned char channels){
    unsigned long buffer_size = x * y * channels;
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
                                        buffer_size * sizeof(int), NULL, &ret);

    //copy input into the memory object
    if (ret == 0)
        ret = clEnqueueWriteBuffer(gpu->commandQueue, input_mem_obj, CL_TRUE, 0, buffer_size * sizeof (int), input, 0, NULL, NULL);

    //create kernel
    if (ret == 0)
        kernel = clCreateKernel(gpu->programs[0].program, "lab_to_lch", &ret);

    //set kernel arguments
    if (ret == 0)
        ret = clSetKernelArg(kernel, 0, sizeof(cl_mem), (void*)&input_mem_obj);
    if (ret == 0)
        ret = clSetKernelArg(kernel, 1, sizeof(cl_mem), (void*)&output_mem_obj);
    if (ret == 0)
        ret = clSetKernelArg(kernel, 2, sizeof(const unsigned char), (void *)&channels);

    size_t global_item_size = x * y;
    size_t local_item_size = MIN(y, gpu->max_parallelism);

    //request the gpu process
    if (ret == 0)
        ret = clEnqueueNDRangeKernel(gpu->commandQueue, kernel, 1, NULL, &global_item_size, &local_item_size, 0, NULL, &event);

    //read the results
    if (ret == 0)
        ret = clEnqueueReadBuffer(gpu->commandQueue, output_mem_obj, CL_TRUE, 0, buffer_size * sizeof (int), output, 1 ,&event, &event);

    //wait for results
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

int gpu_dither_none(gpu_t *gpu, int *input, int*palette, unsigned char* result, unsigned int x, unsigned int y, unsigned char channels, unsigned char palette_indexes, unsigned char palette_variations){
    unsigned long buffer_size = x * y * channels;
    unsigned long palette_size = palette_indexes * palette_variations * 4;
    unsigned long result_size = x * y * 2;
    cl_int ret = 0;
    cl_mem input_mem_obj = NULL;
    cl_mem palette_mem_obj = NULL;
    cl_mem output_mem_obj = NULL;
    cl_kernel kernel = NULL;

    cl_event event;

    //create memory objects

    input_mem_obj = clCreateBuffer(gpu->context, CL_MEM_READ_ONLY,
                                   buffer_size * sizeof(int), NULL, &ret);
    if (ret == 0)
        palette_mem_obj = clCreateBuffer(gpu->context, CL_MEM_READ_ONLY,
                                         palette_size * sizeof(int), NULL, &ret);
    if (ret == 0)
        output_mem_obj = clCreateBuffer(gpu->context, CL_MEM_WRITE_ONLY,
                                        result_size * sizeof(unsigned char), NULL, &ret);

    //copy input into the memory object
    if (ret == 0)
        ret = clEnqueueWriteBuffer(gpu->commandQueue, input_mem_obj, CL_TRUE, 0, buffer_size * sizeof (int), input, 0, NULL, NULL);
    if (ret == 0)
        ret = clEnqueueWriteBuffer(gpu->commandQueue, palette_mem_obj, CL_TRUE, 0, palette_size * sizeof (int), palette, 0, NULL, NULL);

    //create kernel
    if (ret == 0)
        kernel = clCreateKernel(gpu->programs[1].program, "no_dithering", &ret);

    //set kernel arguments
    if (ret == 0)
        ret = clSetKernelArg(kernel, 0, sizeof(cl_mem), (void*)&input_mem_obj);
    if (ret == 0)
        ret = clSetKernelArg(kernel, 1, sizeof(cl_mem), (void*)&palette_mem_obj);
    if (ret == 0)
        ret = clSetKernelArg(kernel, 2, sizeof(cl_mem), (void*)&output_mem_obj);
    if (ret == 0)
        ret = clSetKernelArg(kernel, 3, sizeof(const unsigned char), (void *)&channels);
    if (ret == 0)
        ret = clSetKernelArg(kernel, 4, sizeof(const unsigned char), (void *)&palette_indexes);
    if (ret == 0)
        ret = clSetKernelArg(kernel, 5, sizeof(const unsigned char), (void *)&palette_variations);

    size_t global_item_size = x * y;
    size_t local_item_size = MIN(y, gpu->max_parallelism);

    //request the gpu process
    if (ret == 0)
        ret = clEnqueueNDRangeKernel(gpu->commandQueue, kernel, 1, NULL, &global_item_size, &local_item_size, 0, NULL, &event);

    //read the results
    if (ret == 0)
        ret = clEnqueueReadBuffer(gpu->commandQueue, output_mem_obj, CL_TRUE, 0, result_size * sizeof (unsigned char), result, 1 ,&event, &event);

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
    if (output_mem_obj != NULL)
        clReleaseMemObject(output_mem_obj);

    return ret;
}

int _gpu_dither_error_bleed(gpu_t *gpu, cl_program program, char * kernel_func, int *input, int*palette, unsigned char* result, unsigned int x, unsigned int y, unsigned char channels, unsigned char palette_indexes, unsigned char palette_variations){
    unsigned long buffer_size = x * y * channels;
    unsigned long palette_size = palette_indexes * palette_variations * 4;
    unsigned long result_size = x * y * 2;
    size_t global_workgroup_size = y;
    size_t local_workgroup_size = MIN(y, gpu->max_parallelism);
    while (global_workgroup_size % local_workgroup_size != 0){local_workgroup_size--;}

    cl_event event;

    cl_int ret = 0;
    cl_mem input_mem_obj = NULL;
    cl_mem palette_mem_obj = NULL;
    cl_mem error_buf_mem_obj = NULL;
    cl_mem workgroup_rider_mem_obj = NULL;
    cl_mem workgroup_progress_mem_obj = NULL;
    cl_mem output_mem_obj = NULL;
    cl_kernel kernel = NULL;

    //create memory objects

    input_mem_obj = clCreateBuffer(gpu->context, CL_MEM_READ_ONLY,
                                   buffer_size * sizeof(int), NULL, &ret);
    if (ret == 0)
        palette_mem_obj = clCreateBuffer(gpu->context, CL_MEM_READ_ONLY,
                                         palette_size * sizeof(int), NULL, &ret);
    if (ret == 0)
        output_mem_obj = clCreateBuffer(gpu->context, CL_MEM_WRITE_ONLY,
                                        result_size * sizeof(unsigned char), NULL, &ret);
    if (ret == 0)
        error_buf_mem_obj = clCreateBuffer(gpu->context, CL_MEM_READ_WRITE,
                                           buffer_size * sizeof(int), NULL, &ret);
    if (ret == 0)
        workgroup_rider_mem_obj = clCreateBuffer(gpu->context, CL_MEM_READ_WRITE,
                                                 sizeof(unsigned int), NULL, &ret);
    if (ret == 0)
        workgroup_progress_mem_obj = clCreateBuffer(gpu->context, CL_MEM_READ_WRITE,
                                                    y * sizeof(unsigned int), NULL, &ret);

    //copy input into the memory object
    if (ret == 0)
        ret = clEnqueueWriteBuffer(gpu->commandQueue, input_mem_obj, CL_TRUE, 0, buffer_size * sizeof (int), input, 0, NULL, &event);
    if (ret == 0)
        ret = clEnqueueWriteBuffer(gpu->commandQueue, palette_mem_obj, CL_TRUE, 0, palette_size * sizeof (int), palette, 0, NULL, &event);
    int pattern = 0;

    if (ret == 0)
        ret = clEnqueueFillBuffer(gpu->commandQueue, error_buf_mem_obj, &pattern, sizeof (int), 0, palette_size * sizeof (int), 1, &event, &event);

    //create kernel
    if (ret == 0)
        kernel = clCreateKernel(program, kernel_func, &ret);

    //set kernel arguments
    if (ret == 0)
        ret = clSetKernelArg(kernel, 0, sizeof(cl_mem), (void*)&input_mem_obj);
    if (ret == 0)
        ret = clSetKernelArg(kernel, 1, sizeof(cl_mem), (void*)&output_mem_obj);
    if (ret == 0)
        ret = clSetKernelArg(kernel, 2, sizeof(cl_mem), (void*)&error_buf_mem_obj);
    if (ret == 0)
        ret = clSetKernelArg(kernel, 3, sizeof(cl_mem), (void*)&palette_mem_obj);
    if (ret == 0)
        ret = clSetKernelArg(kernel, 4, sizeof(cl_mem), (void*)&workgroup_rider_mem_obj);
    if (ret == 0)
        ret = clSetKernelArg(kernel, 5, sizeof(cl_mem), (void*)&workgroup_progress_mem_obj);
    if (ret == 0)
        ret = clSetKernelArg(kernel, 6, sizeof(cl_int) * y, NULL);
    if (ret == 0)
        ret = clSetKernelArg(kernel, 7, sizeof(const unsigned char), (void *)&channels);
    if (ret == 0)
        ret = clSetKernelArg(kernel, 8, sizeof(const unsigned int), (void *)&x);
    if (ret == 0)
        ret = clSetKernelArg(kernel, 9, sizeof(const unsigned int), (void *)&y);
    if (ret == 0)
        ret = clSetKernelArg(kernel, 10, sizeof(const unsigned char), (void *)&palette_indexes);
    if (ret == 0)
        ret = clSetKernelArg(kernel, 11, sizeof(const unsigned char), (void *)&palette_variations);


    //request the gpu process
    if (ret == 0)
        ret = clEnqueueNDRangeKernel(gpu->commandQueue, kernel, 1, NULL, &global_workgroup_size, &local_workgroup_size, 1, &event, &event);

    //read the results
    if (ret == 0)
        ret = clEnqueueReadBuffer(gpu->commandQueue, output_mem_obj, CL_TRUE, 0, result_size * sizeof (unsigned char), result, 1 ,&event, &event);

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
    if (output_mem_obj != NULL)
        clReleaseMemObject(output_mem_obj);
    if (error_buf_mem_obj != NULL)
        clReleaseMemObject(error_buf_mem_obj);
    if (workgroup_rider_mem_obj != NULL)
        clReleaseMemObject(workgroup_rider_mem_obj);
    if (workgroup_rider_mem_obj != NULL)
        clReleaseMemObject(workgroup_progress_mem_obj);

    return ret;
}

int gpu_dither_floyd_steinberg(gpu_t *gpu, int *input, int*palette, unsigned char* result, unsigned int x, unsigned int y, unsigned char channels, unsigned char palette_indexes, unsigned char palette_variations){
    return _gpu_dither_error_bleed(gpu, gpu->programs[2].program, "Floyd_Steinberg", input, palette, result, x, y, channels, palette_indexes, palette_variations);
}

int gpu_dither_JJND(gpu_t *gpu, int *input, int*palette, unsigned char* result, unsigned int x, unsigned int y, unsigned char channels, unsigned char palette_indexes, unsigned char palette_variations){
    return _gpu_dither_error_bleed(gpu, gpu->programs[3].program, "JJND", input, palette, result, x, y, channels, palette_indexes, palette_variations);
}

int gpu_dither_Stucki(gpu_t *gpu, int *input, int*palette, unsigned char* result, unsigned int x, unsigned int y, unsigned char channels, unsigned char palette_indexes, unsigned char palette_variations){
    return _gpu_dither_error_bleed(gpu, gpu->programs[4].program, "Stucki", input, palette, result, x, y, channels, palette_indexes, palette_variations);
}

int gpu_dither_Atkinson(gpu_t *gpu, int *input, int*palette, unsigned char* result, unsigned int x, unsigned int y, unsigned char channels, unsigned char palette_indexes, unsigned char palette_variations){
    return _gpu_dither_error_bleed(gpu, gpu->programs[5].program, "Atkinson", input, palette, result, x, y, channels, palette_indexes, palette_variations);
}

int gpu_dither_Burkes(gpu_t *gpu, int *input, int*palette, unsigned char* result, unsigned int x, unsigned int y, unsigned char channels, unsigned char palette_indexes, unsigned char palette_variations){
    return _gpu_dither_error_bleed(gpu, gpu->programs[6].program, "Burkes", input, palette, result, x, y, channels, palette_indexes, palette_variations);
}

int gpu_dither_Sierra(gpu_t *gpu, int *input, int*palette, unsigned char* result, unsigned int x, unsigned int y, unsigned char channels, unsigned char palette_indexes, unsigned char palette_variations){
    return _gpu_dither_error_bleed(gpu, gpu->programs[7].program, "Sierra", input, palette, result, x, y, channels, palette_indexes, palette_variations);
}

int gpu_dither_Sierra2(gpu_t *gpu, int *input, int*palette, unsigned char* result, unsigned int x, unsigned int y, unsigned char channels, unsigned char palette_indexes, unsigned char palette_variations){
    return _gpu_dither_error_bleed(gpu, gpu->programs[8].program, "SierraTwo", input, palette, result, x, y, channels, palette_indexes, palette_variations);
}

int gpu_dither_SierraL(gpu_t *gpu, int *input, int*palette, unsigned char* result, unsigned int x, unsigned int y, unsigned char channels, unsigned char palette_indexes, unsigned char palette_variations){
    return _gpu_dither_error_bleed(gpu, gpu->programs[9].program, "SierraL", input, palette, result, x, y, channels, palette_indexes, palette_variations);
}

int gpu_palette_to_rgb(gpu_t *gpu, unsigned char *input, int*palette, unsigned char* result, unsigned int x, unsigned int y, unsigned char palette_indexes, unsigned char palette_variations){
    unsigned long buffer_size = x * y * 2;
    unsigned long palette_size = palette_indexes * palette_variations * 4;
    unsigned long result_size = x * y * 4;
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
                                        result_size * sizeof(unsigned char), NULL, &ret);

    //copy input into the memory object
    if (ret == 0)
        ret = clEnqueueWriteBuffer(gpu->commandQueue, input_mem_obj, CL_TRUE, 0, buffer_size * sizeof (unsigned char), input, 0, NULL, NULL);
    if (ret == 0)
        ret = clEnqueueWriteBuffer(gpu->commandQueue, palette_mem_obj, CL_TRUE, 0, palette_size * sizeof (int), palette, 0, NULL, NULL);

    //create kernel
    if (ret == 0)
        kernel = clCreateKernel(gpu->programs[10].program, "palette_to_rgb", &ret);

    //set kernel arguments
    if (ret == 0)
        ret = clSetKernelArg(kernel, 0, sizeof(cl_mem), (void*)&input_mem_obj);
    if (ret == 0)
        ret = clSetKernelArg(kernel, 1, sizeof(cl_mem), (void*)&palette_mem_obj);
    if (ret == 0)
        ret = clSetKernelArg(kernel, 2, sizeof(cl_mem), (void*)&output_mem_obj);
    if (ret == 0)
        ret = clSetKernelArg(kernel, 3, sizeof(const unsigned char), (void *)&palette_indexes);
    if (ret == 0)
        ret = clSetKernelArg(kernel, 4, sizeof(const unsigned char), (void *)&palette_variations);

    size_t global_item_size = x * y;
    size_t local_item_size = MIN(y, gpu->max_parallelism);

    //request the gpu process
    if (ret == 0)
        ret = clEnqueueNDRangeKernel(gpu->commandQueue, kernel, 1, NULL, &global_item_size, &local_item_size, 0, NULL, NULL);

    //read the results
    if (ret == 0)
        ret = clEnqueueReadBuffer(gpu->commandQueue, output_mem_obj, CL_TRUE, 0, result_size * sizeof (unsigned char), result, 0 ,NULL, NULL);

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

