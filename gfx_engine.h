//Tile-based graphics engine — public interface.
//All engine state lives in a gfx_context passed explicitly; the engine
//links as a plain library (gfx_engine.c) with no globals of its own.
#ifndef GFX_ENGINE_H
#define GFX_ENGINE_H

#include <stdint.h>
#include <stdio.h>

//the engine uses 16 bit color in the format defined by these bitmasks;
//a set alpha bit marks a color as semitransparent (50/50 blend)
#define Mask_alpha	0x8000
#define Mask_red	0x7C00
#define Mask_green	0x03E0
#define Mask_blue	0x001F
typedef uint16_t color_16bit;
#define null_color ((color_16bit)0x0000)

//Full tiles are 16x16 in size, half tiles are 8x8
#define full_tile_size 16
#define half_tile_size 8

//each layer has a size of 32x32 tiles
#define layer_tile_number_x 32
#define layer_tile_number_y 32

//The viewport size is an integer number of tiles
#if defined(_3DS)
#define vp_tile_number_x 25
#else
#define vp_tile_number_x 20
#endif
#define vp_tile_number_y 15
#define vp_x_origin 0
#define vp_y_origin 0

typedef struct {
 uint32_t width;
 uint32_t height;
 uint32_t x_origin;
 uint32_t y_origin;
} vp;

//There are five background layers, composed back to front:
//bg[4] (base) then overlays bg[3], bg[2], bg[1], bg[0]
#define bg_layer_count 5

#define bg_palettes_per_layer 4 // 2 bits
#define bg_palette_color_count 16 // 4 bits
#define bg_tileset_number 1024 // 10 bits

typedef struct {
 color_16bit color[bg_palette_color_count];
} bg_palette;

typedef struct {
  uint32_t eight_pixel_color_index[full_tile_size*full_tile_size>>3];
} bg_tile;

//background tilemap masks
#define Mask_bgtm_disable	0x8000 //bit 16
#define Mask_bgtm_v_flip	0x4000 //bit 15 (reserved, not rendered yet)
#define Mask_bgtm_h_flip	0x2000 //bit 14 (reserved, not rendered yet)
#define Mask_bgtm_reserved	0x1000 //bit 13
#define Mask_bgtm_palette	0X0C00 //bits 11-12
#define Mask_bgtm_index		0x03FF //bits 1-10

typedef struct {
  bg_palette palette[bg_palettes_per_layer];
  bg_tile tile[bg_tileset_number];
  uint16_t tilemap[layer_tile_number_x*layer_tile_number_y];
  uint32_t offset_x[full_tile_size*vp_tile_number_y];//per-scanline offsets
  uint32_t offset_y[full_tile_size*vp_tile_number_y];
} bg_struct;

//Full sprites: 16x16, for characters; half sprites: 8x8, for bullets/HUD
//NOTE: game2.h animation words store sprite ids in 5 bits (<=31); keep
//animation-word-driven sprites in low slots (they are, by spawn order)
#define fsp_palette_number 8
#define fsp_count 128
#define fsp_palette_color_count 16
#define fsp_tileset_number 1024

typedef struct {
color_16bit color[fsp_palette_color_count];
} fsp_palette;

typedef struct {
 uint32_t eight_pixel_color_index[full_tile_size*full_tile_size>>3];
} fsp_tile;

//Full sprite Object Attribute Memory bitmasks
#define Mask_fsp_oam_in_use   0x8000 //signals if slot is occupied
#define Mask_fsp_oam_enable   0x4000 //enable rendering of sprite
#define Mask_fsp_oam_effects  0x2000 //enable rendering of sprite effects
#define Mask_fsp_oam_palette  0x1C00 //select among 8 palettes
#define Mask_fsp_oam_index    0x03FF //10 bits for tileset index
//OAM2 bitmasks
#define Mask_fsp_oam2_v_flip   0x8000 //flip the tile vertically
#define Mask_fsp_oam2_h_flip   0x4000 //flip the tile horizontally
#define Mask_fsp_oam2_rotation 0x2000 //enable 90 degree clockwise rotation
#define Mask_fsp_oam2_double   0x1000 //double size flag
#define Mask_fsp_oam2_y_pos    0x0FFF //oversampled by 3 bits
//OAM3 bitmasks
#define Mask_fsp_oam3_reserved 0x8000 //not in use
#define Mask_fsp_oam3_priority 0x7000 //3-bit render priority (7 = topmost)
#define Mask_fsp_oam3_x_pos    0x0FFF //oversampled by 3 bits

