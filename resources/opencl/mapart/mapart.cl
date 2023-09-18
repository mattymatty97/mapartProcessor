__kernel void palette_to_rgb(__global const unsigned char  *In, __global const int *palette, __global char *Out,
 const unsigned char palette_indexes, const unsigned char palette_variations) {
    // Get the index of the current element to be processed
    int i = get_global_id(0);

    unsigned char *curr = &In[(i * 2)];
    
    unsigned char index = curr[0];
    unsigned char state = curr[1];

    Out[(i * 4)] = palette[(index * palette_variations * 4) + (state * 4)];
    Out[(i * 4) + 1] = palette[(index * palette_variations * 4) + (state * 4) + 1];
    Out[(i * 4) + 2] = palette[(index * palette_variations * 4) + (state * 4) + 2];
    Out[(i * 4) + 3] = palette[(index * palette_variations * 4) + (state * 4) + 3];
}


#define SIGN(x) (x > 0) - (x < 0)


//kernel

__kernel void palette_to_height(
                  __global uchar         *src,      
                  __global uint          *dst,
                  __global uint          *workgroup_rider,
                  __global uchar          *error,
                  const uint              width,
                  const uint              height,
                  const int               max_mc_height)
{
    __local volatile uint        workgroup_number;

    if (get_local_id(0) == 0)
    {
        workgroup_number        = atomic_inc(workgroup_rider);
    }


    barrier(CLK_LOCAL_MEM_FENCE);


    uint x = (workgroup_number * get_local_size(0)) + get_local_id(0);

    uchar2 first_pixel = vload2(0, src);

    uint mc_height = (first_pixel[2] == 0)?1:0;

    char direction = -1;

    uint start_index = 0;

    for (uint y = 0; y < (height) && atomic_add(error, 0) == 0; y++)
    {
        uint i = (width * y) + x;

        uchar2 og_pixel = vload2(i, src);

        char delta = og_pixel[1] - 1;
        if (SIGN(direction) == -SIGN(delta)){
            //staircase is changing direction
            start_index = y - 1;
            direction = SIGN(delta);
        }

        long tmp_mc_height = mc_height + delta;

        if (tmp_mc_height < 0 || tmp_mc_height > max_mc_height){
            printf("Pixel (%d,%d) shifted %d blocks %s", x, y, (y - start_index), (delta < 0)?"Up":"Down");
            for (uint tmp_y = start_index; tmp_y < y && atomic_add(error, 0) == 0; tmp_y++){
                uint2 old_res = vload2(i ,dst);
                long tmp = old_res[2] - delta;
                if (tmp >= 0 && tmp < max_mc_height){
                    uint2 new_res = (old_res[0], tmp);
                    vstore2(new_res, i, dst);
                }else{
                    printf("Error: Pixel (%d,%d) crashed a staircase y:%d", x, y, mc_height);
                    atomic_add(error,1);
                }
            }
            mc_height -= delta;
            tmp_mc_height -= delta;
        }

        uint2 ret_pixel = (og_pixel[0], tmp_mc_height);
        vstore2(ret_pixel, i ,dst);
    }
}
