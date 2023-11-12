#include <stdbool.h>
#include "nbt.h"
#include "../globaldefs.h"

/// <summary>
/// Stores some stats pertaining to the mapart for the litematica.c code to use
/// </summary>
typedef struct {
	uint16_t x_length; // The length of the 3d mapart along the x axis
	uint16_t y_length; // The length of the 3d mapart along the y axis
	uint16_t z_length; // The length of the 3d mapart along the z axis
	uint64_t volume; // The total volume a box encompassing the 3d mapart would have
	unsigned int* layer_id_count; // 2d matrix containing the block count at a given layer with a given palette id
    bool y0Fix;
} mapart_stats;

/// <summary>
/// Stores the version numbers needed for the litematic file
/// </summary>
typedef struct {
	int mc_data; // The Minecraft data version the litematic should use
	int litematica; // The Litematica version the litematic should use
} version_numbers;

/// <summary>
/// 
/// </summary>
/// <param name="author"></param>
/// <param name="description"></param>
/// <param name="litematic_name"></param>
/// <param name="file_name"></param>
/// <param name="stats"></param>
/// <param name="version_info"></param>
/// <param name="block_palette"></param>
/// <param name="block_data"></param>
void litematica_create(char* author, char* description, char* litematic_name, char* file_name, mapart_stats* stats, version_numbers version_info, mapart_palette* block_palette, image_uint_data* block_data);