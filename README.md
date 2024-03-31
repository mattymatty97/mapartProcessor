# Minecraft Map-Art Tool
## description
tool to convert any image into a minecraft map ( or multiple maps )

### outputs
- png of the image in the limited colorspace
- litematica for building divided in sub-regions each of a map in size

### command-line-options
 - -i/--image  
path to the image to be converted
 - -n/--project-name  
name used to generate the output filename
 - -p/--palette  
path to the block palette json file ( more details below )
 - -d/--dithering  
name of the dithering algorithm to use for the conversion
 - -r/--random/--random-seed  
the text string used to initialize the random
 - -h/--maximum-height  
the maximum allowed height for a staircase
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
0. none  
no dithering applied each pixel is converted to it's closest match
1. floyd / floyd_steinberg
2. jjnd  
   Jarvis, Judice, and Ninke Dithering
3. stucki
4. atkinson
5. burkes
6. sierra
7. sierra2  
Two Row Sierra
8. sierraL  
Sierra Lite

dithering matrices from [this article](https://tannerhelland.com/2012/12/28/dithering-eleven-algorithms-source-code.html)

