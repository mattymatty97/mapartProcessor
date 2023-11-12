#include <stdbool.h>
#include <time.h>
#include <math.h>

#include "litematica.h"
#include "libs/globaldefs.h"
#include "tagutils.h"
#include "libs/alloc/tracked.h"

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

//TODO: Add function documentation
block_pos_data* get_all_block_data(mapart_palette* block_palette, image_uint_data* block_data, int upwards_shift, bool y0Fix, int* all_blocks_len, int** region_block_lens, int** region_sizes, int** region_positions, char*** region_names, int* region_count) {
    fprintf(stdout, "Getting all block data along with supporting blocks\n");
    fflush(stdout);

    // Buffer for string manipulation
    char buffer[1000] = { 0 };

    // Create an array to store all the new block data including the supporting blocks
    // Each block stores its block_id and its x, y, and z positions relative to its region's origin (Note: y is not relative if min_height is not 0)
    // Size allocated to the array is 20x the size of incoming block_data to account for worst-case scenario of all blocks requiring support and all blocks being 10-deep water
    block_pos_data* all_block_data = t_calloc((int64_t)block_data->width * block_data->height * 20, sizeof(block_pos_data));

    // Get the number of regions we'll be using
    int region_x_count = (int)(ceil(block_data->width / 128.0));
    int region_z_count = (int)(ceil((block_data->height - 1) / 128.0)); // Subtract 1 from height to ignore 'header' region
    *region_count = region_x_count * region_z_count + 1; // Add back 1 to region count for the 'header' region

    // Create arrays to store information about each region
    *region_block_lens = t_calloc(*region_count, sizeof(int));
    *region_sizes = t_calloc((*region_count)*3, sizeof(int));
    *region_positions = t_calloc((*region_count)*3, sizeof(int));
    *region_names = t_calloc(*region_count, sizeof(char*));

    block_pos_data* out_iter = all_block_data;
    unsigned int* in_data = block_data->image_data;

    int channels = block_data->channels;
    unsigned int cur_block_id;
    int cur_height;

    // Get 'header' region blocks
    int in_index = 0;
    int max_height = 0;
    int min_height = (int)in_data[1] + upwards_shift;
    for (int x = 0; x < block_data->width; x++) {
        cur_block_id = in_data[in_index];
        cur_height = (int)in_data[in_index + 1] + upwards_shift;

        out_iter->block_id = cur_block_id;
        out_iter->x = (short)x;
        out_iter->y = (short)cur_height;
        out_iter->z = 0;

        max_height = cur_height > max_height ? cur_height : max_height;
        min_height = cur_height < min_height ? cur_height : min_height;

        in_index += channels;
        out_iter++;
    }
    // Store the 'header' region's information
    *all_blocks_len = block_data->width;
    (*region_block_lens)[0] = block_data->width;
    (*region_sizes)[0] = block_data->width;
    (*region_sizes)[1] = max_height - min_height + 1;
    (*region_sizes)[2] = 1;
    (*region_positions)[0] = 0;
    (*region_positions)[1] = min_height;
    (*region_positions)[2] = -1;
    (*region_names)[0] = "Header";

    // Get the main map region blocks
    int region_index = 1;
    for (int region_index_z = 0; region_index_z < region_z_count; region_index_z++) {
        for (int region_index_x = 0; region_index_x < region_x_count; region_index_x++) {
            in_index = region_index_x * 128 * channels
                   + ((region_index_z * 128 + 1) * block_data->width) * channels;
            max_height = 0;
            min_height = (int)in_data[in_index + 1] + upwards_shift;
            // If the 128x128 region extends outside of bounds clamp it down
            int region_width = (region_index_x + 1) * 128 < block_data->width ? 128 : block_data->width - region_index_x * 128;
            int region_height = (region_index_z + 1) * 128 < block_data->height - 1 ? 128 : block_data->height - 1 - region_index_z * 128;
            for (int z = 0; z < region_height; z++) {
                for (int x = 0; x < region_width; x++) {
                    cur_block_id = in_data[in_index];
                    int y_max = (int)in_data[in_index + 2] + upwards_shift;
                    int y_min = (int)in_data[in_index + 1] + upwards_shift;
                    for (cur_height = y_max; cur_height >= y_min; cur_height--) {
                        // Add the block data to the new array
                        out_iter->block_id = cur_block_id;
                        out_iter->x = (short)x;
                        out_iter->y = (short)cur_height;
                        out_iter->z = (short)z;

                        max_height = cur_height > max_height ? cur_height : max_height;
                        min_height = cur_height < min_height ? cur_height : min_height;
                        (*all_blocks_len)++;
                        (*region_block_lens)[region_index]++;

                        out_iter++;
                    }
                    in_index += channels;

                    // If the block placed was at y0 and y0Fix is true
                    if (y_max == 0 && y0Fix) {
                        // Add a glass block above
                        out_iter->block_id = 0;
                        out_iter->x = (short)x;
                        out_iter->y = 1;
                        out_iter->z = (short)z;

                        max_height = 1 > max_height ? 1 : max_height;
                        (*all_blocks_len)++;
                        (*region_block_lens)[region_index]++;

                        out_iter++;
                    }
                    // Add support block underneath if required
                    if (cur_block_id < block_palette->palette_size && block_palette->is_supported[cur_block_id]) {
                        out_iter->block_id = UCHAR_MAX; // Support block_id is always 255
                        out_iter->x = (short)x;
                        out_iter->y = (short)cur_height;
                        out_iter->z = (short)z;

                        min_height = cur_height < min_height ? cur_height : min_height;
                        (*all_blocks_len)++;
                        (*region_block_lens)[region_index]++;

                        out_iter++;
                    }
                }
                // Shift over to the next line
                in_index += (block_data->width - region_width) * channels;
            }
            // Store the current region's information
            (*region_sizes)[region_index * 3] = region_width;
            (*region_sizes)[region_index * 3 + 1] = max_height - min_height + 1;
            (*region_sizes)[region_index * 3 + 2] = region_height;
            (*region_positions)[region_index * 3] = region_index_x * 128;
            (*region_positions)[region_index * 3 + 1] = min_height;
            (*region_positions)[region_index * 3 + 2] = region_index_z * 128;
            sprintf(buffer, "Map %d_%d", region_index_x + 1, region_index_z + 1);
            (*region_names)[region_index] = t_strdup(buffer);

            region_index++;
        }
    }

    return all_block_data;
}

