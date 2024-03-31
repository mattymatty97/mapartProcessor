__kernel void rgba_composite(__global const int *In, __global int *Out) {
    // Get the index of the current element to be processed
    __private int i = get_global_id(0);

    //read the pixel
    __private int4 rgba = vload4(i, In);

    __private int3 rgb = {rgba[0], rgba[1], rgba[2]};

    rgb *= rgba[3];
    rgb /= 255;

    __private int4 rgba_ = { rgb[0], rgb[1], rgb[2], rgba[3] };

    vstore4(rgba_, i, Out);

}

static const __constant float M1[3][3] = {
    0.4122214708f , 0.5363325363f , 0.0514459929f,
	0.2119034982f , 0.6806995451f , 0.1073969566f,
	0.0883024619f , 0.2817188376f , 0.6299787005f
};

static const __constant float M2[3][3] = {
    0.2104542553f,  0.7936177850f, -0.0040720468f,
    1.9779984951f, -2.4285922050f,  0.4505937099f,
    0.0259040371f,  0.7827717662f, -0.8086757660f
};

__kernel void rgb_to_ok(__global const int *In, __global float *Out) {

    // Get the index of the current element to be processed
    __private int i = get_global_id(0);

    //read the pixel
    __private int4 rgb = vload4(i, In);

    //convert to okLab

    __private float3 var = {
        rgb[0]*M1[0][0] + rgb[1]*M1[0][1] + rgb[2]*M1[0][2],
        rgb[0]*M1[1][0] + rgb[1]*M1[1][1] + rgb[2]*M1[1][2],
        rgb[0]*M1[2][0] + rgb[1]*M1[2][1] + rgb[2]*M1[2][2]
    };

    __private float3 var_ = cbrt(var);


    __private float4 ok = {
        var_[0]*M2[0][0] + var_[1]*M2[0][1] + var_[2]*M2[0][2],
        var_[0]*M2[1][0] + var_[1]*M2[1][1] + var_[2]*M2[1][2],
        var_[0]*M2[2][0] + var_[1]*M2[2][1] + var_[2]*M2[2][2],
        rgb[3]
    };

    vstore4(ok, i, Out);

}

