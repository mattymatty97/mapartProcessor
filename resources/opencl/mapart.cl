

#define SIGN(x) ((x > 0) - (x < 0))

//constants
#define LIQUID_DEPTH (int3)(10,5,1)

//kernels

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


__kernel void palette_to_height(
                __global uchar         *src,
                __global uchar         *liquid_palette_ids,
                __global uint          *dst,
                __global uint          *error,
                __global volatile int  *mc_height,
                __global volatile uint *start_index,
                __global volatile int  *start_padding,
                __global volatile uint *flat_count,
                const uint              width,
                const uint              height,
                const int               max_mc_height,
                __global uint          *computed_max)
{

    size_t index        = get_global_id(0);

    uint x              = index % width;

    uint y              = index / width;

    size_t o            = (width * (y + 1)) + x;

    //printf("Pixel %d (%d ,%d) 1\n", (uint)index, (uint)x, (unsigned int)y);

    if (atomic_and(error, 1) == 0){

        uchar2 og_pixel   = vload2(index, src);

        uchar block_id    = og_pixel[0];

        uchar block_state = og_pixel[1];

        if (y == 0){
            mc_height[x]     = 0;
            start_index[x]   = 0;
            start_padding[x] = -1;
            flat_count[x]    = 0;
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
        
        //printf("Pixel %d (%d ,%d) 2\n", (uint)index, (uint)x, (unsigned int)y);

        char delta        = block_state - 1;

        int bottom_block = mc_height[x] + delta;
        int top_block    = mc_height[x] + delta;

        if (block_id == 0){
            //if is transparent
            bottom_block     = 0;
            top_block        = 0;
            start_index[x]   = y + 1;
            start_padding[x] = 0;
        }else if (liquid_palette_ids[block_id]){
            //if it is liquid
            bottom_block     = 0;
            top_block        = LIQUID_DEPTH[block_state];
            start_index[x]   = y;
            start_padding[x] = 0;
        }else{

            //printf("Pixel %d (%d ,%d) 3\n", (uint)index, (uint)x, (unsigned int)y);
            if (SIGN(mc_height[x]) == -SIGN(delta)){
                //if we're changing direction ( was staircase up and now is down )
                //drop down to y0
                bottom_block     = 0;
                top_block        = 0;
                //set this as start of the downards staircase
                start_index[x]   = y;
                //tell that it is possible to raise the block before if we reach it's height
                start_padding[x] = -flat_count[x];
            }

            
            if (delta == 0){
                flat_count[x] ++;
            }else{
                flat_count[x] = 0;
            }

            
            //printf("Pixel %d (%d ,%d) 4\n", (uint)index, (uint)x, (unsigned int)y);

            if (bottom_block < 0 || (max_mc_height > 0 && top_block > max_mc_height)){
                //if the block would be out of the world

                //check the start of the staircase
                long tmp_y = (long)start_index[x];

                //if there are an extra blocks before the staircase
                if (start_padding[x] != 0){
                    size_t tmp_o = ( width * ( tmp_y + 1 ) + x );
                    uint3 s_pixel = vload3(tmp_o, dst);
                    tmp_o = (width * ( tmp_y + 1 + (long)start_padding[x] ) + x);
                    uint3 p_pixel = vload3(tmp_o, dst);

                    //if the block is at our height or one higher include it in the staircase
                    if (s_pixel[2] == (p_pixel[2] - 1) || s_pixel[2] == p_pixel[2])
                        tmp_y += (long)start_padding[x];
                }

                size_t tmp_o;
                uint3 o_pixel;
                //loop over the entire staircase
                for (;tmp_y < y && atomic_and(error, 1) != 0 ; tmp_y++){
                    tmp_o = (width * (tmp_y + 1)) + x;
                    o_pixel = vload3(tmp_o, dst);

                    //shift the staircase to accomodate the new block
                    long2 tmp_height = (o_pixel[1], o_pixel[2]);
                    tmp_height -= delta;

                    //check if we are pushing the staircase out of the world
                    if (tmp_height[0] < 0 || (max_mc_height > 0 && tmp_height[1] > max_mc_height)){
                        printf("Error: Pixel (%d,%d) crashed a staircase y:%d-%d\n", (uint)x, (uint)y, o_pixel[1], o_pixel[2]);
                        atomic_or(error, 1);
                    }else{
                        //save the new positions
                        o_pixel[1] = tmp_height[0];
                        o_pixel[2] = tmp_height[1];
                        vstore3(o_pixel, tmp_o, dst);

                        atomic_max(computed_max, tmp_height[1]);
                    }
                }

                //printf("Pixel %d (%d ,%d) 6\n", (uint)index, (uint)x, (unsigned int)y);
                //we have moved by
                bottom_block -= delta;
                top_block    -= delta;

            }
            
            //printf("Pixel %d (%d ,%d) 7\n", (uint)index, (uint)x, (unsigned int)y);
        }
        
        //printf("Pixel %d (%d ,%d) 8\n", (uint)index, (uint)x, (unsigned int)y);
        mc_height[x] = top_block;
        atomic_max(computed_max, mc_height[x]);
        uint3 ret_pixel = {block_id, bottom_block, top_block};
        vstore3(ret_pixel, o ,dst);
        //printf("Pixel %d (%d ,%d) 9\n", (uint)index, (uint)x, (unsigned int)y);
    }
}



__kernel void height_to_stats(
                __global uint                  *src,
                __global volatile uint         *layer_count,
                __global volatile uint         *layer_id_count,
                __global volatile uint         *id_count
                )
{

    size_t index     = get_global_id(0);

    uint3 og_pixel   = vload3(index, src);

    uint block_id    = og_pixel[0];

    uint min_layer   = og_pixel[1];

    uint max_layer   = og_pixel[2];

    for (uchar layer = min_layer; layer <= max_layer; layer++){
        atomic_inc(&layer_count[layer]);
        atomic_inc(&layer_id_count[(layer * UCHAR_MAX) + block_id]);
        atomic_inc(&id_count[block_id]);
    }

}