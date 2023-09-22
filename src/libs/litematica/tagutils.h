#ifndef TAG_UTILS
#define TAG_UTILS

#include "nbt.h"

void set_tag_name(nbt_tag_t* tag, char* name);

nbt_tag_t* create_child_compound_tag(char* name, nbt_tag_t* append);
#define create_compound_tag(name) create_child_compound_tag(name, NULL)

nbt_tag_t* create_list_tag(char* name, nbt_tag_type_t type, nbt_tag_t* append);

nbt_tag_t* create_int_tag(char* name, int val, nbt_tag_t* append);

nbt_tag_t* create_long_tag(char* name, int64_t val, nbt_tag_t* append);

nbt_tag_t* create_string_tag(char* name, char* val, nbt_tag_t* append);

nbt_tag_t* create_long_array_tag(char* name, int64_t vals[], int length, nbt_tag_t* append);

void close_compound_tag(nbt_tag_t* tag, nbt_tag_t* append);

void close_list_tag(nbt_tag_t* tag, nbt_tag_t* append);

#endif