typedef struct {
 fsp_palette palette[fsp_palette_number];
 fsp_tile tile[fsp_tileset_number]; //128 kB with 1024 tiles in the set
 fsp_tile tile_h[fsp_tileset_number]; //pre-mirrored (OAM h-flip)
 fsp_tile tile_r[fsp_tileset_number]; //pre-rotated 90 degrees CW
 fsp_tile tile_rh[fsp_tileset_number];//rotated 90 CW, then mirrored
 uint16_t oam[fsp_count];
 uint16_t oam2[fsp_count];
 uint16_t oam3[fsp_count];
 uint16_t free_stack[fsp_count];//free OAM slots; top of stack pops first
 uint16_t free_count;
 uint32_t offset_x;//TODO:Combine offsets into one 32 bit variable
 uint32_t offset_y;
} fsp_struct;

//half sprites are 4bpp like everything else: 8 palettes of 16 colors
#define hsp_palette_number 8
#define hsp_count 128
#define hsp_palette_color_count 16
#define hsp_tileset_number 1024

typedef struct {
color_16bit color[hsp_palette_color_count];
} hsp_palette;

typedef struct {
uint32_t eight_pixel_color_index[half_tile_size*half_tile_size>>3];
} hsp_tile;

//half sprite OAM bitmasks (same layout as full sprites)
#define Mask_hsp_oam_in_use    0x8000
#define Mask_hsp_oam_enable    0x4000
#define Mask_hsp_oam_effects   0x2000
#define Mask_hsp_oam_palette   0x1C00
#define Mask_hsp_oam_index     0x03FF
#define Mask_hsp_oam2_v_flip   0x8000
#define Mask_hsp_oam2_h_flip   0x4000
#define Mask_hsp_oam2_rotation 0x2000
#define Mask_hsp_oam2_double   0x1000
#define Mask_hsp_oam2_y_pos    0x0FFF
#define Mask_hsp_oam3_reserved 0x8000
#define Mask_hsp_oam3_priority 0x7000 //3-bit render priority (7 = topmost)
#define Mask_hsp_oam3_x_pos    0x0FFF

typedef struct {
 hsp_palette palette[hsp_palette_number];
 hsp_tile tile[hsp_tileset_number];
 hsp_tile tile_h[hsp_tileset_number]; //pre-mirrored (OAM h-flip)
 hsp_tile tile_r[hsp_tileset_number]; //pre-rotated 90 degrees CW
 hsp_tile tile_rh[hsp_tileset_number];//rotated 90 CW, then mirrored
 uint16_t oam[hsp_count];
 uint16_t oam2[hsp_count];
 uint16_t oam3[hsp_count];
 uint16_t free_stack[hsp_count];//free OAM slots; top of stack pops first
 uint16_t free_count;
 uint32_t offset_x;//TODO: combine offsets into one 32 bit qty
 uint32_t offset_y;
} hsp_struct;

/* All engine state: one of these per engine instance. */
typedef struct {
  vp viewport;
  bg_struct bg[bg_layer_count];
  fsp_struct fsp;
  hsp_struct hsp;
} gfx_context;

/* Tiles pack 8 pixels (4 bits each) per 32 bit word, leftmost pixel in the
   highest nibble. These helpers work for any square tile size (8 or 16). */
