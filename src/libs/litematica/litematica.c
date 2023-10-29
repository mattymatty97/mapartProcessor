#include <stdbool.h>
#include <time.h>
#include <math.h>

#include "litematica.h"
#include "libs/globaldefs.h"
#include "tagutils.h"
#include "libs/alloc/tracked.h"

// TODO: Note to self: need to add mushroom stem checks if they are next to each other (will be pain because of palette changes needed)

/// <summary>
/// Stores data for a given block
/// </summary>
typedef struct {
	uint8_t block_id; // The block id this block has
	int16_t x; // The x coord of this block
	int16_t y; // The y coord of this block
	int16_t z; // The z coord of this block
} block_pos_data;

/// <summary>
/// Compares 2 block_pos_data objects, sorting by y, then z, then x
/// </summary>
/// <returns>- 1 if v1 &lt; v2<para/>+1 if v1 > v2<para/>0 if v1 == v2</returns>
int block_pos_data_compare(const void* v1, const void* v2)
{
	const block_pos_data* p1 = (block_pos_data*)v1;
	const block_pos_data* p2 = (block_pos_data*)v2;
	if (p1->y < p2->y)
		return -1;
	else if (p1->y > p2->y)
		return +1;
	else if (p1->z < p2->z)
		return -1;
	else if (p1->z > p2->z)
		return +1;
	else if (p1->x < p2->x)
		return -1;
	else if (p1->x > p2->x)
		return +1;
	else
		return 0;
}

char** get_new_block_palette(mapart_palette* block_palette, image_uint_data* block_data, uint8_t** new_palette_id_map, int* new_palette_len) {
	// Create an array to store the string values of the new condensed block palette
	// The palette is padded in the front with air
	char** new_block_palette = t_calloc(block_palette->palette_size + 1, sizeof(char*));
	new_block_palette[0] = "minecraft:air";
	new_block_palette[1] = block_palette->support_block;
	*new_palette_len = 2;
	// Create an array to use as a map to relate original block_ids to new condensed block_ids to reduce palette size
	// Assumes the support_block id will be 255
	*new_palette_id_map = t_calloc(256, sizeof(uint8_t));
	(*new_palette_id_map)[UCHAR_MAX] = 1;

	unsigned int* iter = block_data->image_data;
	unsigned int cur_block_id;
	for (int i = 0; i < block_data->width * block_data->height; i++) {
		cur_block_id = *iter;
		if ((*new_palette_id_map)[cur_block_id] == 0) {
			// If the block_id has not been seen before, add the block name to the new palette and initialize the id in new_palette_block_ids
			new_block_palette[*new_palette_len] = block_palette->palette_block_ids[cur_block_id];
			(*new_palette_id_map)[cur_block_id] = *new_palette_len;
			(*new_palette_len)++;
		}
		iter += 3; // Add 3 because there are 3 channels in block_data->image_data
	}

	return new_block_palette;
}

/// <summary>
/// Checks if the y values in the block data need to be shifted up by 1 due to support blocks
/// </summary>
/// <param name="block_palette">the block palette being used for which block ids need support</param>
/// <param name="stats">the stats of the mapart</param>
/// <returns>True if there is any block at height 0 which requires support<para/>False otherwise</returns>
bool is_upwards_shift_needed(mapart_palette* block_palette, mapart_stats* stats) {
	unsigned int cur_block_count;
	for (int i = 0; i < block_palette->palette_size; i++) {
		cur_block_count = stats->layer_id_count[i]; // Assumes that the y0 layer is the first layer

		if (cur_block_count != 0 && block_palette->is_supported[i])
			return true;
	}
	return false;
}

