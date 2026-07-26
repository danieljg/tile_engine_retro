//Tile-based graphics engine — implementation. See gfx_engine.h.
#include <string.h>

#include "gfx_engine.h"

void initialize_viewport(gfx_context* g) {
 g->viewport.width	= full_tile_size*vp_tile_number_x;
 g->viewport.height 	= full_tile_size*vp_tile_number_y;
 g->viewport.x_origin	= vp_x_origin;
 g->viewport.y_origin	= vp_y_origin;
}

void initialize_bg(gfx_context* g)
{
  uint16_t kk=0;
  for(uint8_t ll=0; ll<bg_layer_count; ll++) {
    for(uint16_t jj=0; jj<full_tile_size*vp_tile_number_y; jj++){
      g->bg[ll].offset_x[jj] = 0;
      g->bg[ll].offset_y[jj] = 0;
    }
    //test pattern: pseudo-random tile indices (kept for scene 1's look)
    for(uint16_t jj=0; jj<layer_tile_number_x*layer_tile_number_y; jj++) {
      g->bg[ll].tilemap[jj]=kk%300;
      kk=kk+7;
    }
    for(uint8_t pp=0; pp<bg_palettes_per_layer; pp++) {
      for(uint8_t cc=0; cc<bg_palette_color_count; cc++) {
        g->bg[ll].palette[pp].color[cc]=null_color;
      }
    }
  }
}

/* Sprite transform strategy: h-flip and rotation trade memory for speed by
   pre-computing transformed tilesets at load time (tile_h, tile_r, tile_rh);
   v-flip and double-size are computed per row/pixel at render time.
   The OAM2 bit combinations cover all 8 orientations:
     rotation + h_flip + v_flip = rotate -90 degrees. */
static void generate_tile_variants(const uint32_t* src, uint32_t* h,
                                   uint32_t* r, uint32_t* rh,
                                   uint16_t tile_count, uint8_t size) {
  uint16_t words = ((uint16_t)size*size)>>3;
  for (uint16_t t=0; t<tile_count; t++) {
    const uint32_t* s = src + (uint32_t)t*words;
    for (uint8_t y=0; y<size; y++) {
      for (uint8_t x=0; x<size; x++) {
        uint8_t v = tile_get_pixel(s, size, x, y);
        tile_set_pixel(h  + (uint32_t)t*words, size, size-1-x, y, v);//mirror
        tile_set_pixel(r  + (uint32_t)t*words, size, size-1-y, x, v);//90 CW
        tile_set_pixel(rh + (uint32_t)t*words, size, y, x, v);//90 CW mirrored
      }
    }
  }
}

/* ---- full sprites ---- */

void clear_all_fsp(gfx_context* g) {
 //all OAM slots become free; pushed in reverse so slot 0 pops first
 for(uint8_t ii=0;ii<fsp_count;ii++) {
  g->fsp.oam[ii]=0x00;
  g->fsp.oam2[ii]=0x00;
  g->fsp.oam3[ii]=0x00;
  g->fsp.free_stack[ii]=fsp_count-1-ii;
 }
 g->fsp.free_count=fsp_count;
}

void initialize_full_sprites(gfx_context* g) {
 for(uint8_t ii=0;ii<fsp_palette_number;ii++)
 {
  for(uint8_t jj=0;jj<fsp_palette_color_count;jj++)
  {
   g->fsp.palette[ii].color[jj]=null_color;
  }
 }
 g->fsp.offset_x=0;
 g->fsp.offset_y=0;
 clear_all_fsp(g);
}

/* El sprite es creado en el primer espacio disponible (free list, O(1)).
   Regresa fsp_count si no hay espacios libres. */
uint8_t add_fsp(gfx_context* g, uint16_t sp_index, uint8_t pal_index,
                uint16_t x_pos, uint16_t y_pos) {
  if (g->fsp.free_count == 0) return fsp_count;
  uint8_t i = (uint8_t)g->fsp.free_stack[--g->fsp.free_count];
  g->fsp.oam[i] =
    Mask_fsp_oam_in_use |
    Mask_fsp_oam_enable |
    (uint16_t)(pal_index<<10) |
    (sp_index & Mask_fsp_oam_index);
  g->fsp.oam2[i] = y_pos & Mask_fsp_oam2_y_pos;
  g->fsp.oam3[i] = x_pos & Mask_fsp_oam3_x_pos;
  return i;
}

