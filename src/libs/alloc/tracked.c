#include <stdio.h>
#include <malloc.h>
#define TRACKER_MAX 2048
typedef struct  {
    void * ptr;
    size_t size;
} tracker_t;

static tracker_t tracker_l[TRACKER_MAX] = { 0 };
static int track_last = 0;

int t_compare(const void * o1, const void * o2){
    if ( ((tracker_t *)o1)->ptr > ((tracker_t *)o2)->ptr)
        return -1;
    else if ( ((tracker_t *)o1)->ptr < ((tracker_t *)o2)->ptr)
        return 1;
    return 0;
}

void reorder(){
    qsort(tracker_l, track_last, sizeof (tracker_t), t_compare);
}

void track(tracker_t tracker){
    tracker_l[track_last++] = tracker;
    reorder();
}

tracker_t *find(void * ptr){
    tracker_t tmp = {ptr};
    return bsearch(&tmp, tracker_l, track_last, sizeof (tracker_t), t_compare);
}

void* t_malloc(size_t size)
{
    if (track_last > TRACKER_MAX){
        return NULL;
    }
    void * ptr = malloc(size);
    tracker_t tmp = {ptr, size};
    track(tmp);
    return ptr;
}

void* t_calloc(size_t count, size_t size){
    if (track_last > TRACKER_MAX){
        return NULL;
    }
    void * ptr = calloc(count, size);
    tracker_t tmp = {ptr, count*size};
    track(tmp);
    return ptr;
}

void* t_realloc(void * old_ptr, size_t size){
    if (old_ptr == NULL){
        return t_malloc(size);
    }
    tracker_t * tracker = find(old_ptr);
    if (tracker) {
        void *ptr = realloc(old_ptr, size);
        tracker->ptr =ptr;
        tracker->size = size;
        reorder();
        return ptr;
    }
    return NULL;
}

void* t_recalloc(void * old_ptr, size_t count, size_t size){
    if (old_ptr == NULL){
        return t_calloc(count, size);
    }
    tracker_t * tracker = find(old_ptr);
    if (tracker) {
        size_t old_size = tracker->size;
        size_t new_size = count*size;
        void *ptr = realloc(old_ptr, new_size);
        if (old_size < new_size){
            for(int i = old_size; i < new_size; i++)
                ((char*)ptr)[i] = 0;
        }
        tracker->ptr = ptr;
        tracker->size = new_size;
        reorder();
        return ptr;
    }
    return NULL;
}

void t_free(void* ptr)
{
    tracker_t * tracker = find(ptr);
    if (tracker) {
        free(ptr);
        tracker->ptr =NULL;
        tracker->size = 0;
        reorder();
        track_last--;
    }

    if (tracker == NULL && ptr != NULL)
        printf("%p already freed\n", ptr);
}

int t_isfree(void* ptr)
{
    tracker_t * tracker = find(ptr);
    return tracker != NULL;
}