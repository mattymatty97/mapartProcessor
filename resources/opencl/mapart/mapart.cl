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