void delete_fsp(gfx_context* g, uint8_t sp_id) {
  if (!(g->fsp.oam[sp_id] & Mask_fsp_oam_in_use)) return;//double-free guard
  g->fsp.oam[sp_id] = 0x00;
  g->fsp.oam2[sp_id] = 0x00;
  g->fsp.oam3[sp_id] = 0x00;
  g->fsp.free_stack[g->fsp.free_count++] = sp_id;
}

//sets the transform bits of a sprite, preserving its position
void set_fsp_effects(gfx_context* g, uint8_t sp_id, uint8_t h_flip,
                     uint8_t v_flip, uint8_t rotate, uint8_t double_size) {
  uint16_t o2 = g->fsp.oam2[sp_id] & Mask_fsp_oam2_y_pos;
  if (h_flip)      o2 |= Mask_fsp_oam2_h_flip;
  if (v_flip)      o2 |= Mask_fsp_oam2_v_flip;
  if (rotate)      o2 |= Mask_fsp_oam2_rotation;
  if (double_size) o2 |= Mask_fsp_oam2_double;
  g->fsp.oam2[sp_id] = o2;
}

void set_pos_fsp(gfx_context* g, int16_t sp_id, int16_t pos_x, int16_t pos_y) {
  uint16_t oambuff;
  oambuff=g->fsp.oam2[sp_id];
  g->fsp.oam2[sp_id]=(uint16_t)((oambuff&(~Mask_fsp_oam2_y_pos))|(pos_y%(layer_tile_number_y*full_tile_size)));
  oambuff=g->fsp.oam3[sp_id];
  g->fsp.oam3[sp_id]=(uint16_t)((oambuff&(~Mask_fsp_oam3_x_pos))|(pos_x%(layer_tile_number_x*full_tile_size)));
}

void set_fsp(gfx_context* g, int16_t sp_id, int16_t sp_index) {
  uint16_t oambuff;
  oambuff=g->fsp.oam[sp_id];
  g->fsp.oam[sp_id] = (uint16_t)((oambuff&(~Mask_fsp_oam_index))|sp_index);
}

/* ---- half sprites ---- */

void clear_all_hsp(gfx_context* g) {
  //all OAM slots become free; pushed in reverse so slot 0 pops first
  for(uint8_t ii=0;ii<hsp_count;ii++) {
    g->hsp.oam[ii]=0x00;
    g->hsp.oam2[ii]=0x00;
    g->hsp.oam3[ii]=0x00;
    g->hsp.free_stack[ii]=hsp_count-1-ii;
  }
  g->hsp.free_count=hsp_count;
}

void initialize_half_sprites(gfx_context* g)
{
  for(uint8_t ii=0;ii<hsp_palette_number;ii++)
  {
    for(uint8_t jj=0;jj<hsp_palette_color_count;jj++) {
      g->hsp.palette[ii].color[jj]=null_color;
    }
  }
  g->hsp.offset_x=0;
  g->hsp.offset_y=0;
  clear_all_hsp(g);
}

uint8_t add_hsp(gfx_context* g, uint16_t sp_index, uint8_t pal_index,
                uint16_t x_pos, uint16_t y_pos) {
  if (g->hsp.free_count == 0) return hsp_count;
  uint8_t i = (uint8_t)g->hsp.free_stack[--g->hsp.free_count];
  g->hsp.oam[i] =
    Mask_hsp_oam_in_use |
    Mask_hsp_oam_enable |
    (uint16_t)(pal_index<<10) |
    (sp_index & Mask_hsp_oam_index);
  g->hsp.oam2[i] = y_pos & Mask_hsp_oam2_y_pos;
  g->hsp.oam3[i] = x_pos & Mask_hsp_oam3_x_pos;
  return i;
}

void delete_hsp(gfx_context* g, uint8_t sp_id) {
  if (!(g->hsp.oam[sp_id] & Mask_hsp_oam_in_use)) return;//double-free guard
  g->hsp.oam[sp_id] = 0x00;
  g->hsp.oam2[sp_id] = 0x00;
  g->hsp.oam3[sp_id] = 0x00;
  g->hsp.free_stack[g->hsp.free_count++] = sp_id;
}

