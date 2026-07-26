#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#ifdef HAVE_XMP
#include <xmp.h>
#endif

#include "libretro.h"
#include "../gfx_engine.h"
#include "../game2.h"

#if defined(_3DS)
#endif

static uint16_t *frame_buf;
//backgrounds are composed into a cache and only recomposed when their
//state (scroll, tilemap animation, palettes) changes
static uint16_t *bg_cache;
static uint8_t bg_cache_dirty = 1;
static struct retro_log_callback logging;
static retro_log_printf_t log_cb;

#ifdef HAVE_XMP
static xmp_context ctx;
static struct xmp_module_info mi;
static uint8_t music_playing = 0;

static void display_audiomodule_info(struct xmp_module_info *mi)
{
	int i, j;
	struct xmp_module *mod = mi->mod;

	printf("Name: %s\n", mod->name);
	printf("Type: %s\n", mod->type);
	printf("Number of patterns: %d\n", mod->pat);
	printf("Number of tracks: %d\n", mod->trk);
	printf("Number of channels: %d\n", mod->chn);
	printf("Number of instruments: %d\n", mod->ins);
	printf("Number of samples: %d\n", mod->smp);
	printf("Initial speed: %d\n", mod->spd);
	printf("Initial BPM: %d\n", mod->bpm);
	printf("Length in patterns: %d\n", mod->len);

	printf("\n");

	printf("Instruments:\n");
	for (i = 0; i < mod->ins; i++) {
		struct xmp_instrument *ins = &mod->xxi[i];

		printf("%02x %-32.32s V:%02x R:%04x %c%c%c\n",
				i, ins->name, ins->vol, ins->rls,
				ins->aei.flg & XMP_ENVELOPE_ON ? 'A' : '-',
				ins->pei.flg & XMP_ENVELOPE_ON ? 'P' : '-',
				ins->fei.flg & XMP_ENVELOPE_ON ? 'F' : '-');

		for (j = 0; j < ins->nsm; j++) {
			struct xmp_subinstrument *sub = &ins->sub[j];
			printf("   %02x V:%02x GV:%02x P:%02x X:%+04d F:%+04d\n",
					j, sub->vol, sub->gvl, sub->pan,
					sub->xpo, sub->fin);
		}
	}

	printf("\n");

	printf("Samples:\n");
	for (i = 0; i < mod->smp; i++) {
		struct xmp_sample *smp = &mod->xxs[i];

		printf("%02x %-32.32s %05x %05x %05x %c%c%c%c%c%c",
				i, smp->name, smp->len, smp->lps, smp->lpe,
				smp->flg & XMP_SAMPLE_16BIT ? 'W' : '-',
				smp->flg & XMP_SAMPLE_LOOP ? 'L' : '-',
				smp->flg & XMP_SAMPLE_LOOP_BIDIR ? 'B' : '-',
				smp->flg & XMP_SAMPLE_LOOP_REVERSE ? 'R' : '-',
				smp->flg & XMP_SAMPLE_LOOP_FULL ? 'F' : '-',
				smp->flg & XMP_SAMPLE_SYNTH ? 'S' : '-');

		if (smp->len > 0 && smp->lpe >= smp->len) {
			printf(" LOOP ERROR");
		}

		printf("\n");
	}
}
#endif //HAVE_XMP



static void fallback_log(enum retro_log_level level, const char *fmt, ...)
{
   (void)level;
   va_list va;
   va_start(va, fmt);
   vfprintf(stderr, fmt, va);
   va_end(va);
}

// contadores de frames
static uint32_t frame_counter=0;
static uint32_t scroll_frame_counter=0;
static uint32_t animation_frame_counter=0;
static uint32_t scroll_has_updated_bgtm=0;
static uint16_t bg_scroll_per_step=1;
static uint16_t bg_scroll_wait_frames=2;
static uint16_t animation_wait_frames=16;
// contador de scroll
static uint32_t scrolling_tilemap_index=0;

