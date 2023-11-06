__kernel void progress(
                const uint              width,
                const uint              height)
{
    size_t i = get_global_id(0);
    size_t total = width * height;

    double percentage = (double)i/total;

    percentage *= 100;

    printf("Current Progress is %f%% %llu/%llu pixels\n", percentage, i, total);
}