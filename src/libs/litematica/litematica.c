#include <stdbool.h>
#include <time.h>

#include "litematica.h"
#include "libs/globaldefs.h"
#include "tagutils.h"
#include "libs/alloc/tracked.h"

/// <summary>
/// Compares 2 block_pos_data objects, sorting by y, then x, then z
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
		iter += 2; // Add 2 because there are 2 channels in block_data->image_data
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
			in_iter += 2; // Add 2 because there are 2 channels in block_data->image_data
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
}

void litematica_create(char* author, char* description, char* litematic_name, char* file_name, mapart_stats* stats, version_indeces version_info, mapart_palette* block_palette, image_uint_data* block_data) {
	char buffer[1000] = { 0 };

	// Get the new palette to use for the blocks
	uint8_t* new_palette_id_map;
	int new_palette_len;
	char** new_block_palette = get_new_block_palette(block_palette, block_data, &new_palette_id_map, &new_palette_len);

	int upwards_shift = is_upwards_shift_needed(block_palette, stats);
	int supported_block_data_len;
	block_pos_data* supported_block_data = get_supported_block_data(block_palette, block_data, upwards_shift, &supported_block_data_len);

	// Sort the new block data by y, then x, then z
	qsort(supported_block_data, supported_block_data_len, sizeof(block_pos_data), block_pos_data_compare);

	stats->y_length += upwards_shift;
	uint32_t mapart_volume = (uint32_t)stats->x_length * stats->y_length * stats->z_length;

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
			create_child_long_tag("TimeCreated", time(0), tagMeta);
			create_child_long_tag("TimeModified", time(0), tagMeta);
			create_child_int_tag("TotalBlocks", supported_block_data_len, tagMeta);
			create_child_int_tag("TotalVolume", mapart_volume, tagMeta);
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

				/*int64_t* longArr = t_malloc(sizeof(int64_t) * BLOCK_ARRAY_SIZE);
				int64_t* Bits = longArr;
				*Bits = 0;
				char bitIndex = 0;
				char longBitIndex = 0;

				int y = 0;
				while (y != NBT_Y && Layers[y].size() == 0) {
					y++;
				}
				short Prev[3] = { -1, 0, 0 };

				for (y; y != NBT_Y; y++) {
					for (std::vector<std::array<short, 3>>::iterator it = Layers[y].begin(); it != Layers[y].end(); it++) {
						int limit = it->at(0) - Prev[0] - 1 + (it->at(1) - Prev[1]) * NBT_X + (y - Prev[2]) * NBT_XZ;

						if (limit >= 32 - bitIndex) {
							limit -= 32 - bitIndex;
							Bits += 1 + (bitIndex < 22) + (bitIndex < 11);
							bitIndex = 0;
							longBitIndex = 0;
						}

						int shift = limit >> 5;
						Bits += 3 * shift;
						limit -= shift << 5;

						if (limit > (unsigned char)(21 - bitIndex)) {
							limit -= 22 - bitIndex;
							Bits += 1 + (bitIndex < 11);
							bitIndex = 22;
							longBitIndex = 4;
						}
						else if (limit > (unsigned char)(10 - bitIndex)) {
							limit -= 11 - bitIndex;
							Bits++;
							bitIndex = 11;
							longBitIndex = 2;
						}
						bitIndex += limit;
						longBitIndex += blockBits * limit;


						*Bits = *Bits | ((((int64_t)it->at(2) >> 2) + 1) << longBitIndex);
						bitIndex++;
						longBitIndex += blockBits;

						switch (bitIndex) {
						case 11:
							Bits++;
							*Bits = ((it->at(2) >> 2) + 1) >> 4;
							longBitIndex = 2;
							break;
						case 22:
							Bits++;
							*Bits = ((it->at(2) >> 2) + 1) >> 2;
							longBitIndex = 4;
							break;
						case 32:
							Bits++;
							*Bits = 0;
							bitIndex = 0;
							longBitIndex = 0;
							break;
						default:
							break;
						}

						Prev[0] = it->at(0);
						Prev[1] = it->at(1);
						Prev[2] = y;
					}
				}

				create_child_long_array_tag("BlockStates", longArr, mapart_volume, tagMain);

				t_free(longArr);*/
			}
			add_tag_to_compound_parent(tagMain, tagRegions);
		}
		add_tag_to_compound_parent(tagRegions, tagTop);

		create_child_int_tag("MinecraftDataVersion", version_info.mc_data, tagTop);
		create_child_int_tag("Version", version_info.litematica, tagTop);
	}

	sprintf(buffer, "%s.litematic", file_name);

	//write_nbt_file(buffer, tagTop, NBT_WRITE_FLAG_USE_GZIP);
}