//comment the following line to get nice pixel art in the debug console
#define NUMERIC_DEBUG_OUTPUT
#ifdef NUMERIC_DEBUG_OUTPUT
#else
#endif

static void animate_bg0_blocks(void);
static void save_base_palettes(void);
static void shimmer_ship_palettes(void);
static void pulse_bg0_palette(void);
static void set_fade(uint8_t level);

//loads a gfx file from the frontend's working directory (core/ via make run)
static void load_gfx(const char* path, int gfxtype)
{
  FILE* file = fopen(path,"rb");
  if (!file) {
    log_cb(RETRO_LOG_ERROR, "Missing graphics file: %s\n", path);
    return;
  }
  read_gfx_data(file, gfxtype);
  fclose(file);
}

void retro_init(void)
{
  initialize_viewport();
  initialize_bg();
  //the test pattern lands tile 15 (solid black) as the third member of
  //some clusters, where it reads as a hole; show the solid gray block
  //(tile 6, static by design) there instead
  for (uint16_t i=0; i<layer_tile_number_x*layer_tile_number_y; i++) {
    if ((bg[0].tilemap[i] & Mask_bgtm_index) == 15) {
      bg[0].tilemap[i] = (bg[0].tilemap[i] & (~Mask_bgtm_index)) | 6;
    }
  }
  initialize_full_sprites();
  initialize_half_sprites();
  frame_buf = calloc(viewport.width * viewport.height, sizeof(uint16_t));
  bg_cache  = calloc(viewport.width * viewport.height, sizeof(uint16_t));
  load_gfx("bg0.gfx", 0);
  load_gfx("bg1.gfx", 1);
  load_gfx("fsp.gfx", 2);
  load_gfx("hsp.gfx", 3);
  save_base_palettes();
  initialize_game();
  default_scores();
#ifdef HAVE_XMP
  ctx = xmp_create_context();
  if( xmp_load_module(ctx,"test.xm") == 0 ){
    if (xmp_start_player(ctx, 31920, XMP_FORMAT_MONO) == 0) {
      music_playing = 1;
      xmp_get_module_info(ctx, &mi);
      display_audiomodule_info(&mi);
    }
  }
  else {
    log_cb(RETRO_LOG_WARN, "Could not load music module test.xm\n");
  }
#endif
}

void retro_deinit(void)
{
#ifdef HAVE_XMP
   if (music_playing) {
      xmp_end_player(ctx);
      xmp_release_module(ctx);
   }
   xmp_free_context(ctx);
   music_playing = 0;
#endif
   free(frame_buf);
   frame_buf = NULL;
   free(bg_cache);
   bg_cache = NULL;
}

unsigned retro_api_version(void)
{
   return RETRO_API_VERSION;
}

void retro_set_controller_port_device(unsigned port, unsigned device)
{
   log_cb(RETRO_LOG_INFO, "Plugging device %u into port %u.\n", device, port);
}

void retro_get_system_info(struct retro_system_info *info)
{
   memset(info, 0, sizeof(*info));
   info->library_name     = "TileEngineCore";
   info->library_version  = "v1";
   info->need_fullpath    = false;
   info->valid_extensions = NULL; // Anything is fine, we don't care.
}

static retro_video_refresh_t video_cb;
static retro_audio_sample_t audio_cb;
static retro_audio_sample_batch_t audio_batch_cb;
static retro_environment_t environ_cb;
static retro_input_poll_t input_poll_cb;
static retro_input_state_t input_state_cb;

void retro_get_system_av_info(struct retro_system_av_info *info)
{
   float aspect = (float) vp_tile_number_x / (float) vp_tile_number_y ;
   float sampling_rate = 31920.0f;

   info->timing = (struct retro_system_timing) {
      .fps = 60.0,
      .sample_rate = sampling_rate,
   };

   info->geometry = (struct retro_game_geometry) {
      .base_width   = viewport.width,
      .base_height  = viewport.height,
      .max_width    = viewport.width,
      .max_height   = viewport.height,
      .aspect_ratio = aspect,
   };
}