void set_hsp_effects(gfx_context* g, uint8_t sp_id, uint8_t h_flip,
                     uint8_t v_flip, uint8_t rotate, uint8_t double_size) {
  uint16_t o2 = g->hsp.oam2[sp_id] & Mask_hsp_oam2_y_pos;
  if (h_flip)      o2 |= Mask_hsp_oam2_h_flip;
  if (v_flip)      o2 |= Mask_hsp_oam2_v_flip;
  if (rotate)      o2 |= Mask_hsp_oam2_rotation;
  if (double_size) o2 |= Mask_hsp_oam2_double;
  g->hsp.oam2[sp_id] = o2;
}

void set_pos_hsp(gfx_context* g, int16_t sp_id, int16_t pos_x, int16_t pos_y) {
  uint16_t oambuff;
  oambuff=g->hsp.oam2[sp_id];
  g->hsp.oam2[sp_id]=(uint16_t)((oambuff&(~Mask_hsp_oam2_y_pos))|(pos_y%(layer_tile_number_y*full_tile_size)));
  oambuff=g->hsp.oam3[sp_id];
  g->hsp.oam3[sp_id]=(uint16_t)((oambuff&(~Mask_hsp_oam3_x_pos))|(pos_x%(layer_tile_number_x*full_tile_size)));
}

void set_hsp(gfx_context* g, int16_t sp_id, int16_t sp_index) {
  uint16_t oambuff;
  oambuff=g->hsp.oam[sp_id];
  g->hsp.oam[sp_id] = (uint16_t)((oambuff&(~Mask_hsp_oam_index))|sp_index);
}

void draw_text(gfx_context* g, const char* label,
               int16_t x_pos, int16_t y_pos, uint8_t pal_index) {
  int16_t x_tile;
  size_t len = strlen(label);
  for (size_t i = 0; i < len; i++) {
    x_tile = (int16_t)(x_pos + (int16_t)i * 8);
    if (label[i] != 32) add_hsp(g, (uint16_t)label[i], pal_index,
                                (uint16_t)x_tile, (uint16_t)y_pos);
  }
}

/* ---- asset loading ---- */

void read_gfx_data(gfx_context* g, FILE* file, int gfxtype) {
  uint8_t buff[4];
  uint8_t palette_size, palette_qty, colors_per_pal;
  uint8_t tile_size;
  uint16_t tile_qty, line_bytesize=0;

  if (fread(buff,1,4,file)!=4 || memcmp(buff,"GFX\n",4)!=0) {
    fprintf(stdout,"GFX: bad header\n");
    return;
  }
  if (fread(buff,1,2,file)!=2) return;
  palette_size = buff[0];
  palette_qty = buff[1];
  colors_per_pal = (uint8_t)(1 << palette_size);
  fprintf(stdout,"GFX: palette size %d (%d colors), qty %d\n",
          palette_size, colors_per_pal, palette_qty);
  if (palette_size != 4) {
    fprintf(stdout,"GFX: only 16-color palettes are supported\n");
    return;
  }

  for (uint8_t pal_i=0; pal_i<palette_qty; pal_i++) {
    for (uint8_t col_i=0; col_i<colors_per_pal; col_i++) {
      if (fread(buff,1,2,file)!=2) return;
      color_16bit c = (color_16bit)((buff[0] << 8) | buff[1]);
      if(gfxtype==0)      g->bg[0].palette[pal_i].color[col_i] = c;
      else if(gfxtype==1) g->bg[1].palette[pal_i].color[col_i] = c;
      else if(gfxtype==GFXTYPE_BG2) g->bg[2].palette[pal_i].color[col_i] = c;
      else if(gfxtype==2) g->fsp.palette[pal_i].color[col_i] = c;
      else if(gfxtype==3) g->hsp.palette[pal_i].color[col_i] = c;
    }
  }

  if (fread(buff,1,3,file)!=3) return;
  tile_size = buff[0];
  tile_qty = (uint16_t)((buff[1]<<8) | buff[2]);
  line_bytesize = (uint16_t)((tile_size * palette_size) >> 3);
  fprintf(stdout,"GFX: %d tiles of %dx%d\n", tile_qty, tile_size, tile_size);
  for (uint16_t tile_i=0; tile_i<tile_qty; tile_i++) {
    for (uint8_t line_i=0; line_i<tile_size; line_i++) {
      for (uint16_t byte_i=0; byte_i < line_bytesize; byte_i+=4) {
        if (fread(buff,1,4,file)!=4) return;
        uint32_t pixbuffer =
          ((uint32_t)buff[0]<<24)|((uint32_t)buff[1]<<16)
          |((uint32_t)buff[2]<<8)|buff[3];
        uint16_t word = (uint16_t)((byte_i + (line_i*line_bytesize))>>2);
        if(gfxtype==0)      g->bg[0].tile[tile_i].eight_pixel_color_index[word]=pixbuffer;
        else if(gfxtype==1) g->bg[1].tile[tile_i].eight_pixel_color_index[word]=pixbuffer;
        else if(gfxtype==GFXTYPE_BG2) g->bg[2].tile[tile_i].eight_pixel_color_index[word]=pixbuffer;
        else if(gfxtype==2) g->fsp.tile[tile_i].eight_pixel_color_index[word]=pixbuffer;
        else if(gfxtype==3) g->hsp.tile[tile_i].eight_pixel_color_index[word]=pixbuffer;
      }
    }
  }

  //pre-compute the transformed tilesets for the sprite layers
  //(gfxtype 0/1 background layers have no variants)
  if (gfxtype==2) {
    generate_tile_variants((const uint32_t*)g->fsp.tile,
                           (uint32_t*)g->fsp.tile_h,
                           (uint32_t*)g->fsp.tile_r,
                           (uint32_t*)g->fsp.tile_rh,
                           fsp_tileset_number, full_tile_size);
  }
  else if (gfxtype==3) {
    generate_tile_variants((const uint32_t*)g->hsp.tile,
                           (uint32_t*)g->hsp.tile_h,
                           (uint32_t*)g->hsp.tile_r,
                           (uint32_t*)g->hsp.tile_rh,
                           hsp_tileset_number, half_tile_size);
  }
}

