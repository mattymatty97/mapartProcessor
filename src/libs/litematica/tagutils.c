#include "tagutils.h"
#include "nbt.h"

void set_tag_name(nbt_tag_t* tag, char* name) {
	nbt_set_tag_name(tag, name, strlen(name));
}

nbt_tag_t* create_child_compound_tag(char* name, nbt_tag_t* parent) {
	nbt_tag_t* tag = nbt_new_tag_compound();
	set_tag_name(tag, name);
	if (parent != NULL) {
		nbt_tag_compound_append(parent, tag);
		return NULL;
	}
	return tag;
}

nbt_tag_t* create_child_list_tag(char* name, nbt_tag_type_t type, nbt_tag_t* parent) {
	nbt_tag_t* tag = nbt_new_tag_list(type);
	set_tag_name(tag, name);
	if (parent != NULL) {
		nbt_tag_compound_append(parent, tag);
		return NULL;
	}
	return tag;
}

nbt_tag_t* create_child_int_tag(char* name, int val, nbt_tag_t* parent) {
	nbt_tag_t* tag = nbt_new_tag_int(val);
	set_tag_name(tag, name);
	if (parent != NULL) {
		nbt_tag_compound_append(parent, tag);
		return NULL;
	}
	return tag;
}

nbt_tag_t* create_child_long_tag(char* name, int64_t val, nbt_tag_t* parent) {
	nbt_tag_t* tag = nbt_new_tag_long(val);
	set_tag_name(tag, name);
	if (parent != NULL) {
		nbt_tag_compound_append(parent, tag);
		return NULL;
	}
	return tag;
}

nbt_tag_t* create_child_string_tag(char* name, char* val, nbt_tag_t* parent) {
	nbt_tag_t* tag = nbt_new_tag_string(val, strlen(val));
	set_tag_name(tag, name);
	if (parent != NULL) {
		nbt_tag_compound_append(parent, tag);
		return NULL;
	}
	return tag;
}

nbt_tag_t* create_child_long_array_tag(char* name, int64_t vals[], int length, nbt_tag_t* parent) {
	nbt_tag_t* tag = nbt_new_tag_long_array(vals, length);
	set_tag_name(tag, name);
	if (parent != NULL) {
		nbt_tag_compound_append(parent, tag);
		return NULL;
	}
	return tag;
}

void add_tag_to_compound_parent(nbt_tag_t* tag, nbt_tag_t* parent) {
	nbt_tag_compound_append(parent, tag);
}

void add_tag_to_list_parent(nbt_tag_t* tag, nbt_tag_t* parent) {
	nbt_tag_list_append(parent, tag);
}