void retro_set_environment(retro_environment_t cb)
{
   environ_cb = cb;

   bool no_content = true;
   cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_content);

   if (cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &logging))
      log_cb = logging.log;
   else
      log_cb = fallback_log;
}

void retro_set_audio_sample(retro_audio_sample_t cb)
{
   audio_cb = cb;
}

void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb)
{
   audio_batch_cb = cb;
}

void retro_set_input_poll(retro_input_poll_t cb)
{
   input_poll_cb = cb;
}

void retro_set_input_state(retro_input_state_t cb)
{
   input_state_cb = cb;
}

void retro_set_video_refresh(retro_video_refresh_t cb)
{
   video_cb = cb;
}

void retro_reset(void)
{
}

static void move_viewport(int8_t vel_x, int8_t vel_y) {
  viewport.x_origin=(viewport.x_origin+vel_x*bg_scroll_per_step)%(layer_tile_number_x*full_tile_size);
  viewport.y_origin=(viewport.y_origin+vel_y*bg_scroll_per_step)%(layer_tile_number_y*full_tile_size);
  bg_cache_dirty=1;
}

static void update_input(void)
{
  input_poll_cb();
  for (uint8_t player_id=0; player_id < MAX_PLAYERS; player_id++) {
    uint8_t input = 0x00;
    if (input_state_cb(player_id, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START))
      input |= MASK_INPUT_START;
    if (input_state_cb(player_id, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP))
      input |= MASK_INPUT_UP;
    if (input_state_cb(player_id, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN))
      input |= MASK_INPUT_DOWN;
    if (input_state_cb(player_id, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT))
      input |= MASK_INPUT_LEFT;
    if (input_state_cb(player_id, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT))
      input |= MASK_INPUT_RIGHT;
    if (input_state_cb(player_id, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A))
      input |= MASK_INPUT_A;
    if (input_state_cb(player_id, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B))
      input |= MASK_INPUT_B;
    if (input_state_cb(player_id, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X))
      input |= MASK_INPUT_C;
    players.base[player_id] =
      (players.base[player_id] & (~MASK_PLAYER_BASE_INPUT)) | input;
  }
}


/*Actualiza los primeros 8 HALF SPRITES (0 - 7) con las coordenadas de entrada */

/* Actualiza las mecánicas del juego.
*/
static void update_game(void) {
  for (uint8_t i=0; i<MAX_PLAYERS; i++) {
    update_player(i);
  }
  update_pprojectiles();
  update_enemies();
  update_enemy_spawner();
  check_collisions();
  update_hud();

  frame_counter++;
  scroll_frame_counter=frame_counter%bg_scroll_wait_frames;
  animation_frame_counter=frame_counter%animation_wait_frames;

  //boot fade-in: black to full brightness over the first 64 frames
  if(frame_counter<=64 && (frame_counter&1)==0) set_fade((uint8_t)(frame_counter>>1));

 ///*
  if(scroll_frame_counter==0){
    for(uint32_t yy=0;yy<(vp_tile_number_y*full_tile_size);yy++){
      bg[0].offset_x[yy]--;
      //starfield: base speeds 2,2,3 with a few faster streak lines
      uint8_t speed;
      if(yy%3==0){
        speed=2;
      }
      else{
        speed=(yy%3)+1;
      }
      if(yy%17==0) speed=5;
      else if(yy%11==0) speed=4;
      bg[1].offset_x[yy]-=speed;
    }
    bg_cache_dirty=1;
    //viewport.x_origin=(viewport.x_origin+bg_scroll_per_step)%(layer_tile_number_x*full_tile_size);
    //viewport.y_origin=(viewport.y_origin-bg_scroll_per_step)%(layer_tile_number_y*full_tile_size);
  }//*/

  if(animation_frame_counter==0){
    update_animations();
    animate_bg0_blocks();
    shimmer_ship_palettes();
    pulse_bg0_palette();
    bg_cache_dirty=1;
  }

}