//TODO: Add function documentation
char*** get_new_block_palettes(mapart_palette* main_block_palette, block_pos_data* all_block_data, const int* region_block_lens, int region_count, int** new_block_palette_lens) {
    fprintf(stdout, "Getting new block palettes to use\n");
    fflush(stdout);

    // Buffer for string manipulation
    char buffer[1000] = { 0 };

    const char* air = "minecraft:air";

    // Create an array to store the new palette arrays
    char*** new_block_palettes = t_calloc(region_count, sizeof(char**));
    *new_block_palette_lens = t_calloc(region_count, sizeof(int));

    int* palette_id_map = t_calloc(UCHAR_MAX + 1, sizeof(int));

    char*** curr_palette = new_block_palettes;
    block_pos_data* iter = all_block_data;
    for (int region_index = 0; region_index < region_count; region_index++) {
        new_block_palettes[region_index] = t_calloc(UCHAR_MAX + 1, sizeof(char*));
        (*curr_palette)[0] = strdup(air);
        (*new_block_palette_lens)[region_index] = 1;

        for (int i = 0; i < region_block_lens[region_index]; i++) {
            if (palette_id_map[iter->block_id] == 0) {
                // TODO: Add mushroom stem checks if they are next to each other and give them their own palette index
                // If the block_id has not been seen before, add the block name to the new palette and initialize the id in palette_id_map
                if (iter->block_id == UCHAR_MAX) {
                    (*curr_palette)[(*new_block_palette_lens)[region_index]] = strdup(main_block_palette->support_block);
                }
                else {
                    (*curr_palette)[(*new_block_palette_lens)[region_index]] = strdup(main_block_palette->palette_block_ids[iter->block_id]);
                }
                palette_id_map[iter->block_id] = (*new_block_palette_lens)[region_index];
                (*new_block_palette_lens)[region_index]++;
            }
            // Change the block id in all_block_data to the new id
            iter->block_id = palette_id_map[iter->block_id];

            iter++;
        }

        // Reset the palette_id_map for the next region
        for (int i = 0; i <= UCHAR_MAX; i++) {
            palette_id_map[i] = 0;
        }
        curr_palette++;
    }

    return new_block_palettes;
}

