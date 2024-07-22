# Minecraft Map-Art Tool
## Description
A tool to convert any image into a minecraft map (or multiple maps)

### Requirements:
- OpenCl 2 compatible GPU ( a compatible CPU is also possible but highly discouraged )

### Outputs
- png of the image in the limited colorspace (in `.\images`)
- litematica for building divided in sub-regions each of a map in size (in `.\litematica`)

##### Example:
> mapartProcessor.exe -n "test" -i "./input.png" -p "./palette.json" -d "sierra" -h 32

### command-line-options
 - `-i`/`--image`  
Path to the image to be converted
 - `-n`/`--project-name`  
Name used to generate the output filename
 - `-p`/`--palette`  
Path to the block palette json file ([More details below](#palettejson-format))
 - `-d`/`--dithering`  
Name of the dithering algorithm to use for the conversion ([Options below](#dithering-algorithms))
 - `-r`/`--random`/`--random-seed`  
The text string used to initialize the random
 - `-h`/`--maximum-height`  
The maximum allowed height for a staircase (negative means unlimited, 0-1 means flat)
 - `-v`/`--verbose`  
More logging
 - `-0`/`--y0-fix`  
Add extra blocks to solve a minecraft bug that prevents blocks at y0 from showing up on maps

#### Required arguments
-n -i -p -d

#### Default values
- `random_seed`  
"seed string"
- `maximum_height`  
-1 (unlimited)

#### Dithering algorithms
1. none (no dithering applied each pixel is converted to its closest match)
2. floyd / floyd_steinberg 
3. jjnd (AKA. Jarvis, Judice, and Ninke Dithering)
4. stucki
5. atkinson
6. burkes
7. sierra
8. sierra2 (AKA. Two Row Sierra)
9. sierraL (AKA. Sierra Lite)

dithering matrices from [this article](https://tannerhelland.com/2012/12/28/dithering-eleven-algorithms-source-code.html)

#### palette.json format
- `minecraft_data_version`  
This is the data version number that the litematica file will store to determine what version of Minecraft the schematic is built for. If you do not know your data version number, manually create and save a litematic in the Minecraft version desired, then open an NBT viewer/editor and use the number associated with the "MinecraftDataVersion" tag
- `multipliers`  
This is a list of values used to get the various hues of each color. DO NOT MODIFY THESE
- `support_block_id`  
This is the block to be used underneath any block in the schematic that requires support
- `colors`  
This is a list of all map colors in vanilla Minecraft. DO NOT ADD, REMOVE, OR REORDER ITEMS IN THIS LIST UNLESS YOU KNOW WHAT YOU ARE DOING. For each color in the list there are the following attributes:
	- `id`  
The index of this color. DO NOT MODIFY THIS
	- `name`  
The name of this color. DO NOT MODIFY THIS
	- `block_id`  
The block to be used for this color. Make sure to only put in blocks that match the current map color or the resulting mapart will be incorrect. The blocks for each map color in vanilla can be found [here](https://minecraft.wiki/w/Map_item_format#Color_table)
	- `color`  
The RGB values of this color. DO NOT MODIFY THIS
	- `usable`  
Whether this color should be considered in the mapart generation
	- `needs_support`  
Whether the schematic will place a block of type "support_block_id" underneath this color's block if it gets used. This does not affect the resulting image
	- `is_liquid`  
Whether this color's block is a liquid

### Extra information:
- This tool will handle any images in `json` or `png` format.  
- In case of png with `alpha channel` the program will handle the pixels using black composite and only consider as transparent pixels with alpha smaller than `30%`.  
- All images will be converted to [Oklab colorspace](https://bottosson.github.io/posts/oklab) before attempting to find the matching palette color.  
- The program will account for `height limitations` and `fluid` colors.  
- To avoid `horizontal lines` form being visible once the staircases are forcefully dropped (because you reached the height limit), the program uses an additional `noise image` (generate from the --seed option) to slightly offset the drop height for the staircases.  
This is to spread around the drop points in order to generate a more natural image.  

### Author TIPS:

- If set too small, the `maximum_height` parameter will cause the process to slow down a lot (flat is an exception) and might yield weird/bad results.
  Suggested values are `>=15` with best results documented at values `>=32` (unlimited/flat will work as intended as they are special cases)
- If dark areas do not look accurate:
  * Try increasing the brightness of the source image (with a program like GIMP)
  * Try generating a mapart using only grayscale colors (only enable white/black/gray blocks in the palette)  
If the first resulting limited colorspace image is accurate that just means that the original image is using colors too dark for minecraft/the palette used.  
The second (grayscale) image should help identify the areas that are too far from the usable colors