/* bg0 block animation: each band plays a scripted sequence instead of a
   plain cycle. A cell's band is identified by its current tile (band tile
   sets are disjoint), and its phase is offset by its tilemap position, so
   surges travel across neighboring cells. Tiles 6, 7 and 15 stay static. */
static const uint8_t seq_conduit[] = //power surges ebb and flow
  {0,1,2,3,2,3,4,3,5,4,5,5,4,3,4,3,2,3,3,4,5,4,3,2,4,3,2,3,2,1,2,1};
static const uint8_t seq_porthole[] = //radar blip: breathe in, double-pulse
  {8,8,9,10,11,12,13,14,13,12,11,10,9,8,8,10,12,14,12,10,8,9,11,13,14,14,13,11,9,8};
static const uint8_t seq_plasma[] = //unstable flicker
  {17,18,18,17,19,17,17,18,19,18,18,18,19,17,19,18,17,18,18,18,19,17,19,19};
static const uint8_t seq_glimmer[] = //slow rotation with stutters
  {16,16,20,16,20,21,20,20,21,21,16,21,20,16,21,20,16,21,20,21,21,20,20,16};

static uint32_t bg0_anim_step = 0;

static void animate_bg0_blocks(void) {
  bg0_anim_step++;
  for (uint16_t i=0; i<layer_tile_number_x*layer_tile_number_y; i++) {
    uint16_t entry = bg[0].tilemap[i];
    if (entry & Mask_bgtm_disable) continue;
    uint16_t idx = entry & Mask_bgtm_index;
    const uint8_t* seq;
    uint8_t len;
    if (idx <= 5) {
      seq = seq_conduit;  len = sizeof(seq_conduit);
    }
    else if (idx >= 8 && idx <= 14) {
      seq = seq_porthole; len = sizeof(seq_porthole);
    }
    else if (idx >= 17 && idx <= 19) {
      seq = seq_plasma;   len = sizeof(seq_plasma);
    }
    else if (idx == 16 || idx == 20 || idx == 21) {
      seq = seq_glimmer;  len = sizeof(seq_glimmer);
    }
    else continue;
    bg[0].tilemap[i] = (entry & (~Mask_bgtm_index))
                     | seq[(bg0_anim_step + i) % len];
  }
}

/* Live palettes are derived data: base palettes (saved once after the gfx
   load) -> per-tick effects (shimmer, pulse) -> master fade. Fade in/out
   costs a handful of palette recomputes instead of a per-pixel pass. */
static fsp_palette fsp_palette_base[fsp_palette_number];
static hsp_palette hsp_palette_base[hsp_palette_number];
static bg_palette  bg_palette_base[bg_layer_count]; //palette 0 of each layer
static uint8_t bg0_pulse_phase = 0;
static uint8_t fade_level = 32; //0 = black .. 32 = full brightness

static void save_base_palettes(void) {
  memcpy(fsp_palette_base, fsp.palette, sizeof(fsp_palette_base));
  memcpy(hsp_palette_base, hsp.palette, sizeof(hsp_palette_base));
  for (uint8_t l=0; l<bg_layer_count; l++) bg_palette_base[l] = bg[l].palette[0];
}

static color_16bit inline apply_fade(color_16bit c) {
  if (fade_level >= 32) return c;
  uint16_t r = (((c&Mask_red)>>10)  * fade_level) >> 5;
  uint16_t g = (((c&Mask_green)>>5) * fade_level) >> 5;
  uint16_t b = ((c&Mask_blue)       * fade_level) >> 5;
  return (c & Mask_alpha) | (r<<10) | (g<<5) | b;
}

