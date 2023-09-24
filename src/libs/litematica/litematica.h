#include "nbt.h"


typedef struct {
	int x_length;
	int y_length;
	int z_length;
	unsigned int* layer_id_count;
} mapart_stats;

typedef struct {
	int mc_data;
	int litematica;
} version_indeces;

typedef struct {
	uint8_t block_id;
	uint16_t x;
	uint16_t y;
	uint16_t z;
} block_pos_data;