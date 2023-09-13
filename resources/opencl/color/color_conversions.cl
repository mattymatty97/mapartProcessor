
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

    float4 var = 0;

    var = convert_float4(rgb) / 255.0f;
    
    for (int i=0; i<3; i++){

        if ( var[i] > 0.04045 )
            var[i] = pow((var[i] + 0.055f) / 1.055f, 2.4f);
        else
            var[i] = var[i] / 12.92f;

    }

    //var = pow(var, 2.19921875f);

    var = var * 100.0f;

    //printf("var for Pixel %d is [%f,%f,%f,%f]\n", i, var[0], var[1], var[2], var[3]);
    //write results

    float4 XYZ = {
        var[0] * 0.412390799265959f + var[1] * 0.357584339383878f + var[2] * 0.180480788401834f,
        var[0] * 0.212639005871510f + var[1] * 0.715168678767756f + var[2] * 0.072192315360734f,
        var[0] * 0.019330818715592f + var[1] * 0.119194779794626f + var[2] * 0.950532152249661f,
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



__kernel void xyz_to_luv(__global const float *In, __global float *Out) {

    // Get the index of the current element to be processed
    int i = get_global_id(0);

    //read the pixel
    float4 XYZ = vload4(i, In);

    float4 var = 0;

    //printf("Pixel in [%f,%f,%f,%f]\n", XYZ[0], XYZ[1], XYZ[2], XYZ[3]);

    //convert to L*uv

    var[0] = ( 4 * XYZ[0] ) / ( XYZ[0] + ( 15 * XYZ[1] ) + ( 3 * XYZ[2] ) );
    var[1] = ( 9 * XYZ[1] ) / ( XYZ[0] + ( 15 * XYZ[1] ) + ( 3 * XYZ[2] ) );
    
    var[2] = XYZ[1] / 100;
    if ( var[2] > 0.008856 )
        var[2] = pow(var[2] , 1/3.0f);
    else
        var[2] = ( var[2] * 7.787f) + (16 / 116.0f);
    
    float2 ref = 0;

    ref[0] = ( 4 * reference[0] ) / ( reference[0] + ( 15 * reference[1] ) + ( 3 * reference[2] ) );
    ref[1] = ( 9 * reference[0] ) / ( reference[0] + ( 15 * reference[1] ) + ( 3 * reference[2] ) );

    float4 Luv = 0;
    Luv[0] = (116 * var[2]) - 16;
    Luv[1] = 13 * Luv[0] * (var[0] / ref[0]);
    Luv[2] = 13 * Luv[0] * (var[1] / ref[1]);
    Luv[3] = XYZ[3];

    Luv = max(min(Luv, (float4)(100,224,122,255)), (float4)(0,-134,-240,0));

    //wirte results
    vstore4(Luv, i, Out);
}





__kernel void lab_to_lch(__global const float *In, __global float *Out) {
    // Get the index of the current element to be processed
    int i = get_global_id(0);

    //read the pixel
    float4 lab = vload4(i, In);

    //convert to L*ch

    float var = atan2(lab[2],lab[1]);

    if ( var > 0 )
        var = ( var / M_PI_F ) * 180.0f;
    else
         var = 360 - ( fabs(var) / M_PI_F ) * 180.0f;

    //wirte results

    float4 lch = {
        lab[0],
        sqrt((lab[1]*lab[1]) + (lab[2]*lab[2])),
        var,
        lab[3]
    };

    
    lch = max(min(lch, (float4)(100,100,360,255)), (float4)(0,0,0,0));

    vstore4(lch, i, Out);

}


__kernel void lch_to_lab(__global const float *In, __global float *Out) {
    // Get the index of the current element to be processed
    int i = get_global_id(0);

    //read the pixel
    float4 lch = vload4(i, In);

    //convert to L*ab
    float4 lab = {
        lch[0],
        cos(radians(lch[2])) * lch[1] ,
        sin(radians(lch[2])) * lch[1] ,
        lch[3]
    };
    
    lab = max(min(lab, (float4)(100,128,128,255)), (float4)(0,-128,-128,0));

    vstore4(lab, i, Out);

}