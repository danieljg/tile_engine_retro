//Tile-based graphics engine

//the engine uses 16 bit color in the format define by the following bitmasks:
#define Mask_alpha	0x8000
#define Mask_red	0x7C00
#define Mask_green	0x03E0
#define Mask_blue	0x001F
//for convenience, we define
typedef uint16_t color_16bit;
color_16bit null_color=0x0000;

//Full tiles are 16x16 in size
//half tiles are 8x8 in size
#define full_tile_size 16
#define half_tile_size 8

//The frame is built using a number of layers
//each layer has a size of 32x32 tiles
#define layer_number_x 32
#define layer_number_y 32

//The viewport size is an integer number of tiles
#define vp_tile_number_x 20
#define vp_tile_number_y 15
//The (initial) origin of the viewport may be displaced
#define vp_x_origin 0
#define vp_y_origin 2

typedef struct {
 uint16_t width;
 uint16_t height;
 uint16_t x_origin;
 uint16_t y_origin;
} vp;

vp viewport;

void initialize_viewport() {
 viewport.width		= full_tile_size*vp_tile_number_x;
 viewport.height 	= full_tile_size*vp_tile_number_y;
 viewport.x_origin	= vp_x_origin;
 viewport.y_origin	= vp_y_origin;
}

//There are two background layers
#define bg_layer_count 2

//The bg layers are built using a tilemap, where each bg tile can be rotated,
//flipped either horizontally or vertically and where one may choose between
// one of two palette sets (intended for each bg to have its own palette set)
#define bg_palette_set_count 2 // 1 bit
//Each tile can reference one of two tilesets
#define bg_tileset_count 2 // 1 bit
//Number of palettes per bg palette set
#define bg_palettes_per_set 4 // 2 bits
//Number of colors in each bg palette
#define bg_palette_color_count 16 // 4 bits
//Number of tiles in a tileset
#define bg_tileset_number 1024 // 10 bits

typedef struct {
 color_16bit colors[bg_palette_color_count];
} bg_palette;

typedef struct {
 bg_palette palettes[bg_palettes_per_set];
} bg_palette_set;

#define Mask_bg_tile_index_0	0xF0//four bits per pixel, all bits are used
#define Mask_bg_tile_index_1	0x0F//two pixels per byte

typedef struct {
 uint8_t two_pixel_color_index[full_tile_size*full_tile_size>>1]; // 128 Bytes at 16 px/tile
} bg_tile;

typedef struct {
 bg_tile tile[bg_tileset_number]; // 128 kB at 1024 tiles per tileset
} bg_tileset;

//background tilemap masks
#define Mask_bgtm_reserved	0x8000 //bit 16
#define Mask_bgtm_rotation	0x4000 //bit 15
#define Mask_bgtm_v_flip	0x2000 //bit 14
#define Mask_bgtm_h_flip	0x1000 //bit 13
#define Mask_bgtm_palette	0X0C00 //bits 11-12
#define Mask_bgtm_index		0x03FF //bits 1-10

typedef struct {
uint16_t tile_index[layer_number_x*layer_number_y]; //2Bytes*32*32=2kB
} bg_tilemap;

typedef struct {
  bg_palette_set palette_sets[bg_palette_set_count];
  bg_tileset tilesets[bg_tileset_count];
  bg_tilemap tilemaps[bg_layer_count];
  uint16_t offset_x[bg_layer_count];
  uint16_t offset_y[bg_layer_count];
} bg_struct;

bg_struct bg;

void initialize_bg()
{
  for(uint8_t ii=0; ii<bg_layer_count; ii++) {
    // inicializando offset de backgrounds
    bg.offset_x[ii] = 0;
    bg.offset_y[ii] = 0;
  }
}


void initialize_bg_palettes()
{
 for(uint8_t ii=0;ii<bg_palettes_per_set;ii++)
 {
  for(uint8_t jj=0;jj<bg_palette_color_count;jj++)
  {
   for(uint8_t kk=0;kk<bg_palette_set_count;kk++){
    bg.palette_sets[kk].palettes[ii].colors[jj]=null_color;
    bg.palette_sets[kk].palettes[ii].colors[jj]=null_color;
   }
  }
 }
}


// TO DO: Escribir funciones de inicialización restantes

#define full_sprt_count 32
#define full_sprt_palette_count 8
#define full_sprt_color_count 16
#define full_sprt_tileset_number 4096

typedef struct {
color_16bit colors[full_sprt_color_count];
} full_sprt_palette;

full_sprt_palette full_sprt_palettes[full_sprt_palette_count];
                //full_sprt_palettes[0-7].colors[0-15]

void initialize_full_sprt_palettes()
{
 for(uint8_t ii=0;ii<full_sprt_palette_count;ii++)
 {
  for(uint8_t jj=0;jj<full_sprt_color_count;jj++)
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

#define hlf_sprt_count 128
#define hlf_sprt_palette_count 16
#define hlf_sprt_color_count 7
#define hlf_sprt_tileset_number 4096

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
