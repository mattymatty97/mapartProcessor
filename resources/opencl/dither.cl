//constants
#define LIQUID_DEPTH (int3)(10,5,0)

#define WHT_L 1.8f

#define WHT_C 1.0f

// functions

#define FLT_EQ(x , y) ( (x - y) > -FLT_EPSILON && (x - y) < FLT_EPSILON )
#define FLT_LT(x , y) ( (x - y) < -FLT_EPSILON )
#define FLT_LE(x , y) ( FLT_LT(x, y) || FLT_EQ(x, y) )
#define FLT_GT(x , y) ( (x - y) >  FLT_EPSILON )
#define FLT_GE(x , y) ( FLT_GT(x, y) || FLT_EQ(x, y) )

#define SIGN(x) ((x > 0) - (x < 0))

#define SQR(x) ((x)*(x))

#define DELTA_TO_STATE(x) ((x < 0) ? (0) : ( (x == 0) ? (1) : (2) ) )

__constant char delta_states[3] = { -1, 0 , 1 };

#define STATE_TO_DELTA(x) ( (x >= 0 && x < 3) ? delta_states[x] : 0 )

float deltaHsqr(float4 lab1, float4 lab2){
    float xDE = sqrt(SQR(lab2[1]) + SQR(lab2[2])) - sqrt( SQR(lab1[1]) + SQR(lab1[2]) );

    return SQR( lab2[1] - lab1[1]) + SQR( lab2[2] - lab1[2] ) - SQR(xDE);
}

float deltaH(float4 lab1, float4 lab2){
    return sqrt( deltaHsqr(lab1,lab2) );
}

float deltaCMClab2Hue(float4 lab){
    float bias = 0;
         if (lab[1] >= 0 && lab[0] == 0 ) return 0;
    else if (lab[1] <  0 && lab[0] == 0 ) return 180;
    else if (lab[1] == 0 && lab[0] >  0 ) return 90;
    else if (lab[1] == 0 && lab[0] <  0 ) return 270;
    else if (lab[1] >  0 && lab[0] >  0 ) bias = 0;
    else if (lab[1] <  0                ) bias = 180;
    else if (lab[1] >  0 && lab[0] <  0 ) bias = 360;

    return degrees(atan( lab[2] / lab[1] )) + bias;
}

float deltaCMCsqr(float4 lab1, float4 lab2){
    float xC1 = sqrt( SQR(lab1[1]) + SQR(lab1[2]) );
    float xC2 = sqrt( SQR(lab2[1]) + SQR(lab2[2]) );
    float xff = sqrt( pow(xC1, 4) / ( pow(xC1, 4) + 1900) );
    float xH1 = deltaCMClab2Hue(lab1);

    float xTT = 0;
    if ( xH1 < 164 || xH1 > 345) xTT = 0.36f + fabs( 0.4f * cos( radians(  35.0f + xH1 ) ) );
    else                         xTT = 0.56f + fabs( 0.2f * cos( radians( 168.0f + xH1 ) ) );

    float xSL = 0;
    if ( lab1[0] < 16) xSL = 0.511f;
    else               xSL = ( 0.040975f * lab1[0] ) / ( 1 + ( 0.01765f * lab1[0] ) );

    float xSC = ( ( 0.0638f * xC1 ) / ( 1 + ( 0.0131f * xC1 ) ) ) + 0.638f;
    float xSH = ( ( xff * xTT ) + 1 - xff ) * xSC;
    float xDH = sqrt( SQR( lab2[1] - lab1[1] ) + SQR( lab2[2] - lab1[2] ) - SQR( xC2 - xC1 ) );
          xSL = ( lab2[0] - lab1[0] ) / ( WHT_L * xSL );
          xSC = ( xC2 - xC1 ) / ( WHT_C * xSC );
          xSH = xDH / xSH;
    
    return SQR(xSL) + SQR(xSC) + SQR(xSH) + SQR(lab2[3] - lab1[3]);
}

float deltaCMC(float4 lab1, float4 lab2){
    return sqrt( deltaCMCsqr(lab1,lab2) );
}

//kernel

