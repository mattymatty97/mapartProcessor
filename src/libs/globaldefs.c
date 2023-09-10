#include <stdio.h>
#include <unistd.h>
#include "globaldefs.h"

void * logger_worker(void * arg){/*
    logger_t *worker_options = arg;
    unsigned long total = worker_options->total;
    unsigned long count = *((unsigned long *)worker_options->count);
    while (count < total && !worker_options->stopped){
        double percentage = (double)count / total * 100;
        fprintf(stdout,"Progress %lu/%lu (%.4f%%)\n", count, total, percentage);
        fflush(stdout);
        sleep(7);
        count = *((unsigned long *)worker_options->count);
    }
    if (worker_options->stopped) {
        double percentage = (double) count / total * 100;
        fprintf(stdout, "Stopped at %lu/%lu (%.4f%%)\n", count, total, percentage);
    }*/
    return 0;
}