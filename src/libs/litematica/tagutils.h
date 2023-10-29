#ifndef TAG_UTILS
#define TAG_UTILS

#include "nbt.h"

/// <summary>
/// Creates a compound tag and adds it to the parent compound tag provided
/// </summary>
/// <param name="name">The string name for this tag</param>
/// <param name="parent">The compound tag parent to add this tag to</param>
/// <returns>The tag created</returns>
nbt_tag_t* create_child_compound_tag(char* name, nbt_tag_t* parent);
#define create_compound_tag(filename) create_child_compound_tag(filename, NULL)

/// <summary>
/// Creates a list tag and adds it to the parent compound tag provided
/// </summary>
/// <param name="name">The string name for this tag</param>
/// <param name="type">The tag type for this list</param>
/// <param name="parent">The compound tag parent to add this tag to</param>
/// <returns>The tag created</returns>
nbt_tag_t* create_child_list_tag(char* name, nbt_tag_type_t type, nbt_tag_t* parent);
#define create_list_tag(filename, type) create_child_list_tag(filename, type, NULL)

/// <summary>
/// Creates an int tag and adds it to the parent compound tag provided
/// </summary>
/// <param name="name">The string name for this tag</param>
/// <param name="val">The value this tag should hold</param>
/// <param name="parent">The compound tag parent to add this tag to</param>
/// <returns>The tag created</returns>
nbt_tag_t* create_child_int_tag(char* name, int val, nbt_tag_t* parent);
#define create_int_tag(filename, val) create_child_int_tag(filename, val, NULL)

/// <summary>
/// Creates a long tag and adds it to the parent compound tag provided
/// </summary>
/// <param name="name">The string name for this tag</param>
/// <param name="val">The value this tag should hold</param>
/// <param name="parent">The compound tag parent to add this tag to</param>
/// <returns>The tag created</returns>
nbt_tag_t* create_child_long_tag(char* name, int64_t val, nbt_tag_t* parent);
#define create_long_tag(filename, val) create_child_long_tag(filename, val, NULL)

/// <summary>
/// Creates a string tag and adds it to the parent compound tag provided
/// </summary>
/// <param name="name">The string name for this tag</param>
/// <param name="val">The value this tag should hold</param>
/// <param name="parent">The compound tag parent to add this tag to</param>
/// <returns>The tag created</returns>
nbt_tag_t* create_child_string_tag(char* name, char* val, nbt_tag_t* parent);
#define create_string_tag(filename, val) create_child_string_tag(filename, val, NULL)

/// <summary>
/// Creates a long array tag and adds it to the parent compound tag provided
/// </summary>
/// <param name="name">The string name for this tag</param>
/// <param name="vals">The values this tag array should hold</param>
/// <param name="length">The array length for this tag array</param>
/// <param name="parent">The compound tag parent to add this tag to</param>
/// <returns>The tag created</returns>
nbt_tag_t* create_child_long_array_tag(char* name, int64_t vals[], int length, nbt_tag_t* parent);
#define create_long_array_tag(filename, vals, length) create_child_long_array_tag(filename, vals, length, NULL)

/// <summary>
/// Adds the given tag to the parent compound tag provided
/// </summary>
/// <param name="tag">The tag to add</param>
/// <param name="parent">The compound tag parent to add to</param>
void add_tag_to_compound_parent(nbt_tag_t* tag, nbt_tag_t* parent);

/// <summary>
/// Adds the given tag to the parent list tag provided
/// </summary>
/// <param name="tag">The tag to add</param>
/// <param name="parent">The list tag parent to add to</param>
void add_tag_to_list_parent(nbt_tag_t* tag, nbt_tag_t* parent);

/// <summary>
/// Writes the given tag and subtags to an nbt file
/// </summary>
/// <param name="filename">The name of the file to write to</param>
/// <param name="tag">The tag (and any subtags) to be written</param>
/// <param name="flags">Any write flags to be specified for the filewriter</param>
void write_nbt_file(const char* filename, nbt_tag_t* tag, int flags);

#endif