static void refresh_palettes(void) {
  for (uint8_t p=0; p<fsp_palette_number; p++)
    for (uint8_t c=0; c<fsp_palette_color_count; c++)
      fsp.palette[p].color[c] = apply_fade(fsp_palette_base[p].color[c]);
  for (uint8_t p=0; p<hsp_palette_number; p++)
    for (uint8_t c=0; c<hsp_palette_color_count; c++)
      hsp.palette[p].color[c] = apply_fade(hsp_palette_base[p].color[c]);
  for (uint8_t l=0; l<bg_layer_count; l++)
    for (uint8_t c=0; c<bg_palette_color_count; c++)
      bg[l].palette[0].color[c] = apply_fade(bg_palette_base[l].color[c]);
  bg_cache_dirty = 1;
}

static void set_fade(uint8_t level) {
  if (level > 32) level = 32;
  if (level == fade_level) return;
  fade_level = level;
  refresh_palettes();
}

static color_16bit inline shift_brightness(color_16bit c, int8_t d) {
  int16_t r = ((c&Mask_red)>>10) + d;
  int16_t g = ((c&Mask_green)>>5) + d;
  int16_t b = (c&Mask_blue) + d;
  if (r<0) r=0; if (r>31) r=31;
  if (g<0) g=0; if (g>31) g=31;
  if (b<0) b=0; if (b>31) b=31;
  return (c&Mask_alpha) | (r<<10) | (g<<5) | b;
}

static uint8_t shimmer_phase = 0;

//ship palette shimmer: oscillates the brightness of fsp palettes 1-4
//(one per player ship) by +/-1 around the values loaded from fsp.gfx
static void shimmer_ship_palettes(void) {
  shimmer_phase = (shimmer_phase+1) & 3;
  int8_t delta = (shimmer_phase==1) - (shimmer_phase==3); // 0,+1,0,-1
  for (uint8_t p=1; p<=4; p++) {
    for (uint8_t c=1; c<fsp_palette_color_count; c++) { //index 0 is transparent
      fsp.palette[p].color[c] =
        apply_fade(shift_brightness(fsp_palette_base[p].color[c], delta));
    }
  }
}

//bg0 palette pulse: the colors participating in the block animations
//(indices 6-15; 0-5 are backdrop and structural grays) breathe +/-1
//around their loaded values, each index offset in phase
static void pulse_bg0_palette(void) {
  static const int8_t deltas[4] = {0, 1, 0, -1};
  bg0_pulse_phase = (bg0_pulse_phase+1) & 3;
  for (uint8_t c=6; c<bg_palette_color_count; c++) {
    bg[0].palette[0].color[c] =
      apply_fade(shift_brightness(bg_palette_base[0].color[c],
                                  deltas[(bg0_pulse_phase + c) & 3]));
  }
  bg_cache_dirty = 1;
}

static color_16bit inline average_colors(color_16bit color1, color_16bit color2) {
  return ( ( ( (color1&Mask_red)   + (color2&Mask_red)   )>>1 )&Mask_red   ) |
         ( ( ( (color1&Mask_green) + (color2&Mask_green) )>>1 )&Mask_green ) |
         ( ( ( (color1&Mask_blue)  + (color2&Mask_blue)  )>>1 )&Mask_blue  );
}

/* Generic sprite layer renderer (fsp and hsp share the same OAM bit layout,
   so the fsp masks are used as the canonical ones).
   Transforms: h-flip and rotation select a pre-computed tileset variant
   (memory); v-flip remaps the source row and double-size samples each
   source pixel into a 2x2 block (cpu). rotation+h_flip+v_flip = -90 deg.
   Slots are drawn in descending order so lower slots land on top. */