/* Reads a 32x32 tilemap from a MAP file (as written by gfxtool) into a
   background layer. Entries are big-endian uint16 tilemap words. */
void read_map_data(gfx_context* g, FILE* file, uint8_t layer) {
  uint8_t buff[6];
  if (fread(buff,1,6,file)!=6 || memcmp(buff,"MAP\n",4)!=0) {
    fprintf(stdout,"MAP: bad header\n");
    return;
  }
  if (buff[4]!=layer_tile_number_x || buff[5]!=layer_tile_number_y) {
    fprintf(stdout,"MAP: unexpected size %dx%d\n",buff[4],buff[5]);
    return;
  }
  for (uint16_t i=0;i<layer_tile_number_x*layer_tile_number_y;i++) {
    uint8_t b[2];
    if (fread(b,1,2,file)!=2) {
      fprintf(stdout,"MAP: truncated at entry %d\n",i);
      return;
    }
    g->bg[layer].tilemap[i]=(uint16_t)((b[0]<<8)|b[1]);
  }
}

/* ---- rendering ---- */

color_16bit average_colors(color_16bit color1, color_16bit color2) {
  return ( ( ( (color1&Mask_red)   + (color2&Mask_red)   )>>1 )&Mask_red   ) |
         ( ( ( (color1&Mask_green) + (color2&Mask_green) )>>1 )&Mask_green ) |
         ( ( ( (color1&Mask_blue)  + (color2&Mask_blue)  )>>1 )&Mask_blue  );
}

/* Background scanline renderer, shared by both layers. Walks tile-aligned
   spans: one tilemap lookup per span, one group fetch per 8 pixels, a
   rolling shift per pixel. The base layer (BG1) writes every pixel,
   including index 0 (backdrop) and disabled tiles, always alpha-stripped;
   the overlay (BG0) skips index 0 and disabled tiles, writes opaque colors
   and 50/50-blends semitransparent ones over what is already there. */
