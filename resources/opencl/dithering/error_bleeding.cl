char sign(int x) {
    return (x > 0) - (x < 0);
}

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

            float4 pixel = vload4(i, src);
            
            //printf("Pixel %d %d is [%f, %f, %f, %f]\n", x , y, pixel[0], pixel[1], pixel[2], pixel[3]);

            float4 error = vload4(i, err_buf);

            //printf("Error at %d %d is [%f, %f, %f, %f]\n", x , y, error[0], error[1], error[2], error[3]);
            float4 d_error = error * error;

            if(d_error[0] + d_error[1] + d_error[2] + d_error[3] < 500)
                pixel += error;
			
			
			pixel = max(min(pixel,(float4)(100.0, 128.0, 128.0, 255.0)),(float4)(0.0,-128.0,-128.0, 0.0));

            //printf("Pixel %d %d after error is [%f, %f, %f, %f]\n", x , y, pixel[0], pixel[1], pixel[2], pixel[3]);

            float4 min_d = 0;
            float min_d2_sum = INT_MAX;
            unsigned char min_index = 0;
            unsigned char min_state = 0;

            float4 tmp_d = 0;
            float4 tmp_d2 = INT_MAX;
            float tmp_d2_sum = INT_MAX;

            bool valid = false;

            bool blacklisted_states[4] = {};

            if (max_mc_height == 0){
                blacklisted_states[0] = true;
                blacklisted_states[2] = true;
            }

            int curr_mc_height = mc_y[get_local_id(0)];
            
            int tmp_mc_height = 0;

            uint abs_mc_height = abs(curr_mc_height);

            uint reference = max_mc_height/2;
            
            //randomly reset the height to spread out the errors
            float rand = noise[i];

            float f_x = abs(curr_mc_height)/ (float)max_mc_height;
            float compare = (pow((float)reference, f_x) - 1) / (reference - 1);
            if (max_mc_height > 0 && rand < compare){
                blacklisted_states[sign(curr_mc_height) + 1] = true;
                if (rand < compare - 0.005f){
                    blacklisted_states[1] = true;
                }
            }
            
            
            //check if we're not going our of build limit
            while(!valid){

                min_d = 0;
                min_d2_sum = SHRT_MAX;
                min_index = 0;
                min_state = 0;

                tmp_d = 0;
                tmp_d2 = SHRT_MAX;
                tmp_d2_sum = SHRT_MAX;
			
                min_d[3] = Palette[3] - pixel[3];
                min_d2_sum = min_d[3] * min_d[3];

                for(unsigned char p = 1; p < palette_indexes; p++){
                    for (unsigned char s = 0; s < palette_variations; s++){
                        if (blacklisted_states[s]){
                                continue;
                        }
                        int palette_index = p * palette_variations + s;
                        float4 palette = vload4(palette_index, Palette);

                        tmp_d = pixel - palette;
                        tmp_d2 = tmp_d * tmp_d * (float4)((pixel[0]<30)?10:1,1,1,1);
                        tmp_d2_sum = tmp_d2[0] + tmp_d2[1] + tmp_d2[2] + tmp_d2[3];

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
                    if ( sign(delta) == -sign(curr_mc_height) ){
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
                    err_buf[(error_index * 4) + 0]  += spread_error[0];
                    err_buf[(error_index * 4) + 1]  += spread_error[1];
                    err_buf[(error_index * 4) + 2]  += spread_error[2];
                    err_buf[(error_index * 4) + 3]  += spread_error[3];
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
