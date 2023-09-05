#include <unistd.h>
#include <getopt.h>

#include "../../libs/images/stb_image.h"
#include "../../libs/globaldefs.h"

typedef struct {
    char* image_filename;
} command_options;

typedef struct {
    unsigned char* image_data;
    int x;
    int y;
    int channels;
} image_data;

void image_cleanup(image_data* image);

int load_image(command_options *options, image_data* image);

int load_image_command(int argc, char** argv, main_options *config){
    command_options local_config = {};
    int ret = 0;

    printf("\nLoad command start\n\n");
    static struct option long_options[] = {
            {"image", required_argument, 0, 'i'}
    };

    int c;
    opterr = 0;
    int option_index = 0;
    while ((c = getopt_long (argc, argv, "+:p:", long_options, &option_index)) != -1){
        switch (c)
        {
            case 0:
                /* If this option set a flag, do nothing else now. */
                if (long_options[option_index].flag != 0)
                    break;
                printf ("option %s", long_options[option_index].name);
                if (optarg)
                    printf (" with arg %s", optarg);
                printf ("\n");
                break;

            case 'i':
                local_config.image_filename = strdup(optarg);
                break;

            case ':':
                printf("option needs a value\n");
                exit(1);
                break;

            default :
                /* unknown flag ignore for now. */
                break;
        }
    }

    if (local_config.image_filename == 0){
        printf("missing required options\n");
        return 4;
    }

    image_data image = {};

    ret = load_image(&local_config, &image);

    //if everything is ok
    if (ret == 0){
        ;
    }

    image_cleanup(&image);
    return ret;
}

int load_image(command_options *options, image_data* image){
    printf("Loading image\n");
    //load the image
    if(access(options->image_filename, F_OK) == 0 && access(options->image_filename, R_OK) == 0) {
        image->image_data = stbi_load(options->image_filename, &image->x, &image->y, &image->channels, 0);
        if (image->image_data == NULL){
            fprintf (stderr,"Failed to load image %s:\n%s\n", options->image_filename, stbi_failure_reason());
            return 10;
        }
        printf("Image loaded: %dx%d(%d)\n\n", image->x, image->y, image->channels);
    }else{
        fprintf (stderr,"Failed to load image %s:\nFile does not exists\n", options->image_filename);
        return 10;
    }
    return 0;
}

void image_cleanup(image_data* image){
    if (image->image_data != NULL)
        stbi_image_free(image->image_data);
}