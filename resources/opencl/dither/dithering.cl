static const char RGBA_SIZE = 4;

__kernel void no_dithering(__global const int *In, __global const int *palette, __global unsigned char *Out,
    const unsigned char channels, const unsigned char palette_indexes, const unsigned char palette_variations) {

    // Get the index of the current element to be processed
    int i = get_global_id(0);

    //save the pixel
    int pixel[4];
    pixel[0] = In[(i * channels)];
    pixel[1] = In[(i * channels) + 1];
    pixel[2] = In[(i * channels) + 2];
    if (channels > 2)
        pixel[3] = In[(i * channels) + 3];
    else
        //assume full opacity
        pixel[3] = 255;

    int min_d[4] = {};
    long min_d2 = LONG_MAX;
    unsigned char min_index = -1;
    unsigned char min_state = -1;

    int tmp_d[4] = {};
    long tmp_d2 = LONG_MAX;

    //process inxed 0 separately
    //Transparency
    min_d[3] = palette[3] - pixel[3];
    min_d2 = min_d[3] * min_d[3];

    //find closest match
    for(unsigned char p = 1; p < palette_indexes; p++){
        for (unsigned char s = 0; s < palette_variations; s++){
            int * p_value = &(palette[p * palette_variations * RGBA_SIZE + s * RGBA_SIZE]);
            tmp_d2 = 0;
            for (int j = 0; j < RGBA_SIZE ; j ++){
                tmp_d[j] = p_value[j] - pixel[j];
                tmp_d2 += tmp_d[j] * tmp_d[j];
            }
            
            if (tmp_d2 < min_d2){
                min_d2 = tmp_d2;
                min_index = p;
                min_state = s;
                for (int j = 0; j < RGBA_SIZE ; j ++){
                    min_d[j] = tmp_d[j];
                }
            }

        }
    }

    Out[(i * 2)] = min_index;
    Out[(i * 2) + 1] = min_state;
}