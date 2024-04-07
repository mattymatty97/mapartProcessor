# Minecraft Map-Art Tool
## description
tool to convert any image into a minecraft map (or multiple maps)

### Requrements:
- OpenCl 2 compatible GPU ( a compatible CPU is also possible but highly discouraged )

### outputs
- png of the image in the limited colorspace (in `.\images`)
- litematica for building divided in sub-regions each of a map in size (in `.\litematica`)

##### Example:
> mapartProcessor.exe -n "test" -i "./input.png" -p "./palette.json" -d "sierra" -h 32

### command-line-options
 - -i/--image  
path to the image to be converted
 - -n/--project-name  
name used to generate the output filename
 - -p/--palette  
path to the block palette json file (more details below)
 - -d/--dithering  
name of the dithering algorithm to use for the conversion
 - -r/--random/--random-seed  
the text string used to initialize the random
 - -h/--maximum-height  
the maximum allowed height for a staircase (negative means unlimited, 0-1 means flat)
 - -v/--verbose  
more logging
 - -0/--y0-fix
add extra blcoks to solve a minecraft bug that prevents blocks at y0 from showing up on maps

#### required arguments
-n -i -p -d

#### default values
- random seed  
> "seed string"
- maximum height  
> unlimited

#### dithering algorithms
1. none (no dithering applied each pixel is converted to it's closest match)
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
> TODO: add palette description

### extra information:
This tool will handle any images in `json` or `png` format.  
In case of png with `alpha channel` the program will handle the pixels using black composite and only consider as transparent pixels with alpha smaller than `30%`.  
All images will be converted in `okLab colorspace` before attempting to find the matching palette color.  
The program will account for `height limitations` and `fluid` colors.  
To avoid `horizontal lines` form being visible once the staircases are forcefully dropped (because you reached the height limit),
the program uses an additional `noise image` (generate from the --seed option) to slightly offset the drop height for the staircases.  
This is to spread around the drop points in order to generate a more natural image.  

### Author TIP:

- the `Maximum Height` parameter will cause the process to slow down a lot in case of too small values (flat is an exception) and might yield wierd/bad results  
suggested values are `>=15` where the best results have been tested with `>=32` (unlimited/flat will work as intended as they are special cases)
- if dark areas look not accurate:
  * try increasing the brightness of the source image (with a program like gimp)
  * try generating a mapart only using grayscale colors (only enable white/black/gray blocks in the palette)
  
  if the first resulting limited colorspace image is accurate that just means that the original image is using colors too dark for minecraft/the palette used.  
  the second (grayscale) image should help identify the areas that are too far from the usable colors
