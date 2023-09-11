
//reference values for sRGB
static const float4 reference_srgb_10 = {
    94.811f,
    100.000f,
    107.304f,
    1.0f
};

static const float4 reference_ICC = {
    96.720f,
    100.000f,
    81.427f,
    1.0f
};

static const float4 reference_srgb_2 = {
    95.047f,
    100.000f,
    108.883f,
    1.0f
};

static const float4 reference = reference_srgb_10;

__kernel void rgb_to_XYZ(__global const int *In, __global float *Out) {

    // Get the index of the current element to be processed
    int i = get_global_id(0);

    //read the pixel
    int4 rgb = vload4(i, In);

    //printf("Pixel %d is [%d,%d,%d,%d]\n", i, rgb[0], rgb[1], rgb[2], rgb[3]);
    //convert to XYZ

    float3 var = 0;

    for (int i=0; i<3; i++){
        var[i] = (float)rgb[i] / 255.0f;

        if ( var[i] > 0.04045 )
            var[i] = pow((var[i] + 0.055f) / 1.055f, 2.4f);
        else
            var[i] = var[i] / 12.92f;

        var[i] = var[i] * 100.0f;
    }


    //printf("var for Pixel %d is [%f,%f,%f,%f]\n", i, var[0], var[1], var[2], var[3]);
    //write results

    float4 XYZ = {
        var[0] * 0.4124f + var[1] * 0.3576f + var[2] * 0.1805f,
        var[0] * 0.2126f + var[1] * 0.7152f + var[2] * 0.0722f,
        var[0] * 0.0193f + var[1] * 0.1192f + var[2] * 0.9505f,
        rgb[3]
    };

    //printf("Result Pixel %d is [%f,%f,%f,%f]\n", i, XYZ[0], XYZ[1], XYZ[2], XYZ[3]);
    vstore4(XYZ, i, Out);

}


__kernel void xyz_to_lab(__global const float *In, __global float *Out) {

    // Get the index of the current element to be processed
    int i = get_global_id(0);

    //read the pixel
    float4 XYZ = vload4(i, In);

    float4 var = 0;

    //printf("Pixel in [%f,%f,%f,%f]\n", XYZ[0], XYZ[1], XYZ[2], XYZ[3]);

    //convert to L*ab

    var = XYZ / reference;

    for (int i=0; i<3; i++){
        if ( var[i] > 0.008856 )
            var[i] = pow(var[i] , 1/3.0f);
        else
            var[i] = ( var[i] * 7.787f) + (16 / 116.0f);
    }

    float4 Lab = {
        (116 * var[1]) - 16,
        500 * (var[0] - var[1]),
        200 * (var[1] - var[2]),
        XYZ[3]
    };

    Lab = max(min(Lab, (float4)(100,128,128,255)), (float4)(0,-128,-128,0));

    //wirte results
    vstore4(Lab, i, Out);
}

__kernel void lab_to_lch(__global const float *In, __global float *Out) {
    // Get the index of the current element to be processed
    int i = get_global_id(0);

    //read the pixel
    float4 lab = vload4(i, In);

    //convert to L*ch

    float var = atan((float)lab[1]/lab[2]);

    if ( var > 0 )
        var = ( var / M_PI_F ) * 180.0f;
    else
         var = 360 - ( (- var) / M_PI_F ) * 180.0f;



    //wirte results

    float4 Lch = {
        lab[0],
        sqrt((float)(lab[1]*lab[1]) + (lab[2]*lab[2])),
        var,
        lab[3]
    };

    
    Lch = max(min(Lch, (float4)(100,100,360,255)), (float4)(0,0,0,0));

    vstore4(Lch, i, Out);

}