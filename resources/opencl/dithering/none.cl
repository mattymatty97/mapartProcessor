
__kernel void no_dithering(__global const double *In, __global const double *Palette, __global unsigned char *Out,
                           const unsigned char channels, const unsigned char palette_indexes, const unsigned char palette_variations) {

    // Get the index of the current element to be processed
    int i = get_global_id(0);

    double4 pixel;
    //save the pixel
    pixel[0] = In[(i * channels)];
    pixel[1] = In[(i * channels) + 1];
    pixel[2] = In[(i * channels) + 2];
    if (channels > 3)
        pixel[3] = In[(i * channels) + 3];
    else {
        pixel[3] = 255;
    }

    double4 min_d = 0;
    double min_d2_sum = INT_MAX;
    unsigned char min_index = 0;
    unsigned char min_state = 0;

    double4 tmp_d = 0;
    double4 tmp_d2 = INT_MAX;
    double tmp_d2_sum = INT_MAX;

    //process inxed 0 separately
    //Transparency
    min_d[3] = Palette[3] - pixel[3];
    min_d2_sum = min_d[3] * min_d[3];

    //find closest match
    for(unsigned char p = 1; p < palette_indexes; p++){
        for (unsigned char s = 0; s < palette_variations; s++){
            int palette_index = p * palette_variations + s;
            double4 palette = vload4(palette_index, Palette);

            tmp_d = pixel - palette;
            tmp_d2 = tmp_d * tmp_d;
            tmp_d2_sum = tmp_d2[0] + tmp_d2[1] + tmp_d2[2] + tmp_d2[3];

            if (tmp_d2_sum < min_d2_sum){
                min_d2_sum = tmp_d2_sum;
                min_index = p;
                min_state = s;
                min_d = tmp_d;
            }

        }
    }
    //printf("Result Pixel is %d %d\n", (int)min_index ,(int)min_state);
    vstore2((uchar2)(min_index,min_state), i, Out);
}