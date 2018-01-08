#define full_tile_size 16
#define half_tile_size 8

#define bg_tile_number_x 32
#define bg_tile_number_y 20

#define vp_tile_number_x 20
#define vp_tile_number_y 15

#define bg_count 2
#define bg_palette_count 2
#define bg_tileset_count 2
#define bg_color_count 63
#define bg_tileset_number 4096

#define sprt_count 32
#define sprite_palette_count 8
#define sprt_color_count 15

#define hlf_sprt_count 128
#define hlf_sprt_palette_count 16
#define hlf_sprt_color_count 7
#define hlf_sprt_tileset_number 4096

#define Mask_alpha	0x8000
#define Mask_red	0x7C00
#define Mask_green	0x03E0
#define Mask_blue	0x001F

typedef uint16_t color_16bit;

color_16bit null_color=0x0000;

typedef struct {
 color_16bit colors[1+bg_color_count];
} bg_palette;

bg_palette bg_palettes[bg_palette_count];

void initialize_bg_palettes()
{
 for(int ii=0;ii<bg_palette_count;ii++)
 {
  for(int jj=0;jj<(1+bg_color_count);jj++)
  {
   bg_palettes[ii].colors[jj]=null_color;
  }
 }
}

#define Mask_bg_unused_index	0xC0
#define Mask_bg_color_index	0x3F //one byte per pixel, using six bits

typedef struct {
 uint8_t pixel_color_index[full_tile_size*full_tile_size]; // 256 Bytes at 16 px/tile
} bg_tile;

typedef struct {
 bg_tile tile[bg_tileset_number]; // 1 MB at 4096 tiles per tileset
} bg_tileset;
bg_tileset bg_tiles[bg_tileset_count];//bg_tiles[0-1].tile[0-4095].
                                      //pixel_color_index[ii*y+x]

//missing sp definitions for the moment...

typedef struct {
color_16bit colors[1+hlf_sprt_color_count];
} hlf_sprt_palette;

hlf_sprt_palette hlf_sprt_palettes[hlf_sprt_palette_count];

void initialize_hlf_sprt_palettes()
{
 for(int ii=0;ii<hlf_sprt_palette_count;ii++)
 {
  for(int jj=0;jj<(1+hlf_sprt_color_count);jj++)
  {
   hlf_sprt_palettes[ii].colors[jj]=null_color;
  }
 }
}

#define Mask_hlf_sprt_index_0	0xF0
#define Mask_hlf_sprt_index_1	0x0F //four bits per pixel, two pixels per byte

typedef struct {       //there's only 32 elements for 64 pixels here so be 
 uint8_t two_pixel_color_index[half_tile_size*half_tile_size>>1];//careful!!!!!!
} hlf_sprt_tile;

typedef struct {
 hlf_sprt_tile tile[hlf_sprt_tileset_number]; //128 KB at 4096 tiles per tileset
} hlf_sprt_tileset;

hlf_sprt_tileset hlf_sprt_tiles;//hlf_sprt_tiles.tile[0-4095].
                        //two_pixel_color_index[0-31]& Mask_hlf_sprt_index_[0-1]
