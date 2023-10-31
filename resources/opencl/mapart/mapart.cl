__kernel void palette_to_rgb(
                __global const unsigned char  *In, 
                __global const int *palette, 
                __global char *Out,
                const unsigned char palette_indexes, 
                const unsigned char palette_variations) {
    // Get the index of the current element to be processed
    int i = get_global_id(0);
    
    unsigned char index = (&In[(i * 2)])[0];
    unsigned char state = (&In[(i * 2)])[1];

    Out[(i * 4)] = palette[(index * palette_variations * 4) + (state * 4)];
    Out[(i * 4) + 1] = palette[(index * palette_variations * 4) + (state * 4) + 1];
    Out[(i * 4) + 2] = palette[(index * palette_variations * 4) + (state * 4) + 2];
    Out[(i * 4) + 3] = palette[(index * palette_variations * 4) + (state * 4) + 3];
}


#define SIGN(x) ((x > 0) - (x < 0))

//constants
#define LIQUID_DEPTH (int3)(10,5,1)

//kernel

__kernel void palette_to_height(
                __global uchar         *src,
                __global uchar         *liquid_palette_ids,
                __global uint          *dst,
                __global uint          *error,
                __global volatile int  *mc_height,
                __global volatile uint *start_index,
                __global volatile int *start_padding,
                const uint              width,
                const uint              height,
                const int               max_mc_height,
                __global uint          *computed_max)
{

    if (*error){
        return;
    }

    uint index        = get_global_id(0);

    uint x            = index % width;

    uint y            = index / height;

    uint o            = (width * (y + 1)) + x;

    uchar2 og_pixel   = vload2(index, src);

    uchar block_id    = og_pixel[0];

    uchar block_state = og_pixel[1];

    if (x == 0){
        mc_height[x]     = 0;
        start_index[x]   = 0;
        start_padding[x] = -1;
    }

    if (y == 0){
        if (block_id == 0 || liquid_palette_ids[block_id] || block_state == 1){
            vstore3((uint3){UCHAR_MAX,0,0}, x ,dst);
        }else if (block_state == 0){
            vstore3((uint3){UCHAR_MAX,1,1}, x ,dst);
        }else if (block_state == 2){
            vstore3((uint3){UCHAR_MAX,0,0}, x ,dst);
            mc_height[x] = 1;
        }
    }

    char delta        = block_state - 1;

    long bottom_block = mc_height[x] + delta;
    long top_block    = mc_height[x] + delta;

    if (block_id == 0){
        //if is transparent
        bottom_block     = 0;
        top_block        = 0;
        start_index[x]   = y;
        start_padding[x] = 0;
    }else if (liquid_palette_ids[block_id]){
        //if it is liquid
        bottom_block     = 0;
        top_block        = LIQUID_DEPTH[block_state];
        start_index[x]   = y;
        start_padding[x] = 0;
    }else{

        if (SIGN(mc_height) == -SIGN(delta)){
            //if we're changing direction ( was staircase up and now is down )
            //drop down to y0
            bottom_block     = 0;
            top_block        = 0;
            //set this as start of the downards staircase
            start_index[x]   = y;
            //tell that it is possible to raise the block bofore if we reach it's height
            start_padding[x] = -1;
        }

        if (bottom_block < 0 || (max_mc_height > 0 && top_block > max_mc_height)){
            //if the block would be out of the world
            //check if we are touching another block
            uint tmp_y = start_index[x];
            uint tmp_o = (width * ( tmp_y + 1 + start_padding[x])) + x;
            uint3 o_pixel = vload3(tmp_o, dst);

            //if we are shift the loop to include it too
            if (o_pixel[2] >= top_block - 1)
                tmp_y += start_index[x];

            //loop over the entire staircase
            for (;tmp_y < y && !(*error) ; tmp_y++){
                tmp_o = (width * (tmp_y + 1)) + x;
                o_pixel = vload3(tmp_o, dst);

                //shift the staircase to accomodate the new block
                o_pixel[1] -= delta;
                o_pixel[2] -= delta;

                //check if we are pushing the staircase out of the world
                if (o_pixel[1] < 0 || (max_mc_height > 0 && o_pixel[2] > max_mc_height)){
                    printf("Error: Pixel (%d,%d) crashed a staircase y:%d-%d\n", x, y, o_pixel[1], o_pixel[2]);
                    atomic_add(error, 1);
                }else{
                    //save the new positions
                    vstore3(o_pixel, tmp_o, dst);
                }
            }

            //we have moved by
            bottom_block -= delta;
            top_block    -= delta;

        }
    }
    mc_height = top_block;
    atomic_max(computed_max, mc_height[x]);
    uint3 ret_pixel = {block_id, bottom_block, top_block};
    vstore3(ret_pixel, o ,dst);
}