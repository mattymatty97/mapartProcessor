#ifndef TAG_UTILS
#define TAG_UTILS

#include "nbt.h"

nbt_tag_t* create_child_compound_tag(char* name, nbt_tag_t* parent);
#define create_compound_tag(name) create_child_compound_tag(name, NULL)

nbt_tag_t* create_child_list_tag(char* name, nbt_tag_type_t type, nbt_tag_t* parent);
#define create_list_tag(name, type) create_child_list_tag(name, type, NULL);

nbt_tag_t* create_child_int_tag(char* name, int val, nbt_tag_t* parent);
#define create_int_tag(name, val) create_child_int_tag(name, val, NULL);

nbt_tag_t* create_child_long_tag(char* name, int64_t val, nbt_tag_t* parent);
#define create_long_tag(name, val) create_child_long_tag(name, val, NULL);

nbt_tag_t* create_child_string_tag(char* name, char* val, nbt_tag_t* parent);
#define create_string_tag(name, val) create_child_string_tag(name, val, NULL);

nbt_tag_t* create_child_long_array_tag(char* name, int64_t vals[], int length, nbt_tag_t* parent);
#define create_long_array_tag(name, vals, length) create_child_long_array_tag(name, vals, length, NULL)

void add_tag_to_compound_parent(nbt_tag_t* tag, nbt_tag_t* parent);

void add_tag_to_list_parent(nbt_tag_t* tag, nbt_tag_t* parent);

#endif