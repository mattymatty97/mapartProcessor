#include "nbt.h"

void SaveToLitematica() {
	std::string Author = "Hunsinger";
	std::string Description = "";
	std::string NBTName = "Testing";

	nbt_tag_t* tagTop = CreateCompoundTag("");
	nbt_tag_t* tagMeta = CreateCompoundTag("Metadata");
	nbt_tag_t* tagEncSize = CreateCompoundTag("EnclosingSize");
	CreateIntTag("x", NBT_X, tagEncSize);
	CreateIntTag("y", NBT_Y, tagEncSize);
	CreateIntTag("z", NBT_Z + 1, tagEncSize);
	CloseTag(tagEncSize, tagMeta);
	CreateStringTag("Author", Author, tagMeta);
	CreateStringTag("Description", Description, tagMeta);
	CreateStringTag("Name", NBTName, tagMeta);
	CreateIntTag("RegionCount", 1, tagMeta);
	CreateLongTag("TimeCreated", time(0), tagMeta);
	CreateLongTag("TimeModified", time(0), tagMeta);
	CreateIntTag("TotalBlocks", BLOCK_ARRAY_SIZE, tagMeta);
	CreateIntTag("TotalVolume", BLOCK_ARRAY_SIZE, tagMeta);
	CloseTag(tagMeta, tagTop);
	nbt_tag_t* tagRegions = CreateCompoundTag("Regions");
	nbt_tag_t* tagMain = CreateCompoundTag(NBTName);
	nbt_tag_t* tagPos = CreateCompoundTag("Position");
	CreateIntTag("x", 0, tagPos);
	CreateIntTag("y", 0, tagPos);
	CreateIntTag("z", 0, tagPos);
	CloseTag(tagPos, tagMain);
	nbt_tag_t* tagSize = CreateCompoundTag("Size");
	CreateIntTag("x", NBT_X, tagSize);
	CreateIntTag("y", NBT_Y, tagSize);
	CreateIntTag("z", NBT_Z + 1, tagSize);
	CloseTag(tagSize, tagMain);
	nbt_tag_t* tagPalette = CreateListTag("BlockStatePalette", nbt_tag_type_t::NBT_TYPE_COMPOUND);
	nbt_tag_t* tagPalAir = CreateCompoundTag("");
	CreateStringTag("Name", "minecraft:air", tagPalAir);
	CloseTag(tagPalAir, tagPalette, false);
	for (int i = 0; i < TOTAL_COLORS + 1; i++) {
		nbt_tag_t* tagPal = CreateCompoundTag("");
		CreateStringTag("Name", "minecraft:" + BlockTypes[i], tagPal);
		CloseTag(tagPal, tagPalette, false);
	}
	CloseTag(tagPalette, tagMain);
	CreateListTag("Entities", nbt_tag_type_t::NBT_TYPE_COMPOUND, tagMain);
	CreateListTag("PendingBlockTicks", nbt_tag_type_t::NBT_TYPE_COMPOUND, tagMain);
	CreateListTag("PendingFluidTicks", nbt_tag_type_t::NBT_TYPE_COMPOUND, tagMain);
	CreateListTag("TileEntities", nbt_tag_type_t::NBT_TYPE_COMPOUND, tagMain);
}