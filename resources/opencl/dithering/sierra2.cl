

const static int4 param[7] = {
                                                                  (int4)(0, 1, 4, 16), (int4)(0, 2, 3, 16),
 (int4)(1, -2, 1, 16), (int4)(1, -1, 2, 16), (int4)(1, 0, 3, 16), (int4)(1, 1, 2, 16), (int4)(1, 2, 1, 16)};

__kernel void SierraTwo (
                  __global int           *src,               // The source greyscale image buffer
                  __global uchar         *dst,               // The destination buffer
                  __global int           *err_buf,           // The distribution of errors buffer
                  __global int           *Palette,           // The distribution of errors buffer
                  __global uint          *workgroup_rider,   // A single rider used to create a unique workgroup index
                  __global volatile uint *workgroup_progress,// A buffer of progress markers for each workgroup
                  __local volatile uint  *progress,          // The local buffer for each workgroup
                  const uchar             channels,
                  const uint              width,             // The width of the image
                  const uint              height,             // The height of the image
                  const uchar             palette_indexes,
                  const uchar             palette_variations)
{
    __local volatile uint        workgroup_number;

    /* We need to put the workgroups in some order. This is done by
        the first work item in the workgroup atomically incrementing
        the global workgroup rider. The local progress buffer - used
        by the work items in this workgroup also needs initialising..
      */
      if (get_local_id(0) == 0)            // A job for the first work item...
      {
            // Get the global order for this workgroup...
            workgroup_number        = atomic_inc(workgroup_rider);

            // Initialise the local progress markers...
            for (int i = 0; i < get_local_size(0); i++)
                progress[i]        = 0;
      }


      barrier(CLK_LOCAL_MEM_FENCE);        // Wait here so we know progress buffer and
                                          // workgroup_number have been initialised


/* The area of the image we work on depends on the workgroup_number determined earlier.
        We multiply this by the workgroup size and add the local id index. This gives us the
        y value for the row this work item needs to calculate. Normally we would expect to
        use get_global_id to determine this, but can't here.
      */
      int                y = (workgroup_number * get_local_size(0)) + get_local_id(0);
      int                err;
      int                sum;

      for (int x = 1; x < (width - 1); x++)  // Each work item processes a line (ignoring 1st and last pixels)...
      {
          /* Need to ensure that the data in err_buf required by this
              workitem is ready. To do that we need to check the progress
              marker for the line just above us. For the first work item in this
              workgroup, we get this from the global workgroup_progress buffer.
              For other work items we can peek into the progress buffer local
              to this workgroup.

              In each case we need to know that the previous line has reached
              2 pixels on from our own current position...
          */
          if (get_local_id(0) > 0)          // For all work items other than the first in this workgroup...
          {
              while (progress[get_local_id(0) - 1] < (x + 3));
          }
          else                              // For the first work item in this workgroup...
          {
              if (workgroup_number > 0)
                  while (workgroup_progress[workgroup_number - 1] < (x + 3));
          }

          /*
            add our cusotom palette based color matching
          */
            int i = (width * y) + x;
            int4 pixel;
            //read the pixel
            pixel[0] = src[(i * channels)];
            pixel[1] = src[(i * channels) + 1];
            pixel[2] = src[(i * channels) + 2];
            if (channels > 3)
                pixel[3] = src[(i * channels) + 3];
            else {
                pixel[3] = 255;
            }

            //printf("Pixel %d %d is [%d, %d, %d, %d]\n", x , y, pixel[0], pixel[1], pixel[2], pixel[3]);

            //read the error

            int4 error = vload4(i, err_buf);

            //printf("Error at %d %d is [%d, %d, %d, %d]\n", x , y, error[0], error[1], error[2], error[3]);

            //get the padded pixel
            pixel += error;

            //printf("Pixel %d %d after error is [%d, %d, %d, %d]\n", x , y, pixel[0], pixel[1], pixel[2], pixel[3]);

            int4 min_d = 0;
            int min_d2_sum = INT_MAX;
            unsigned char min_index = 0;
            unsigned char min_state = 0;

            int4 tmp_d = 0;
            int4 tmp_d2 = INT_MAX;
            int tmp_d2_sum = INT_MAX;

            //process inxed 0 separately
            //Transparency
            min_d[3] = Palette[3] - pixel[3];
            min_d2_sum = min_d[3] * min_d[3];

            //find closest match
            for(unsigned char p = 1; p < palette_indexes; p++){
                for (unsigned char s = 0; s < palette_variations; s++){
                    int palette_index = p * palette_variations + s;
                    int4 palette = vload4(palette_index, Palette);

                    tmp_d = palette - pixel;
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

            //printf("Result Pixel %d %d is %d %d\n", x , y, (int)min_index ,(int)min_state);
            vstore2((uchar2)(min_index,min_state), i, dst);

            // Distribute the error values...
            int error_index;
            int4 dst_error;
            //
            for (uchar j = 0; j < 7; j++){
                //do not go out or range
                if (( x + param[j][1] ) >= 0 &&  ( x + param[j][1]) < width && ( y + param[j][0]) >= 0 && ( y + param[j][0]) < height){
                    error_index =  (width * ( y + param[j][0])) + x + param[j][1];
                    int4 spread_error = (min_d * param[j][2] / param[j][3]);
                    err_buf[(error_index * 4) + 0]  += spread_error[0];
                    err_buf[(error_index * 4) + 1]  += spread_error[1];
                    err_buf[(error_index * 4) + 2]  += spread_error[2];
                    err_buf[(error_index * 4) + 3]  += spread_error[3];
                }
            }

          /* Set the progress marker for this line...


              If this work item is the last in the workgroup we set
              the global marker so the first item in the next
              workgroup will pick this up.


              For all other workitems we set the local progress marker.
          */
          if (get_local_id(0) == (get_local_size(0) - 1))      // Last work item in this workgroup?
              workgroup_progress[workgroup_number]  = x;
          else
              progress[get_local_id(0)]             = x;
      }

      /* Although this work item has now finished, subsequent lines
          need to be able to continue to their ends. So the relevant
          progress markers need bumping up...
        */
      if (get_local_id(0) == (get_local_size(0) - 1)) // Last work item in this workgroup?
          workgroup_progress[workgroup_number]       = width + 3;
      else
          progress[get_local_id(0)]                  = width + 3;
  }