static void render_bg_scanline(gfx_context* g, uint16_t* line, uint32_t yy,
                               uint8_t layer, uint8_t is_overlay)
{
  uint32_t ysrc = (uint32_t)(yy + g->viewport.y_origin
                             - g->bg[layer].offset_y[yy])
                  & (full_tile_size*layer_tile_number_y - 1);
  uint16_t trow = (uint16_t)((ysrc>>4)<<5); //tilemap row base (32 per row)
  uint8_t in_y = ysrc & 15;
  uint32_t x0 = g->viewport.x_origin - g->bg[layer].offset_x[yy];
  uint16_t W = (uint16_t)g->viewport.width;
  uint16_t xx = 0;
  while (xx < W) {
    uint32_t xsrc = (uint32_t)(xx + x0)
                    & (full_tile_size*layer_tile_number_x - 1);
    uint8_t in_x = xsrc & 15;
    uint16_t span = (uint16_t)(16 - in_x);
    if (span > W - xx) span = (uint16_t)(W - xx);
    uint16_t entry = g->bg[layer].tilemap[trow + (xsrc>>4)];
    uint8_t pal = (entry & Mask_bgtm_palette)>>10;
    if (entry & Mask_bgtm_disable) {
      if (is_overlay) { xx = (uint16_t)(xx + span); continue; }
      color_16bit c = g->bg[layer].palette[pal].color[0] & 0x7FFF;
      for (uint16_t s=0; s<span; s++) line[xx++] = c;
      continue;
    }
    const uint32_t* row_groups =
      &g->bg[layer].tile[entry & Mask_bgtm_index]
        .eight_pixel_color_index[(uint16_t)in_y<<1];
    uint32_t group = row_groups[in_x>>3];
    uint8_t shift = 4*(7-(in_x&7));
    for (uint16_t s=0; s<span; s++, in_x++) {
      if ((in_x&7)==0 && s) {//fetch 8 pixels at a time
        group = row_groups[in_x>>3];
        shift = 28;
      }
      uint8_t pix = (group>>shift) & 0x0F;
      shift -= 4;
      if (!is_overlay) {
        line[xx++] = g->bg[layer].palette[pal].color[pix] & 0x7FFF;
      }
      else {
        if (pix) {
          color_16bit c = g->bg[layer].palette[pal].color[pix];
          if (c < 0x8000) line[xx] = c;
          else line[xx] = average_colors(c, line[xx]);//semitransparent
        }
        xx++;
      }
    }
  }
}

void gfx_render_backgrounds(gfx_context* g, uint16_t* cache)
{
  for (uint32_t yy=0; yy<g->viewport.height; yy++) {
    uint16_t* line = cache + yy*g->viewport.width;
    render_bg_scanline(g, line, yy, 2, 0); //BG2: back layer, opaque base
    render_bg_scanline(g, line, yy, 1, 1); //BG1: middle overlay
    render_bg_scanline(g, line, yy, 0, 1); //BG0: front overlay
  }
}

//marks every cell of a layer disabled; as an overlay it then draws nothing
void disable_bg_layer(gfx_context* g, uint8_t layer)
{
  for (uint16_t i=0; i<layer_tile_number_x*layer_tile_number_y; i++) {
    g->bg[layer].tilemap[i] = Mask_bgtm_disable;
  }
}

/* Generic sprite layer renderer (fsp and hsp share the same OAM bit layout,
   so the fsp masks are used as the canonical ones).
   Transforms: h-flip and rotation select a pre-computed tileset variant
   (memory); v-flip remaps the source row and double-size samples each
   source pixel into a 2x2 block (cpu). rotation+h_flip+v_flip = -90 deg.
   Slots are drawn in descending order so lower slots land on top. */