__kernel void error_bleed(
                    __global float         *src,      
                    __global uchar         *dst,
                    __global int           *err_buf,
                    __global float         *Palette,
                    __global uchar         *valid_palette_ids,
                    __global uchar         *liquid_palette_ids,
                    __global float         *noise,
                    __global int           *mc_height,
                    __global uint          *coord_list,
                    const uint              width,
                    const uint              height,
                    const uchar             palette_indexes,
                    __global int           *bleeding_params,
                    const uchar             bleeding_size,
                    const uchar             min_progress,
                    const int               max_mc_height)
{

    __private uint index = get_global_id(0);

    //printf("Index was %d \n", index);

    __private uint2 coords = vload2(index, coord_list);

    //printf("Coords are %d %d\n", coords[0], coords[1]);

    __private int curr_mc_height = mc_height[coords[0]];

    __private ulong i = (width * coords[1]) + coords[0];

    __private float4 og_pixel = vload4(i, src);

    //printf("Pixel %d %d is [%f, %f, %f, %f]\n", coords[0] , coords[1], og_pixel[0], og_pixel[1], og_pixel[2], og_pixel[3]);
    __private int4   int_error = vload4(i, err_buf);
    __private float4 error     = {int_error[0],int_error[1],int_error[2],int_error[3]};
                     error    /= 1000.f;

    //printf("Error at %d %d is [%f, %f, %f, %f]\n", coords[0] , coords[1], error[0], error[1], error[2], error[3]);

    __private float4 pixel = og_pixel + error;

    //restrict in Lab colorspace
    pixel = max(min(pixel,(float4)(100.0, 128.0, 128.0, 255.0)),(float4)(0.0,-128.0,-128.0, 0.0));

    //printf("Pixel %d %d after error is [%f, %f, %f, %f]\n", coords[0] , coords[1], pixel[0], pixel[1], pixel[2], pixel[3]);

    __private float4 min_d = 0;
    __private float  min_d2_sum = FLT_MAX;
    __private uchar  min_index = 0;
    __private uchar  min_state = 0;

    __private float4 tmp_d = 0;
    __private float4 tmp_d2 = FLT_MAX;
    __private float  tmp_d2_sum = FLT_MAX;

    __private uchar  valid = 0;

    __private uchar  blacklisted_states[3] = {};
    __private uchar  blacklisted_liquid_states[3] = {};

    if (max_mc_height == 0){
        blacklisted_states[0] = 1;
        blacklisted_states[2] = 1;
        blacklisted_liquid_states[0] = 1;
        blacklisted_liquid_states[1] = 1;
    }else {
        for(__private char state = 0; state < 3; state++){
            if (LIQUID_DEPTH[state] > max_mc_height)
                blacklisted_liquid_states[state] = 1;
        }
    }

    //if there is a previous pixel
    if ( coords[1] > 0 ) {
        __private uint p_i = (width * (coords[1] - 1)) + coords[0]; ;
        __private uchar2 prev = vload2(p_i, dst);
        //if the previous pixel was transparent
        if (prev[0] == 0){
            //only valid state is up!
            blacklisted_states[0] = 1;
            blacklisted_states[1] = 1;
        }
    }


    __private int tmp_mc_height = curr_mc_height;

    __private uint abs_mc_height = abs(curr_mc_height);
    
    //randomly reset the height to spread out the errors
    __private float rand = noise[i];
    //have the probability heavily tipped towards high y levels
    __private float f_x = (float)(abs_mc_height) / max_mc_height;
    __private float compare = -log(1 - f_x) / 3;

    if (max_mc_height > 0 && FLT_LT(rand, compare)){
        if (curr_mc_height < 0){
            blacklisted_states[0] = 1;
        }else{
            blacklisted_states[2] = 1;
        }
        
        if (FLT_LT(rand , compare - 0.05f)){
            blacklisted_states[1] = 1;
        }
    }
    
    //check if we're not going out of build limit
    while(!valid){

        min_d = 0;
        min_d2_sum = FLT_MAX;
        min_index = 0;
        min_state = 0;

        tmp_d = 0;
        tmp_d2 = FLT_MAX;
        tmp_d2_sum = FLT_MAX;

        min_d[3] = Palette[3] - pixel[3];
        min_d2_sum = SQR(min_d[3]);

        for(__private uchar p = 1; p < palette_indexes; p++){
            if (valid_palette_ids[p])
                for (__private uchar s = 0; s < 3; s++){
                    if ( ( liquid_palette_ids[p] && blacklisted_liquid_states[s]) || ( !liquid_palette_ids[p] && blacklisted_states[s]) ){
                        continue;
                    }
                    __private int palette_index = p * 3 + s;
                    __private float4 palette = vload4(palette_index, Palette);

                    tmp_d = pixel - palette;

                    tmp_d2_sum = deltaCMCsqr(pixel, palette);

                    if (FLT_LT(tmp_d2_sum, min_d2_sum)){
                        min_d2_sum = tmp_d2_sum;
                        min_index = p;
                        min_state = s;
                        min_d = tmp_d;
                    }

                }
        }

        __private char delta = STATE_TO_DELTA(min_state);
        if (max_mc_height > 0 && min_index != 0){
            if (liquid_palette_ids[min_index]){
                printf("Pixel %d %d choose water %d\n", coords[0] , coords[1], min_state);
                //if this is a liquid
                tmp_mc_height = LIQUID_DEPTH[min_state];
            }else{
                //if we're changing direction reset to 0
                if ( delta == - SIGN(curr_mc_height) ){
                    tmp_mc_height = delta;
                    //printf("Pixel %d %d reset height was: %d\n", coords[0] , coords[1], curr_mc_height);
                }else
                    tmp_mc_height = curr_mc_height + delta;
            }

            valid = abs( tmp_mc_height ) < max_mc_height;
            if (!valid){
                blacklisted_states[min_state] = 1;
                if ( FLT_LT(rand, 0.5f) )
                    blacklisted_states[1] = 1;
                printf("Pixel %d %d reached %d: Restricted\n", coords[0] , coords[1], tmp_mc_height);
            }

        }else{
            valid = 1;
            tmp_mc_height = 0;
            if ( FLT_LT(og_pixel[4], 128.f) ){
                printf("Pixel %d %d defaulted to Transparent\n", coords[0] , coords[1]);
            }
        }
    }

    mc_height[coords[0]] = tmp_mc_height;

    //printf("Pixel %d %d Error is [%f,%f,%f,%f]\n", coords[0] , coords[1], min_d[0], min_d[1], min_d[2], min_d[3]);
    //printf("Result Pixel %d %d is %d %d\n", coords[0] , coords[1], (int)min_index ,(int)min_state);
    vstore2((uchar2){min_index, min_state}, i, dst);

    for (__private uchar j = 0; j < bleeding_size; j++){
        __private int4 param = vload4(j, bleeding_params);

        __private long2 new_coords;
        new_coords[0] = (long)coords[0] + (long)param[0];
        new_coords[1] = (long)coords[1] + (long)param[1];

        //do not go out or range
        if ( new_coords[0] >= 0L && new_coords[0] < width 
        &&   new_coords[1] >= 0L && new_coords[1] < height){

            __private uint   error_index =  (width * new_coords[1]) + new_coords[0];
            
            __private float4 spread_error = (min_d * (float)param[2] / (float)param[3]);
            __private float4 tmp_spread_error = spread_error * 1000.f;
            __private int4   int_spread_error = {tmp_spread_error[0],tmp_spread_error[1],tmp_spread_error[2], tmp_spread_error[3]};

            __private float4 dst_pixel = vload4(error_index, src);

            __private float dH = deltaHsqr(pixel, dst_pixel);
            __private float dA = pixel[3] - dst_pixel[3];

            atomic_add(     &(err_buf[(error_index * 4) + 0]) , int_spread_error[0] );

            if (FLT_LT(dH, 400.f)){
                atomic_add( &(err_buf[(error_index * 4) + 1]) , int_spread_error[1] );
                atomic_add( &(err_buf[(error_index * 4) + 2]) , int_spread_error[2] );
            }

            if (FLT_LT(dA, 128.f)){
                atomic_add( &(err_buf[(error_index * 4) + 3]) , int_spread_error[3] );
            }
        }
    }
}
