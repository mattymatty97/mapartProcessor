
//reference values for sRGB
static const float reference_srgb[3] = {
    94.811f,
    100.000f,
    107.304f
};

static const float reference_ICC[3] = {
    96.720f,
    100.000f,
    81.427f
};

static const float reference[3] = {
    94.811f,
    100.000f,
    107.304f
};
__kernel void rgb_to_XYZ(__global const int *In, __global float *Out, const unsigned char channels) {

    // Get the index of the current element to be processed
    int i = get_global_id(0);

    //save the pixel
    int rgb[4];
    rgb[0] = In[(i * channels)];
    rgb[1] = In[(i * channels) + 1];
    rgb[2] = In[(i * channels) + 2];
    if (channels > 2)
        rgb[3] = In[(i * channels) + 3];


    //convert to XYZ
    float var[3];

    for (int i=0; i<3; i++){
        var[i] = (rgb[i] / 255.0f);

        if ( var[i] > 0.04045f )
            var[i] = pow((var[i] + 0.055f) / 1.055f, 2.4f);
        else
            var[i] = var[i] / 12.92f;


        var[i] = var[i] * 100;
    }

    //wirte results

    Out[ i * channels]        = var[0] * 0.4124 + var[1] * 0.3576 + var[2] * 0.1805;
    Out[ (i * channels ) + 1 ]= var[0] * 0.2126 + var[1] * 0.7152 + var[2] * 0.0722;
    Out[ (i * channels ) + 2 ]= var[0] * 0.0193 + var[1] * 0.1192 + var[2] * 0.9505;

    //keep alpha channel as original if any
    if (channels > 2)
        Out[ (i * channels ) + 3 ]= rgb[3];


    //fill remaining channels if any
    for (int j = 4; j < channels; j++){
        Out[ (i * channels ) + j ]= 0;
    }
}


__kernel void xyz_to_lab(__global const float *In, __global int *Out, const unsigned char channels) {

    // Get the index of the current element to be processed
    int i = get_global_id(0);

    //save the pixel
    float XYZ[4];
    XYZ[0] = In[(i * channels)];
    XYZ[1] = In[(i * channels) + 1];
    XYZ[2] = In[(i * channels) + 2];
    if (channels > 2)
        XYZ[3] = In[(i * channels) + 3];

    float var[3] = {};

    //convert to L*ab

    for (int i=0; i<3; i++){
        var[i] = XYZ[i] / reference[i];

        if ( var[i] > 0.008856 )
            var[i] = pow(var[i] , 1/3.0f);
        else
            var[i] = ( var[i] * 7.787f) + (16 / 116.0f);
    }


    //wirte results
    
    Out[ (i * channels )]     = min(max((int)round((116 * var[1]) - 16), 0), 100);
    Out[ (i * channels ) + 1 ]= min(max((int)round(500 * (var[0] - var[1])), -128), 128);
    Out[ (i * channels ) + 2 ]= min(max((int)round(200 * (var[1] - var[2])), -128), 128);

    //keep alpha channel as original if any
    if (channels > 2)
        Out[ (i * channels ) + 3 ]= round(XYZ[3]);

    //fill remaining channels if any
    for (int j = 4; j < channels; j++){
        Out[ (i * channels ) + j ]= 0;
    }
}

__kernel void lab_to_lch(__global const int *In, __global int *Out, const unsigned char channels) {
    // Get the index of the current element to be processed
    int i = get_global_id(0);

    //save the pixel
    int lab[4];
    lab[0] = In[(i * channels)];
    lab[1] = In[(i * channels) + 1];
    lab[2] = In[(i * channels) + 2];
    if (channels > 2)
        lab[3] = In[(i * channels) +3];


    //convert to L*ch

    float var = atan((float)lab[1]/lab[2]);

    if ( var > 0 )
        var = ( var / M_PI_F ) * 180.0f;
    else
         var = 360 - ( (- var) / M_PI_F ) * 180.0f;



    //wirte results

    Out[ (i * channels )]     = min(max(lab[0],0), 100);
    Out[ (i * channels ) + 1 ]= min(max((int)round(sqrt((float)(lab[1]*lab[1]) + (lab[2]*lab[2]))), 0), 100);
    Out[ (i * channels ) + 2 ]= min(max((int)round(var), 0), 360 );

    //keep alpha channel as original if any
    if (channels > 3)
        Out[ (i * channels ) + 3 ]= lab[3];

    //fill remaining channels if any
    for (int j = 4; j < channels; j++){
        Out[ (i * channels ) + j ]= 0;
    }

}