static void render_sprite_layer(
    gfx_context* g,
    const uint16_t* oam, const uint16_t* oam2, const uint16_t* oam3,
    uint16_t slot_count,
    const uint32_t* tiles_n, const uint32_t* tiles_h,
    const uint32_t* tiles_r, const uint32_t* tiles_rh,
    uint8_t size,
    const color_16bit* palette_colors, uint8_t colors_per_palette,
    uint32_t layer_offset_x, uint32_t layer_offset_y,
    uint16_t* buf, uint16_t stride)
{
  uint16_t words = ((uint16_t)size*size)>>3;
  uint8_t groups_per_row = size>>3;
  uint16_t W = (uint16_t)g->viewport.width;
  for (int16_t slot = (int16_t)(slot_count-1); slot >= 0; slot--) {
    uint16_t o = oam[slot];
    if (!(o & Mask_fsp_oam_in_use)) continue;
    if (!(o & Mask_fsp_oam_enable)) continue;
    uint8_t pal = (o & Mask_fsp_oam_palette)>>10;
    uint16_t o2 = oam2[slot];
    uint8_t vfl = (o2 & Mask_fsp_oam2_v_flip) != 0;
    uint8_t hfl = (o2 & Mask_fsp_oam2_h_flip) != 0;
    uint8_t rot = (o2 & Mask_fsp_oam2_rotation) != 0;
    uint8_t dbl = (o2 & Mask_fsp_oam2_double) != 0;
    const uint32_t* tiles = rot ? (hfl ? tiles_rh : tiles_r)
                                : (hfl ? tiles_h  : tiles_n);
    const uint32_t* tile = tiles + (uint32_t)(o & Mask_fsp_oam_index)*words;
    uint8_t out_size = (uint8_t)(size << dbl);
    //clip once per sprite: at most two visible x spans (wraparound)
    uint16_t base_x = (uint16_t)((oam3[slot] & Mask_fsp_oam3_x_pos)
                      - g->viewport.x_origin + layer_offset_x)
                      & (full_tile_size*layer_tile_number_x - 1);
    uint16_t base_y = (uint16_t)((o2 & Mask_fsp_oam2_y_pos)
                      - g->viewport.y_origin + layer_offset_y)
                      & (full_tile_size*layer_tile_number_y - 1);
    struct { uint8_t ii0; uint16_t xx0; uint8_t len; } spans[2];
    uint8_t nspans = 0;
    if (base_x < W) {
      uint16_t len = (uint16_t)(W - base_x);
      if (len > out_size) len = out_size;
      spans[nspans].ii0 = 0;
      spans[nspans].xx0 = base_x;
      spans[nspans].len = (uint8_t)len;
      nspans++;
    }
    if (base_x + out_size > full_tile_size*layer_tile_number_x) {//wrapped tail
      uint8_t tail = (uint8_t)(base_x + out_size
                               - full_tile_size*layer_tile_number_x);
      spans[nspans].ii0 = (uint8_t)(full_tile_size*layer_tile_number_x - base_x);
      spans[nspans].xx0 = 0;
      spans[nspans].len = tail;
      nspans++;
    }
    if (nspans == 0) continue;
    for (uint8_t jj=0; jj<out_size; jj++) {//itera sobre renglones
      uint16_t yy = (uint16_t)(base_y + jj)
                    & (full_tile_size*layer_tile_number_y - 1);
      if (yy >= g->viewport.height) continue;//discriminar renglones visibles
      uint8_t src_row = jj >> dbl;
      if (vfl) src_row = (uint8_t)(size-1-src_row);
      const uint32_t* row_groups = tile + (uint32_t)src_row*groups_per_row;
      uint16_t* line = buf + (uint32_t)yy*stride;
      for (uint8_t s=0; s<nspans; s++) {
        uint16_t xx = spans[s].xx0;
        uint8_t ii = spans[s].ii0;
        int8_t g_idx = -1;
        uint32_t group = 0;
        for (uint8_t k=0; k<spans[s].len; k++, ii++, xx++) {
          uint8_t src_col = ii >> dbl;
          if ((int8_t)(src_col>>3) != g_idx) {//fetch 8 pixels at a time
            g_idx = (int8_t)(src_col>>3);
            group = row_groups[g_idx];
          }
          uint8_t pix = (group >> (4*(7-(src_col&7)))) & 0x0F;
          if (pix==0) continue;
          color_16bit c = palette_colors[(uint16_t)(pal*colors_per_palette) + pix];
          if (c < 0x8000) {
            line[xx] = c;
          }
          else {
            line[xx] = average_colors(c, line[xx]);//semitransparent
          }
        }
      }
    }
  }
}

void gfx_render_sprites(gfx_context* g, uint16_t* buf)
{
  uint16_t stride = (uint16_t)g->viewport.width;
  //full sprites below half-sprites (SP0 then SP1)
  render_sprite_layer(g, g->fsp.oam, g->fsp.oam2, g->fsp.oam3, fsp_count,
                      (const uint32_t*)g->fsp.tile,
                      (const uint32_t*)g->fsp.tile_h,
                      (const uint32_t*)g->fsp.tile_r,
                      (const uint32_t*)g->fsp.tile_rh,
                      full_tile_size,
                      (const color_16bit*)g->fsp.palette,
                      fsp_palette_color_count,
                      g->fsp.offset_x, g->fsp.offset_y, buf, stride);
  render_sprite_layer(g, g->hsp.oam, g->hsp.oam2, g->hsp.oam3, hsp_count,
                      (const uint32_t*)g->hsp.tile,
                      (const uint32_t*)g->hsp.tile_h,
                      (const uint32_t*)g->hsp.tile_r,
                      (const uint32_t*)g->hsp.tile_rh,
                      half_tile_size,
                      (const color_16bit*)g->hsp.palette,
                      hsp_palette_color_count,
                      g->hsp.offset_x, g->hsp.offset_y, buf, stride);
}
