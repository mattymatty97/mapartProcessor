				   
const static int4 param[10] = {(int4)(0, 1, 5, 32), (int4)(0, 2, 3, 32),
 (int4)(1, -2, 2, 32), (int4)(1, -1, 4, 32), (int4)(1, 0, 5, 32), (int4)(1, 1, 4, 32), (int4)(1, 2, 2, 32),
 (int4)(2, -1, 2, 32), (int4)(2, 0, 3, 32), (int4)(2, 1, 2, 32)};					   
					

__kernel void Sierra(
                  __global double        *src,      
                  __global uchar         *dst,      
                  __global double        *err_buf,     
                  __global double        *Palette,     
                  __global uint          *workgroup_rider, 
                  __global volatile uint *workgroup_progress,
                  __local volatile uint  *progress, 
                  const uchar             channels,
                  const uint              width,
                  const uint              height,
                  const uchar             palette_indexes,
                  const uchar             palette_variations)
{
    __local volatile uint        workgroup_number;

      if (get_local_id(0) == 0)
      {
            workgroup_number        = atomic_inc(workgroup_rider);

        
            for (int i = 0; i < get_local_size(0); i++)
                progress[i]        = 0;
      }


      barrier(CLK_LOCAL_MEM_FENCE);


      int                y = (workgroup_number * get_local_size(0)) + get_local_id(0);
      int                err;
      int                sum;

      for (int x = 0; x < (width); x++)
      {
          if (get_local_id(0) > 0)      
          {
              while (progress[get_local_id(0) - 1] < (x + 2));
          }
          else              
          {
              if (workgroup_number > 0)
                  while (workgroup_progress[workgroup_number - 1] < (x + 2));
          }

            int i = (width * y) + x;
            double4 pixel;
			
            pixel[0] = src[(i * channels)];
            pixel[1] = src[(i * channels) + 1];
            pixel[2] = src[(i * channels) + 2];
            if (channels > 3)
                pixel[3] = src[(i * channels) + 3];
            else {
                pixel[3] = 255;
            }
            
            //printf("Pixel %d %d is [%f, %f, %f, %f]\n", x , y, pixel[0], pixel[1], pixel[2], pixel[3]);

            double4 error = vload4(i, err_buf);

            //printf("Error at %d %d is [%f, %f, %f, %f]\n", x , y, error[0], error[1], error[2], error[3]);

            pixel += error;
			
			
			pixel = max(min(pixel,(double4)(100.0, 128.0, 128.0, 255.0)),(double4)(0.0,-128.0,-128.0, 0.0));

            //printf("Pixel %d %d after error is [%f, %f, %f, %f]\n", x , y, pixel[0], pixel[1], pixel[2], pixel[3]);

            double4 min_d = 0;
            int min_d2_sum = INT_MAX;
            unsigned char min_index = 0;
            unsigned char min_state = 0;

            double4 tmp_d = 0;
            double4 tmp_d2 = INT_MAX;
            int tmp_d2_sum = INT_MAX;
			
            min_d[3] = Palette[3] - pixel[3];
            min_d2_sum = min_d[3] * min_d[3];

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
                        min_d = tmp_d + 0;
                    }

                }
            }
            //printf("Pixel %d %d Error is [%f,%f,%f,%f]\n", x , y, min_d[0], min_d[1], min_d[2], min_d[3]);
            //printf("Result Pixel %d %d is %d %d\n", x , y, (int)min_index ,(int)min_state);
            vstore2((uchar2)(min_index,min_state), i, dst);

            int error_index;
            int4 dst_error;
            
            for (uchar j = 0; j < 10; j++){
                //do not go out or range
                if (( x + param[j][1] ) >= 0 &&  ( x + param[j][1]) < width && ( y + param[j][0]) >= 0 && ( y + param[j][0]) < height){
                    error_index =  (width * ( y + param[j][0])) + x + param[j][1];
                    double4 spread_error = (min_d * param[j][2] / param[j][3]);
                    err_buf[(error_index * 4) + 0]  += spread_error[0];
                    err_buf[(error_index * 4) + 1]  += spread_error[1];
                    err_buf[(error_index * 4) + 2]  += spread_error[2];
                    err_buf[(error_index * 4) + 3]  += spread_error[3];
                }
            }

          if (get_local_id(0) == (get_local_size(0) - 1)){
              workgroup_progress[workgroup_number]  = x;
          }else{
              progress[get_local_id(0)]             = x;
		  }
      }

      if (get_local_id(0) == (get_local_size(0) - 1)){
          workgroup_progress[workgroup_number]       = width + 2;
      }else{
          progress[get_local_id(0)]                  = width + 2;
      }
  }