static inline uint8_t tile_get_pixel(const uint32_t* tile, uint8_t size,
                                     uint8_t x, uint8_t y) {
  uint32_t group = tile[((uint16_t)y*size + x)>>3];
  return (group >> (4*(7-(x&7)))) & 0x0F;
}

static inline void tile_set_pixel(uint32_t* tile, uint8_t size,
                                  uint8_t x, uint8_t y, uint8_t value) {
  uint16_t idx = ((uint16_t)y*size + x)>>3;
  uint8_t shift = 4*(7-(x&7));
  tile[idx] = (tile[idx] & ~((uint32_t)0x0F<<shift)) | ((uint32_t)value<<shift);
}

/* setup */
void initialize_viewport(gfx_context* g);
void initialize_bg(gfx_context* g);
void initialize_full_sprites(gfx_context* g);
void initialize_half_sprites(gfx_context* g);

/* sprite OAM: add returns fsp_count/hsp_count when no slot is free */
uint8_t add_fsp(gfx_context* g, uint16_t sp_index, uint8_t pal_index,
                uint16_t x_pos, uint16_t y_pos);
void delete_fsp(gfx_context* g, uint8_t sp_id);
void clear_all_fsp(gfx_context* g); //palettes and tilesets untouched
void set_fsp(gfx_context* g, int16_t sp_id, int16_t sp_index);
void set_pos_fsp(gfx_context* g, int16_t sp_id, int16_t pos_x, int16_t pos_y);
void set_fsp_effects(gfx_context* g, uint8_t sp_id, uint8_t h_flip,
                     uint8_t v_flip, uint8_t rotate, uint8_t double_size);
void set_fsp_priority(gfx_context* g, uint8_t sp_id, uint8_t prio);

uint8_t add_hsp(gfx_context* g, uint16_t sp_index, uint8_t pal_index,
                uint16_t x_pos, uint16_t y_pos);
void delete_hsp(gfx_context* g, uint8_t sp_id);
void clear_all_hsp(gfx_context* g);
void set_hsp(gfx_context* g, int16_t sp_id, int16_t sp_index);
void set_pos_hsp(gfx_context* g, int16_t sp_id, int16_t pos_x, int16_t pos_y);
void set_hsp_effects(gfx_context* g, uint8_t sp_id, uint8_t h_flip,
                     uint8_t v_flip, uint8_t rotate, uint8_t double_size);
void set_hsp_priority(gfx_context* g, uint8_t sp_id, uint8_t prio);

/* half-sprite text: tiles indexed by ASCII code, spaces skipped */
void draw_text(gfx_context* g, const char* label,
               int16_t x_pos, int16_t y_pos, uint8_t pal_index);

/* asset loading; gfxtype: 0 = bg0, 1 = bg1, 2 = fsp, 3 = hsp,
   4 = bg2, 5 = bg3, 6 = bg4 */
#define GFXTYPE_BG2 4
#define GFXTYPE_BG3 5
#define GFXTYPE_BG4 6
void read_gfx_data(gfx_context* g, FILE* file, int gfxtype);
void read_map_data(gfx_context* g, FILE* file, uint8_t layer);
void disable_bg_layer(gfx_context* g, uint8_t layer);

/* rendering: backgrounds compose into a caller-owned buffer of
   viewport.width x viewport.height; sprites draw over an existing one.
   The pass API lets a scene interleave sprite priorities between bg
   layers (prio 0xFF renders every sprite regardless of priority). */
void gfx_render_backgrounds(gfx_context* g, uint16_t* cache);
void gfx_render_sprites(gfx_context* g, uint16_t* buf);
void gfx_render_bg_layer(gfx_context* g, uint16_t* buf, uint8_t layer,
                         uint8_t as_base);
void gfx_render_fsp_pass(gfx_context* g, uint16_t* buf, uint8_t prio);
void gfx_render_hsp_pass(gfx_context* g, uint16_t* buf, uint8_t prio);
color_16bit average_colors(color_16bit color1, color_16bit color2);

#endif //GFX_ENGINE_H