//TODO: Add function documentation
int64_t* get_bit_packed_block_data(block_pos_data* block_data, int block_data_len, int block_palette_len, const int* region_size, const int* region_position, int* bit_packed_block_data_len) {
    fprintf(stdout, "Getting bit-packed block data\n");
    fflush(stdout);

    int bits_per_block = (int)log2(block_palette_len - 1) + 1;
    uint64_t total_bits = region_size[0] * region_size[1] * region_size[2] * bits_per_block;
	*bit_packed_block_data_len = (int)(total_bits >> 6) + ((total_bits & 63) > 0);
	int64_t* bit_packed_block_data = t_calloc(*bit_packed_block_data_len, sizeof(int64_t));

	int64_t* curr_bit_section = bit_packed_block_data; // The 64-bit section that is currently being written to
	int section_index = 0; // The index in the 64-bit section that we are at, from right to left
	int section_bits_left = 64; // The number of bits left to be written to the current section
	block_pos_data prev_block = { 0, -1, (short)region_position[1], 0 }; // Set the starting y value to the region's y position to compensate for the note in get_all_block_data()

	uint64_t block_id, air_gap;
	for (int i = 0; i < block_data_len; i++) {
		block_id = block_data[i].block_id;
		air_gap = (int64_t)(block_data[i].x - prev_block.x)
				+ (int64_t)(block_data[i].z - prev_block.z) * region_size[0]
				+ (uint64_t)(block_data[i].y - prev_block.y) * region_size[0] * region_size[2]
				- 1;

		if (air_gap > 0) {
			uint64_t air_bits = air_gap * bits_per_block;

			// If the air bits are smaller than the remaining bits in the current section
			if (section_bits_left > air_bits) {
				section_index += (int)air_bits;
				section_bits_left -= (int)air_bits;
			}
			// If the air bits are greater than or equal to the remaining bits in the current section
			else {
				air_bits -= section_bits_left; // Remove the bits remaining for the current section
				curr_bit_section += (air_bits >> 6) + 1; // Shift the current section forward
				section_index = (int)air_bits & 63; // Set the current section's index to the number of air bits remaining after the section shift
				section_bits_left = 64 - section_index;
			}
		}

		// Very similar logic as above but for a single block instead of multiple
		if (section_bits_left > bits_per_block) {
			*curr_bit_section |= (int64_t)block_id << section_index;
            section_index += bits_per_block;
			section_bits_left -= bits_per_block;
		}
		else {
			*curr_bit_section |= (int64_t)block_id << section_index;
			curr_bit_section++;
			*curr_bit_section |= (int64_t)block_id >> section_bits_left;
			section_index = bits_per_block - section_bits_left;
			section_bits_left = 64 - section_index;
		}
		
		prev_block = block_data[i];
	}

	return bit_packed_block_data;
}

