#include "litematica.h"
#include "libs/globaldefs.h"
#include "tagutils.h";
#include "libs/alloc/tracked.h"

void litematica_create(char* author, char* description, char* litematic_name, char* file_name, map_size size, version_indeces version_info, mapart_palette* block_palette, char* support_block, image_uint_data* block_data) {
	char buffer[1000] = {};

	nbt_tag_t* tagTop = create_compound_tag("");
	{
		nbt_tag_t* tagMeta = create_compound_tag("Metadata");
		{
			nbt_tag_t* tagEncSize = create_child_compound_tag("EnclosingSize", NULL);
			{
				create_int_tag("x", size.x_length, tagEncSize);
				create_int_tag("y", size.y_length, tagEncSize);
				create_int_tag("z", size.z_length + 1, tagEncSize);
			}
			close_compound_tag(tagEncSize, tagMeta);
			create_string_tag("Author", author, tagMeta);
			create_string_tag("Description", description, tagMeta);
			create_string_tag("Name", litematic_name, tagMeta);
			create_int_tag("RegionCount", 1, tagMeta);
			create_long_tag("TimeCreated", time(0), tagMeta);
			create_long_tag("TimeModified", time(0), tagMeta);
			create_int_tag("TotalBlocks", BLOCK_ARRAY_SIZE, tagMeta);
			create_int_tag("TotalVolume", BLOCK_ARRAY_SIZE, tagMeta);
		}
		close_compound_tag(tagMeta, tagTop);
		nbt_tag_t* tagRegions = create_child_compound_tag("Regions", NULL);
		{
			nbt_tag_t* tagMain = create_child_compound_tag(litematic_name, NULL);
			{
				nbt_tag_t* tagPos = create_child_compound_tag("Position", NULL);
				{
					create_int_tag("x", 0, tagPos);
					create_int_tag("y", 0, tagPos);
					create_int_tag("z", 0, tagPos);
				}
				close_compound_tag(tagPos, tagMain);
				nbt_tag_t* tagSize = create_child_compound_tag("Size", NULL);
				{
					create_int_tag("x", size.x_length, tagSize);
					create_int_tag("y", size.y_length, tagSize);
					create_int_tag("z", size.z_length + 1, tagSize);
				}
				close_compound_tag(tagSize, tagMain);
				nbt_tag_t* tagPalette = create_list_tag("BlockStatePalette", NBT_TYPE_COMPOUND, NULL);
				{
					nbt_tag_t* tagPalAir = create_child_compound_tag("", NULL);
					{
						create_string_tag("Name", "minecraft:air", tagPalAir);
					}
					close_list_tag(tagPalAir, tagPalette);
					for (int i = 0; i < block_palette_size; i++) {
						nbt_tag_t* tagPal = create_child_compound_tag("", NULL);
						{
							create_string_tag("Name", block_palette_used[i], tagPal);
						}
						close_list_tag(tagPal, tagPalette);
					}
				}
				close_compound_tag(tagPalette, tagMain);
				create_list_tag("Entities", NBT_TYPE_COMPOUND, tagMain);
				create_list_tag("PendingBlockTicks", NBT_TYPE_COMPOUND, tagMain);
				create_list_tag("PendingFluidTicks", NBT_TYPE_COMPOUND, tagMain);
				create_list_tag("TileEntities", NBT_TYPE_COMPOUND, tagMain);

				int64_t* longArr = t_malloc(sizeof(int64_t) * BLOCK_ARRAY_SIZE);
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

				create_long_array_tag("BlockStates", longArr, BLOCK_ARRAY_SIZE, tagMain);

				t_free(longArr);
			}
			close_compound_tag(tagMain, tagRegions);
		}
		close_compound_tag(tagRegions, tagTop);

		create_int_tag("MinecraftDataVersion", version_info.mc_data, tagTop);
		create_int_tag("Version", version_info.litematica, tagTop);
	}

	sprintf(buffer, "%s.litematic", file_name);

	write_nbt_file(buffer, tagTop, NBT_WRITE_FLAG_USE_GZIP);
}

int temp(mapart_palette* block_palette, unsigned int* block_palette_counts) {

}