block_pos_data* get_supported_block_data(mapart_palette* block_palette, image_uint_data* block_data, int upwards_shift, int* supported_block_data_len) {
	// Create an array to store all the new block data including the supporting blocks
	// Each block stores its block_id and its x, y, and z positions relative to the mapart origin
	// Size allocated to the array is double the size of incoming block_data to account for worst-case scenario of all blocks requiring support
	block_pos_data* supported_block_data = t_calloc((int64_t)block_data->width * block_data->height * 2, sizeof(block_pos_data));

	*supported_block_data_len = 0;
	unsigned int* in_iter = block_data->image_data;
	block_pos_data* out_iter = supported_block_data;

	unsigned int cur_block_id;
	unsigned int cur_height;
	for (int x = 0; x < block_data->width; x++) {
		for (int z = 0; z < block_data->height; z++) {
			cur_block_id = *in_iter;
			cur_height = *(in_iter + 1) + upwards_shift;

			// Add the block data to the new array
			out_iter->block_id = cur_block_id;
			out_iter->x = x;
			out_iter->y = cur_height;
			out_iter->z = z;

			(*supported_block_data_len)++;
			in_iter += 3; // Add 3 because there are 3 channels in block_data->image_data
			out_iter++;

			// Add support block underneath if required
			if (block_palette->is_supported[cur_block_id]) {
				out_iter->block_id = UCHAR_MAX; // Support block_id is always 255
				out_iter->x = x;
				out_iter->y = cur_height - 1;
				out_iter->z = z;

				(*supported_block_data_len)++;
				out_iter++;
			}
		}
	}

	return supported_block_data;
}

int64_t* get_bit_packed_block_data(block_pos_data* block_data, int block_data_len, mapart_stats* stats, int block_palette_len, uint8_t* new_block_palette_id_map, int* bit_packed_block_data_len) {
	int bits_per_block = log2(block_palette_len) + 1;
	*bit_packed_block_data_len = (int)ceil(stats->volume * bits_per_block / 64.0);
	int64_t* bit_packed_block_data = t_calloc(*bit_packed_block_data_len, sizeof(int64_t));

	int64_t* curr_bit_section = bit_packed_block_data; // The 64-bit section that is currently being written to
	int section_index = 0; // The index in the 64-bit section that we are at, from right to left
	int section_bits_left = 64; // The number of bits left to be written to the current section
	block_pos_data prev_block = { 0, 0, 0, 0 };

	uint64_t block_id, air_gap;
	for (int i = 0; i < block_data_len; i++) {
		block_id = new_block_palette_id_map[block_data[i].block_id];
		air_gap = (uint64_t)(block_data[i].x - prev_block.x)
				+ (uint64_t)(block_data[i].z - prev_block.z) * stats->x_length
				+ (uint64_t)(block_data[i].y - prev_block.y) * stats->x_length * stats->z_length
				- 1;

		if (air_gap > 0) {
			uint64_t air_bits = air_gap * bits_per_block;

			// If the air bits are smaller than the remaining bits in the current section
			if (section_bits_left > air_bits) {
				section_index += air_bits;
				section_bits_left -= air_bits;
			}
			// If the air bits completely fill up the current section
			else if (section_bits_left == air_bits) {
				curr_bit_section++;
				section_index = 0;
				section_bits_left = 64;
			}
			// If the air bits are greater than the remaining bits in the current section
			else {
				air_bits -= section_bits_left; // Remove the bits remaining for the current section
				curr_bit_section += air_bits >> 6; // Shift the current section forward
				section_index = air_bits - (air_bits >> 6); // Set the current section's index to the number of air bits remaining after the section shift
				section_bits_left = 64 - section_index;
			}
		}

		// Very similar logic as above
		if (section_bits_left > bits_per_block) {
			*curr_bit_section |= block_id << section_index;
			section_index += bits_per_block;
			section_bits_left -= bits_per_block;
		}
		else if (section_bits_left == bits_per_block) {
			*curr_bit_section |= block_id << section_index;
			curr_bit_section++;
			section_index = 0;
			section_bits_left = 64;
		}
		else {
			*curr_bit_section |= block_id << section_index;
			curr_bit_section++;
			*curr_bit_section |= block_id >> section_bits_left;
			section_index = 0;
			section_bits_left = 64;
		}
		
		prev_block = block_data[i];
	}

	return bit_packed_block_data;
}