static void render_sprite_layer(
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
  uint16_t W = viewport.width;
  for (int16_t slot = slot_count-1; slot >= 0; slot--) {
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
    uint8_t out_size = size << dbl;
    //clip once per sprite: at most two visible x spans (wraparound)
    uint16_t base_x = (uint16_t)((oam3[slot] & Mask_fsp_oam3_x_pos)
                      - viewport.x_origin + layer_offset_x)
                      & (full_tile_size*layer_tile_number_x - 1);
    uint16_t base_y = (uint16_t)((o2 & Mask_fsp_oam2_y_pos)
                      - viewport.y_origin + layer_offset_y)
                      & (full_tile_size*layer_tile_number_y - 1);
    struct { uint8_t ii0; uint16_t xx0; uint8_t len; } spans[2];
    uint8_t nspans = 0;
    if (base_x < W) {
      uint16_t len = W - base_x;
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
      if (yy >= viewport.height) continue;//discriminar renglones visibles
      uint8_t src_row = jj >> dbl;
      if (vfl) src_row = size-1-src_row;
      const uint32_t* row_groups = tile + (uint32_t)src_row*groups_per_row;
      uint16_t* line = buf + yy*stride;
      for (uint8_t s=0; s<nspans; s++) {
        uint16_t xx = spans[s].xx0;
        uint8_t ii = spans[s].ii0;
        int8_t g_idx = -1;
        uint32_t group = 0;
        for (uint8_t k=0; k<spans[s].len; k++, ii++, xx++) {
          uint8_t src_col = ii >> dbl;
          if ((int8_t)(src_col>>3) != g_idx) {//fetch 8 pixels at a time
            g_idx = src_col>>3;
            group = row_groups[g_idx];
          }
          uint8_t pix = (group >> (4*(7-(src_col&7)))) & 0x0F;
          if (pix==0) continue;
          color_16bit c = palette_colors[(uint16_t)pal*colors_per_palette + pix];
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

/* Background scanline renderer, shared by both layers. Walks tile-aligned
   spans: one tilemap lookup per span, one group fetch per 8 pixels, a
   rolling shift per pixel. The base layer (BG1) writes every pixel,
   including index 0 (backdrop) and disabled tiles, always alpha-stripped;
   the overlay (BG0) skips index 0 and disabled tiles, writes opaque colors
   and 50/50-blends semitransparent ones over what is already there. */
static void render_bg_scanline(uint16_t* line, uint32_t yy, uint8_t layer,
                               uint8_t is_overlay)
{
  uint32_t ysrc = (uint32_t)(yy + viewport.y_origin - bg[layer].offset_y[yy])
                  & (full_tile_size*layer_tile_number_y - 1);
  uint16_t trow = (uint16_t)((ysrc>>4)<<5); //tilemap row base (32 per row)
  uint8_t in_y = ysrc & 15;
  uint32_t x0 = viewport.x_origin - bg[layer].offset_x[yy];
  uint16_t W = viewport.width;
  uint16_t xx = 0;
  while (xx < W) {
    uint32_t xsrc = (uint32_t)(xx + x0)
                    & (full_tile_size*layer_tile_number_x - 1);
    uint8_t in_x = xsrc & 15;
    uint16_t span = 16 - in_x;
    if (span > W - xx) span = W - xx;
    uint16_t entry = bg[layer].tilemap[trow + (xsrc>>4)];
    uint8_t pal = (entry & Mask_bgtm_palette)>>10;
    if (entry & Mask_bgtm_disable) {
      if (is_overlay) { xx += span; continue; }
      color_16bit c = bg[layer].palette[pal].color[0] & 0x7FFF;
      for (uint16_t s=0; s<span; s++) line[xx++] = c;
      continue;
    }
    const uint32_t* row_groups =
      &bg[layer].tile[entry & Mask_bgtm_index]
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
        line[xx++] = bg[layer].palette[pal].color[pix] & 0x7FFF;
      }
      else {
        if (pix) {
          color_16bit c = bg[layer].palette[pal].color[pix];
          if (c < 0x8000) line[xx] = c;
          else line[xx] = average_colors(c, line[xx]);//semitransparent
        }
        xx++;
      }
    }
  }
}

static void render_backgrounds(uint16_t* cache)
{
  for (uint32_t yy=0; yy<viewport.height; yy++) {
    uint16_t* line = cache + yy*viewport.width;
    render_bg_scanline(line, yy, 1, 0); //BG1: back layer, opaque base
    render_bg_scanline(line, yy, 0, 1); //BG0: front layer, overlay
  }
}

/* Dibuja una frame del juego
*/
static void render_frame(void)
{
  uint16_t *buf    = frame_buf;
  uint16_t stride  = viewport.width; // Stride igual a ancho de viewport

  //backgrounds come from the cache, recomposed only when dirty
  if (bg_cache_dirty) {
    render_backgrounds(bg_cache);
    bg_cache_dirty = 0;
  }
  memcpy(buf, bg_cache,
         (size_t)viewport.width * viewport.height * sizeof(uint16_t));


  //sprite rendering: full sprites below half-sprites (SP0 then SP1)
  render_sprite_layer(fsp.oam, fsp.oam2, fsp.oam3, fsp_count,
                      (const uint32_t*)fsp.tile, (const uint32_t*)fsp.tile_h,
                      (const uint32_t*)fsp.tile_r, (const uint32_t*)fsp.tile_rh,
                      full_tile_size,
                      (const color_16bit*)fsp.palette, fsp_palette_color_count,
                      fsp.offset_x, fsp.offset_y, buf, stride);
  render_sprite_layer(hsp.oam, hsp.oam2, hsp.oam3, hsp_count,
                      (const uint32_t*)hsp.tile, (const uint32_t*)hsp.tile_h,
                      (const uint32_t*)hsp.tile_r, (const uint32_t*)hsp.tile_rh,
                      half_tile_size,
                      (const color_16bit*)hsp.palette, hsp_palette_color_count,
                      hsp.offset_x, hsp.offset_y, buf, stride);

  video_cb(buf, viewport.width, viewport.height, stride << 1);
}


static void check_variables(void)
{
}

static void audio_callback(void)
{
   int16_t audiobuff[532];//532 samples at 31920 Hz is exactly one 60 fps frame
#ifdef HAVE_XMP
   if (music_playing)
      xmp_play_buffer(ctx,&audiobuff,532*2,1);
   else
      memset(audiobuff, 0, sizeof(audiobuff));
#else
   memset(audiobuff, 0, sizeof(audiobuff));
#endif
   for(unsigned ii=0;ii<532;ii++) {
      audio_cb(audiobuff[ii], audiobuff[ii]);
   }
}

void retro_run(void)
{
   update_input();
   update_game();
   render_frame();
   audio_callback();

   bool updated = false;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
      check_variables();
}

bool retro_load_game(const struct retro_game_info *info)
{
   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_0RGB1555;
   if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
   {
      log_cb(RETRO_LOG_INFO, "0RGB1555 is not supported.\n");
      return false;
   }

   check_variables();

   (void)info;
   return true;
}

void retro_unload_game(void)
{
}

unsigned retro_get_region(void)
{
   return RETRO_REGION_NTSC;
}

bool retro_load_game_special(unsigned type, const struct retro_game_info *info, size_t num)
{
   (void)type;
   (void)info;
   (void)num;
   return false;
}

/* Save states: a versioned manifest of memcpy'd blocks. Everything that
   mutates after load is listed once, and serialize/unserialize walk the
   same table so they cannot drift apart. Tilesets, their transform
   variants and the base palettes are immutable after load and stay out;
   the shimmer/pulse palettes are recomputed from base + phase anyway,
   but serializing them keeps the first frame after a load exact.
   Not endian-portable; music position is not saved. */
#define SAVESTATE_MAGIC   0x30524554 // "TER0"
#define SAVESTATE_VERSION 1

typedef struct { void* ptr; size_t size; } save_block;

static const save_block save_blocks[] = {
  //game state
  {&players, sizeof(players)},
  {&enemies, sizeof(enemies)},
  {&pprojectiles, sizeof(pprojectiles)},
  {&eprojectiles, sizeof(eprojectiles)},
  {&power_ups, sizeof(power_ups)},
  {&top_scores, sizeof(top_scores)},
  {&gamedata1, sizeof(gamedata1)},
  {&gamedata2, sizeof(gamedata2)},
  {&spawner_wait, sizeof(spawner_wait)},
  {&spawner_lane, sizeof(spawner_lane)},
  //full sprite bank: OAM, free list, scroll, live palettes
  {fsp.oam, sizeof(fsp.oam)},
  {fsp.oam2, sizeof(fsp.oam2)},
  {fsp.oam3, sizeof(fsp.oam3)},
  {fsp.free_stack, sizeof(fsp.free_stack)},
  {&fsp.free_count, sizeof(fsp.free_count)},
  {&fsp.offset_x, sizeof(fsp.offset_x)},
  {&fsp.offset_y, sizeof(fsp.offset_y)},
  {fsp.palette, sizeof(fsp.palette)},
  //half sprite bank
  {hsp.oam, sizeof(hsp.oam)},
  {hsp.oam2, sizeof(hsp.oam2)},
  {hsp.oam3, sizeof(hsp.oam3)},
  {hsp.free_stack, sizeof(hsp.free_stack)},
  {&hsp.free_count, sizeof(hsp.free_count)},
  {&hsp.offset_x, sizeof(hsp.offset_x)},
  {&hsp.offset_y, sizeof(hsp.offset_y)},
  {hsp.palette, sizeof(hsp.palette)},
  //backgrounds: tilemaps and palettes mutate through the animations
  {bg[0].tilemap, sizeof(bg[0].tilemap)},
  {bg[0].offset_x, sizeof(bg[0].offset_x)},
  {bg[0].offset_y, sizeof(bg[0].offset_y)},
  {bg[0].palette, sizeof(bg[0].palette)},
  {bg[1].tilemap, sizeof(bg[1].tilemap)},
  {bg[1].offset_x, sizeof(bg[1].offset_x)},
  {bg[1].offset_y, sizeof(bg[1].offset_y)},
  {bg[1].palette, sizeof(bg[1].palette)},
  //viewport and timeline
  {&viewport, sizeof(viewport)},
  {&frame_counter, sizeof(frame_counter)},
  {&bg0_anim_step, sizeof(bg0_anim_step)},
  {&shimmer_phase, sizeof(shimmer_phase)},
  {&bg0_pulse_phase, sizeof(bg0_pulse_phase)},
};
#define SAVE_BLOCK_COUNT (sizeof(save_blocks)/sizeof(save_blocks[0]))

size_t retro_serialize_size(void)
{
   size_t total = 8; //magic + version
   for (size_t i=0; i<SAVE_BLOCK_COUNT; i++) total += save_blocks[i].size;
   return total;
}

bool retro_serialize(void *data_, size_t size)
{
   if (size < retro_serialize_size()) return false;
   uint8_t *p = data_;
   uint32_t magic = SAVESTATE_MAGIC, version = SAVESTATE_VERSION;
   memcpy(p, &magic, 4); p += 4;
   memcpy(p, &version, 4); p += 4;
   for (size_t i=0; i<SAVE_BLOCK_COUNT; i++) {
      memcpy(p, save_blocks[i].ptr, save_blocks[i].size);
      p += save_blocks[i].size;
   }
   return true;
}

bool retro_unserialize(const void *data_, size_t size)
{
   if (size < retro_serialize_size()) return false;
   const uint8_t *p = data_;
   uint32_t magic, version;
   memcpy(&magic, p, 4); p += 4;
   memcpy(&version, p, 4); p += 4;
   if (magic != SAVESTATE_MAGIC || version != SAVESTATE_VERSION)
      return false;
   for (size_t i=0; i<SAVE_BLOCK_COUNT; i++) {
      memcpy(save_blocks[i].ptr, p, save_blocks[i].size);
      p += save_blocks[i].size;
   }
   return true;
}

void *retro_get_memory_data(unsigned id)
{
   (void)id;
   return NULL;
}

size_t retro_get_memory_size(unsigned id)
{
   (void)id;
   return 0;
}

void retro_cheat_reset(void)
{}

void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
   (void)index;
   (void)enabled;
   (void)code;
}