//TODO: Add function documentation
void litematica_create(char* author, char* description, char* litematic_name, char* file_name, mapart_stats* stats, version_numbers version_info, mapart_palette* block_palette, image_uint_data* block_data) {
    fprintf(stdout, "Creating litematica file from image\n");
    fflush(stdout);

    // Buffers for string manipulation
	char buffer[1000] = { 0 };
    char buffer2[1000] = { 0 };

	// Add the support blocks to the block data and reorganize it into a block_pos_data array
	int upwards_shift = is_upwards_shift_needed(block_palette, stats);
	int all_blocks_len;
    int region_count;
    int* region_block_lens;
    int* region_sizes;
    int* region_positions;
    char** region_names;
    block_pos_data* all_block_data = get_all_block_data(block_palette, block_data, upwards_shift, stats->y0Fix, &all_blocks_len, &region_block_lens, &region_sizes, &region_positions, &region_names, &region_count);

    // Sort the new block data by y, then z, then x for each region separately
    int all_blocks_offset = 0;
    for (int i = 0; i < region_count; i++) {
        qsort(&all_block_data[all_blocks_offset], region_block_lens[i], sizeof(block_pos_data), block_pos_data_compare);
        all_blocks_offset += region_block_lens[i];
    }

    int* new_block_palette_lens;
    char*** new_block_palettes = get_new_block_palettes(block_palette, all_block_data, region_block_lens, region_count, &new_block_palette_lens);

	// Update the height of the mapart if necessary and calculate the total volume
	stats->y_length += upwards_shift;
	stats->volume = (uint64_t)stats->x_length * stats->y_length * stats->z_length;

    fprintf(stdout, "Starting NBT file creation\n");
    fflush(stdout);

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
			create_child_int_tag("RegionCount", region_count, tagMeta);
			create_child_long_tag("TimeCreated", time(0), tagMeta); // Set the time created & modified to the current time
			create_child_long_tag("TimeModified", time(0), tagMeta);
			create_child_int_tag("TotalBlocks", all_blocks_len, tagMeta);
			create_child_int_tag("TotalVolume", (int)stats->volume, tagMeta);
		}
		add_tag_to_compound_parent(tagMeta, tagTop);

		nbt_tag_t* tagRegions = create_compound_tag("Regions");
		{
            all_blocks_offset = 0;
            for (int region_index = 0; region_index < region_count; region_index++) {
                fprintf(stdout, "Creating Region #%d\n", region_index);
                fflush(stdout);

                nbt_tag_t* tagMain = create_compound_tag(region_names[region_index]);
                {
                    nbt_tag_t* tagPos = create_compound_tag("Position");
                    {
                        create_child_int_tag("x", region_positions[region_index * 3 + 0], tagPos);
                        create_child_int_tag("y", region_positions[region_index * 3 + 1], tagPos);
                        create_child_int_tag("z", region_positions[region_index * 3 + 2], tagPos);
                    }
                    add_tag_to_compound_parent(tagPos, tagMain);

                    nbt_tag_t* tagSize = create_compound_tag("Size");
                    {
                        create_child_int_tag("x", region_sizes[region_index * 3 + 0], tagSize);
                        create_child_int_tag("y", region_sizes[region_index * 3 + 1], tagSize);
                        create_child_int_tag("z", region_sizes[region_index * 3 + 2], tagSize);
                    }
                    add_tag_to_compound_parent(tagSize, tagMain);

                    nbt_tag_t* tagPalette = create_list_tag("BlockStatePalette", NBT_TYPE_COMPOUND);
                    {
                        // Add all the blocks in the palette
                        for (int palette_idx = 0; palette_idx < new_block_palette_lens[region_index]; palette_idx++) {
                            nbt_tag_t* tagBlockInPalette = create_compound_tag("");
                            {
                                int char_idx = 0;
                                // Check if the block has any blockstates by looking for the '[' character
                                for (; new_block_palettes[region_index][palette_idx][char_idx] != '[' && new_block_palettes[region_index][palette_idx][char_idx] != '\0'; char_idx++) {
                                    buffer[char_idx] = new_block_palettes[region_index][palette_idx][char_idx];
                                }
                                buffer[char_idx] = '\0';

                                create_child_string_tag("Name", buffer, tagBlockInPalette);

                                // Get the blockstates if they exist
                                if (new_block_palettes[region_index][palette_idx][char_idx] == '[') {
                                    nbt_tag_t* tagProperties = create_compound_tag("Properties");
                                    {
                                        bool first_half = true;
                                        int str_idx = 0;
                                        // Search through the block states and add them
                                        // Blockstates should be comma-delimited and follow the format "propertyName=propertyValue"
                                        for (char_idx++; new_block_palettes[region_index][palette_idx][char_idx] != ']' && new_block_palettes[region_index][palette_idx][char_idx] != '\0'; char_idx++) {
                                            if (new_block_palettes[region_index][palette_idx][char_idx] == '=') {
                                                buffer[str_idx] = '\0';
                                                first_half = false;
                                                str_idx = 0;
                                            }
                                            else if (new_block_palettes[region_index][palette_idx][char_idx] == ',') {
                                                buffer2[str_idx] = '\0';
                                                first_half = true;
                                                str_idx = 0;
                                                create_child_string_tag(buffer, buffer2, tagProperties);
                                            }
                                            else if (new_block_palettes[region_index][palette_idx][char_idx] != ' ') {
                                                if (first_half) {
                                                    buffer[str_idx] = new_block_palettes[region_index][palette_idx][char_idx];
                                                }
                                                else {
                                                    buffer2[str_idx] = new_block_palettes[region_index][palette_idx][char_idx];
                                                }
                                                str_idx++;
                                            }
                                        }
                                        create_child_string_tag(buffer, buffer2, tagProperties);
                                    }
                                    add_tag_to_compound_parent(tagProperties, tagBlockInPalette);
                                }
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
                    int64_t* bit_packed_block_data = get_bit_packed_block_data(all_block_data + all_blocks_offset, region_block_lens[region_index], new_block_palette_lens[region_index], &(region_sizes[region_index * 3]), &(region_positions[region_index * 3]), &bit_packed_block_data_len);
                    create_child_long_array_tag("BlockStates", bit_packed_block_data, bit_packed_block_data_len, tagMain);
                    t_free(bit_packed_block_data);

                    all_blocks_offset += region_block_lens[region_index];
                }
                add_tag_to_compound_parent(tagMain, tagRegions);
            }
		}
		add_tag_to_compound_parent(tagRegions, tagTop);

		create_child_int_tag("MinecraftDataVersion", version_info.mc_data, tagTop);
		create_child_int_tag("Version", version_info.litematica, tagTop);
	}

	// Free the allocated memory
	/*t_free(new_block_palette);
	t_free(new_palette_id_map);
	t_free(supported_block_data);*/

    fprintf(stdout, "Saving litematica file\n");
    fflush(stdout);

	// Save the litematic
	sprintf(buffer, "%s.litematic", file_name);
    clock_t start = clock();
	write_nbt_file(buffer, tagTop, NBT_WRITE_FLAG_USE_GZIP);
    clock_t stop = clock();
    double delta = (double)(stop - start) / CLOCKS_PER_SEC;
    fprintf(stdout, "Saved in %.5lf s\n", delta);
    fflush(stdout);
}