void litematica_create(char* author, char* description, char* litematic_name, char* file_name, mapart_stats* stats, version_numbers version_info, mapart_palette* block_palette, image_uint_data* block_data) {
	// Buffer for string manipulation
	char buffer[1000] = { 0 };

	// Get the new palette to use for the blocks
	uint8_t* new_palette_id_map;
	int new_palette_len;
	char** new_block_palette = get_new_block_palette(block_palette, block_data, &new_palette_id_map, &new_palette_len);

	// Add the support blocks to the block data and reorganize it into a block_pos_data array
	int upwards_shift = is_upwards_shift_needed(block_palette, stats);
	int supported_block_data_len;
	block_pos_data* supported_block_data = get_supported_block_data(block_palette, block_data, upwards_shift, &supported_block_data_len);

	// Sort the new block data by y, then z, then x
	qsort(supported_block_data, supported_block_data_len, sizeof(block_pos_data), block_pos_data_compare);

	// Update the height of the mapart if necessary and calculate the total volume
	stats->y_length += upwards_shift;
	stats->volume = (uint64_t)stats->x_length * stats->y_length * stats->z_length;

	nbt_tag_t* tagTop = create_compound_tag("");
	{
		nbt_tag_t* tagMeta = create_compound_tag("Metadata");
		{
			nbt_tag_t* tagEncSize = create_compound_tag("EnclosingSize");
			{
				create_child_int_tag("x", stats->x_length, tagEncSize);
				create_child_int_tag("y", stats->y_length, tagEncSize);
				create_child_int_tag("z", stats->z_length, tagEncSize);
			}
			add_tag_to_compound_parent(tagEncSize, tagMeta);

			create_child_string_tag("Author", author, tagMeta);
			create_child_string_tag("Description", description, tagMeta);
			create_child_string_tag("Name", litematic_name, tagMeta);
			create_child_int_tag("RegionCount", 1, tagMeta);
			create_child_long_tag("TimeCreated", time(0), tagMeta); // Set the time created & modified to the current time
			create_child_long_tag("TimeModified", time(0), tagMeta);
			create_child_int_tag("TotalBlocks", supported_block_data_len, tagMeta);
			create_child_int_tag("TotalVolume", stats->volume, tagMeta);
		}
		add_tag_to_compound_parent(tagMeta, tagTop);

		nbt_tag_t* tagRegions = create_compound_tag("Regions");
		{
			nbt_tag_t* tagMain = create_compound_tag(litematic_name);
			{
				nbt_tag_t* tagPos = create_compound_tag("Position");
				{
					create_child_int_tag("x", 0, tagPos);
					create_child_int_tag("y", 0, tagPos);
					create_child_int_tag("z", 0, tagPos);
				}
				add_tag_to_compound_parent(tagPos, tagMain);

				nbt_tag_t* tagSize = create_compound_tag("Size");
				{
					create_child_int_tag("x", stats->x_length, tagSize);
					create_child_int_tag("y", stats->y_length, tagSize);
					create_child_int_tag("z", stats->z_length, tagSize);
				}
				add_tag_to_compound_parent(tagSize, tagMain);
				
				nbt_tag_t* tagPalette = create_list_tag("BlockStatePalette", NBT_TYPE_COMPOUND);
				{
					// Add all the blocks in the palette
					for (int i = 0; i < new_palette_len; i++) {
						nbt_tag_t* tagBlockInPalette = create_compound_tag("");
						{
							create_child_string_tag("Name", new_block_palette[i], tagBlockInPalette);
						}
						add_tag_to_list_parent(tagBlockInPalette, tagPalette);
					}
				}
				add_tag_to_compound_parent(tagPalette, tagMain);

				create_child_list_tag("Entities", NBT_TYPE_COMPOUND, tagMain);
				create_child_list_tag("PendingBlockTicks", NBT_TYPE_COMPOUND, tagMain);
				create_child_list_tag("PendingFluidTicks", NBT_TYPE_COMPOUND, tagMain);
				create_child_list_tag("TileEntities", NBT_TYPE_COMPOUND, tagMain);

				// Get the block data bit-packed into a long array and add it
				int bit_packed_block_data_len;
				int64_t* bit_packed_block_data = get_bit_packed_block_data(supported_block_data, supported_block_data_len, stats, new_palette_len, new_palette_id_map, &bit_packed_block_data_len);
				create_child_long_array_tag("BlockStates", bit_packed_block_data, bit_packed_block_data_len, tagMain);
				t_free(bit_packed_block_data);
			}
			add_tag_to_compound_parent(tagMain, tagRegions);
		}
		add_tag_to_compound_parent(tagRegions, tagTop);

		create_child_int_tag("MinecraftDataVersion", version_info.mc_data, tagTop);
		create_child_int_tag("Version", version_info.litematica, tagTop);
	}

	// Free the allocated memory
	t_free(new_block_palette);
	t_free(new_palette_id_map);
	t_free(supported_block_data);

	// Save the litematic
	sprintf(buffer, "%s.litematic", file_name);
	write_nbt_file(buffer, tagTop, NBT_WRITE_FLAG_USE_GZIP);
}