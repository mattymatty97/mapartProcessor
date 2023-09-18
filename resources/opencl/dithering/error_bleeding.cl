//constants

#define WHT_L 1.8f

#define WHT_C 1.0f

// functions

#define SIGN(x) (x > 0) - (x < 0)

#define SQR(x) (x)*(x)

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

__kernel void Error_bleed_dither_by_cols(
                  __global float         *src,      
                  __global uchar         *dst,      
                  __global float         *err_buf,     
                  __global float         *Palette,  
                  __global float         *noise,
                  __global uint          *workgroup_rider, 
                  __global volatile uint *workgroup_progress,
                  __local volatile uint  *progress,
                  const uint              width,
                  const uint              height,
                  const uchar             palette_indexes,
                  const uchar             palette_variations,
                  __global int           *bleeding_params,
                  const uchar             bleeding_size,
                  const uchar             min_progress,
                  __local volatile int   *mc_y,
                  const int              max_mc_height)
{
    __local volatile uint        workgroup_number;

    if (get_local_id(0) == 0)
    {
        workgroup_number        = atomic_inc(workgroup_rider);

        for (int i = 0; i < get_local_size(0); i++)
            progress[i]        = 0;
        
        for (int i = 0; i < get_local_size(0); i++)
            mc_y[i]        = 0;
    }


    barrier(CLK_LOCAL_MEM_FENCE);


    int x = (workgroup_number * get_local_size(0)) + get_local_id(0);

    for (int y = 0; y < (height); y++)
    {
        if (get_local_id(0) > 0)      
        {
            while (progress[get_local_id(0) - 1] < (y + min_progress));
        }
        else              
        {
            if (workgroup_number > 0)
                while (workgroup_progress[workgroup_number - 1] < (y + min_progress));
        }

        int i = (width * y) + x;

        float4 og_pixel = vload4(i, src);

        //printf("Pixel %d %d is [%f, %f, %f, %f]\n", x , y, pixel[0], pixel[1], pixel[2], pixel[3]);

        float4 error = vload4(i, err_buf);

        //printf("Error at %d %d is [%f, %f, %f, %f]\n", x , y, error[0], error[1], error[2], error[3]);
        
        float4 pixel = og_pixel + error;
        
        //restrict in Lab colorspace
        pixel = max(min(pixel,(float4)(100.0, 128.0, 128.0, 255.0)),(float4)(0.0,-128.0,-128.0, 0.0));

        //restrict in Luv colorspace
        //pixel = max(min(pixel, (float4)(100,224,122,255)), (float4)(0,-134,-240,0));

        //printf("Pixel %d %d after error is [%f, %f, %f, %f]\n", x , y, pixel[0], pixel[1], pixel[2], pixel[3]);

        float4 min_d = 0;
        float min_d2_sum = FLT_MAX;
        unsigned char min_index = 0;
        unsigned char min_state = 0;

        float4 tmp_d = 0;
        float4 tmp_d2 = FLT_MAX;
        float tmp_d2_sum = FLT_MAX;

        bool valid = false;

        bool blacklisted_states[4] = {};

        if (max_mc_height == 0){
            blacklisted_states[0] = true;
            blacklisted_states[2] = true;
        }

        int curr_mc_height = mc_y[get_local_id(0)];
        
        int tmp_mc_height = 0;

        uint abs_mc_height = abs(curr_mc_height);

        uint reference = max_mc_height / 2;
        
        //randomly reset the height to spread out the errors
        float rand = noise[i];

        //have the probability heavily tipped towards high y levels
        float f_x = abs(curr_mc_height)/ (float)max_mc_height;
        float compare = (pow((float)reference, f_x) - 1) / (reference - 1);


        if (max_mc_height > 0 && rand < compare){
            blacklisted_states[SIGN(curr_mc_height) + 1] = true;
            if (rand < compare - 0.005f){
                blacklisted_states[1] = true;
            }
        }
		        
        //check if we're not going our of build limit
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

            for(unsigned char p = 1; p < palette_indexes; p++){
                if (p != 60)
                    for (unsigned char s = 0; s < palette_variations; s++){
                        if (blacklisted_states[s]){
                                continue;
                        }
                        int palette_index = p * palette_variations + s;
                        float4 palette = vload4(palette_index, Palette);

                        tmp_d = pixel - palette;

                        tmp_d2_sum = deltaCMCsqr(pixel, palette);

                        if (tmp_d2_sum < min_d2_sum){
                            min_d2_sum = tmp_d2_sum;
                            min_index = p;
                            min_state = s;
                            min_d = tmp_d + 0;
                        }

                    }
            }

            char delta = min_state - 1;
            if (max_mc_height > 0 && min_index != 0){
                //if we're changing direction reset to 0
                if ( SIGN(delta) == -SIGN(curr_mc_height) ){
                    tmp_mc_height = delta;
                    //printf("Pixel %d %d reset height was: %d\n", x , y, curr_mc_height);
                }else
                    tmp_mc_height = curr_mc_height + delta;
                
                valid = abs(tmp_mc_height) < max_mc_height;

                if (!valid){
                    blacklisted_states[min_state] = true;
                    if ( noise[i] > 0.5f)
                        blacklisted_states[1] = true;
                    printf("Pixel %d %d reached %d: Restricted\n", x , y, tmp_mc_height);
                }
            }else{
                valid = true;
            }
        }

        mc_y[get_local_id(0)] = tmp_mc_height;

        //printf("Pixel %d %d Error is [%f,%f,%f,%f]\n", x , y, min_d[0], min_d[1], min_d[2], min_d[3]);
        //printf("Result Pixel %d %d is %d %d\n", x , y, (int)min_index ,(int)min_state);
        vstore2((uchar2)(min_index,min_state), i, dst);

        int error_index;
        int4 dst_error;
        
        for (uchar j = 0; j < bleeding_size; j++){
            int4 param = vload4(j, bleeding_params);
            //do not go out or range
            if (( x + param[0] ) >= 0 &&  ( x + param[0]) < width && ( y + param[1]) >= 0 && ( y + param[1]) < height){
                error_index =  (width * ( y + param[1])) + x + param[0];
                float4 spread_error = (min_d * param[2] / param[3]);
                float4 dst_pixel = vload4(error_index, src);


                float dH = deltaHsqr(pixel, dst_pixel);
                float dA = pixel[3] - dst_pixel[3];
                
                err_buf[(error_index * 4) + 0]  += spread_error[0];

                if (dH < 400){
                    err_buf[(error_index * 4) + 1]  += spread_error[1];
                    err_buf[(error_index * 4) + 2]  += spread_error[2];
                }

                if (dA < 128){
                    err_buf[(error_index * 4) + 3]  += spread_error[3];
                }
            }
        }

        if (get_local_id(0) == (get_local_size(0) - 1)){
            workgroup_progress[workgroup_number]  = y;
        }else{
            progress[get_local_id(0)]             = y;
        }
    }

    if (get_local_id(0) == (get_local_size(0) - 1)){
        workgroup_progress[workgroup_number]       = height + min_progress;
    }else{
        progress[get_local_id(0)]                  = height + min_progress;
    }
}
