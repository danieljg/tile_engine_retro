#define full_tile_size 16
#define half_tile_size 8

#define bg_tile_number_x 32//layer_tile_number
#define bg_tile_number_y 32

#define vp_tile_number_x 20
#define vp_tile_number_y 15

#define bg_count 2 // bg_layer_count

#define bg_palette_count 2 // 1 bit
#define bg_tileset_count 2 // 1 bit
#define bg_color_count 63 // null * 63 = 64 colors (6 bits)
#define bg_tileset_number 1024 // 10 bits

#define full_sprt_count 32
#define full_sprt_palette_count 8
#define full_sprt_color_count 15
#define full_sprt_tileset_number 4096

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
 for(uint8_t ii=0;ii<bg_palette_count;ii++)
 {
  for(uint8_t jj=0;jj<(1+bg_color_count);jj++)
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

typedef struct {
color_16bit colors[1+full_sprt_color_count];
} full_sprt_palette;

full_sprt_palette full_sprt_palettes[full_sprt_palette_count];
                //full_sprt_palettes[0-7].colors[0-15]

void initialize_full_sprt_palettes()
{
 for(uint8_t ii=0;ii<full_sprt_palette_count;ii++)
 {
  for(uint8_t jj=0;jj<(1+full_sprt_color_count);jj++)
  {
   full_sprt_palettes[ii].colors[jj]=null_color;
  }
 }
}

#define Mask_full_sprt_index_0	0xF0//four bits per pixel, all bits are used
#define Mask_full_sprt_index_1	0x0F//two pixels per byte

typedef struct {
 uint8_t two_pixel_color_index[half_tile_size*half_tile_size>>1];
} full_sprt_tile;

typedef struct {
 full_sprt_tile tile[full_sprt_tileset_number]; //512KB at 4096 tiles per set
} full_sprite_tileset;

full_sprite_tileset full_sprite_tiles;//full_sprt_tiles.tile[0-4095].
                         //two_pixel_color_index[0-31]&Mask_hlf_sprt_index_[0-1]
typedef struct {
color_16bit colors[1+hlf_sprt_color_count];
} hlf_sprt_palette;

hlf_sprt_palette hlf_sprt_palettes[hlf_sprt_palette_count];
               //hlf_sprt_palettes[0-15].colors[0_7]

void initialize_hlf_sprt_palettes()
{
 for(uint8_t ii=0;ii<hlf_sprt_palette_count;ii++)
 {
  for(uint8_t jj=0;jj<(1+hlf_sprt_color_count);jj++)
  {
   hlf_sprt_palettes[ii].colors[jj]=null_color;
  }
 }
}

#define Mask_hlf_sprt_index_0	0xF0 //for simplicity, we reserve four bits per
#define Mask_hlf_sprt_index_1	0x0F //pixel (two pixels per byte), and the most
                                   //significant bit is unused at this time

typedef struct {       //there's only 32 elements for 64 pixels here so be
 uint8_t two_pixel_color_index[half_tile_size*half_tile_size>>1];//careful!!!!!!
} hlf_sprt_tile;

typedef struct {
 hlf_sprt_tile tile[hlf_sprt_tileset_number]; //128 KB at 4096 tiles per tileset
} hlf_sprt_tileset;

hlf_sprt_tileset hlf_sprt_tiles;//hlf_sprt_tiles.tile[0-4095].
                        //two_pixel_color_index[0-31]& Mask_hlf_sprt_index_[0-1]

typedef struct {
 uint16_t x_pos;
 uint16_t y_pos;
 uint8_t palette;
} hlf_sprt_ObjectAttributeTable;

hlf_sprt_ObjectAttributeTable hlf_sprt_attributes[hlf_sprt_count];
       //hlf_sprt_attributes[0-128].x_pos;
       //hlf_sprt_attributes[0-128].y_pos;
       //hlf_sprt_attributes[0-128].palette;
