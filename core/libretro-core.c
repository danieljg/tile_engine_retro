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
static void save_ship_palettes(void);
static void shimmer_ship_palettes(void);

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
  initialize_full_sprites();
  initialize_half_sprites();
  frame_buf = calloc(viewport.width * viewport.height, sizeof(uint16_t));
  load_gfx("bg0.gfx", 0);
  load_gfx("bg1.gfx", 1);
  load_gfx("fsp.gfx", 2);
  load_gfx("hsp.gfx", 3);
  save_ship_palettes();
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
    //viewport.x_origin=(viewport.x_origin+bg_scroll_per_step)%(layer_tile_number_x*full_tile_size);
    //viewport.y_origin=(viewport.y_origin-bg_scroll_per_step)%(layer_tile_number_y*full_tile_size);
  }//*/

  if(animation_frame_counter==0){
    update_animations();
    animate_bg0_blocks();
    shimmer_ship_palettes();
  }

}

//the bg0 tileset is organized in three animation bands: tiles 0-6, 8-14 and
//17-19 are frames of the same block; entries outside the bands stay static
static void animate_bg0_blocks(void) {
  for (uint16_t i=0; i<layer_tile_number_x*layer_tile_number_y; i++) {
    uint16_t entry = bg[0].tilemap[i];
    if (entry & Mask_bgtm_disable) continue;
    uint16_t idx = entry & Mask_bgtm_index;
    uint16_t next;
    if      (idx <= 6)              next = (idx + 1) % 7;
    else if (idx >= 8 && idx <= 14) next = ((idx - 8 + 1) % 7) + 8;
    else if (idx >= 17 && idx <= 19) next = ((idx - 17 + 1) % 3) + 17;
    else continue;
    bg[0].tilemap[i] = (entry & (~Mask_bgtm_index)) | next;
  }
}

//ship palette shimmer: oscillates the brightness of fsp palettes 1-4
//(one per player ship) by +/-1 around the values loaded from fsp.gfx
static fsp_palette ship_palette_base[4];

static void save_ship_palettes(void) {
  for (uint8_t p=0; p<4; p++) ship_palette_base[p] = fsp.palette[1+p];
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

static void shimmer_ship_palettes(void) {
  static uint8_t phase = 0;
  phase = (phase+1) & 3;
  int8_t delta = (phase==1) - (phase==3); // 0,+1,0,-1
  for (uint8_t p=0; p<4; p++) {
    for (uint8_t c=1; c<fsp_palette_color_count; c++) { //index 0 is transparent
      fsp.palette[1+p].color[c] =
        shift_brightness(ship_palette_base[p].color[c], delta);
    }
  }
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
  for (int16_t slot = slot_count-1; slot >= 0; slot--) {
    uint16_t o = oam[slot];
    if (!(o & Mask_fsp_oam_in_use)) continue;
    if (!(o & Mask_fsp_oam_enable)) continue;
    uint8_t pal = (o & Mask_fsp_oam_palette)>>10;
    uint16_t o2 = oam2[slot];
    uint16_t yy_pos = o2 & Mask_fsp_oam2_y_pos;
    uint16_t xx_pos = oam3[slot] & Mask_fsp_oam3_x_pos;
    uint8_t vfl = (o2 & Mask_fsp_oam2_v_flip) != 0;
    uint8_t hfl = (o2 & Mask_fsp_oam2_h_flip) != 0;
    uint8_t rot = (o2 & Mask_fsp_oam2_rotation) != 0;
    uint8_t dbl = (o2 & Mask_fsp_oam2_double) != 0;
    const uint32_t* tiles = rot ? (hfl ? tiles_rh : tiles_r)
                                : (hfl ? tiles_h  : tiles_n);
    const uint32_t* tile = tiles + (uint32_t)(o & Mask_fsp_oam_index)*words;
    uint8_t out_size = size << dbl;
    for (uint8_t jj=0; jj<out_size; jj++) {//itera sobre renglones
      uint8_t src_row = jj >> dbl;
      if (vfl) src_row = size-1-src_row;
      uint16_t yy = ((uint16_t)(yy_pos+jj-viewport.y_origin+layer_offset_y))
                    %(full_tile_size*layer_tile_number_y);
      if (yy >= viewport.height) continue;//discriminar renglones visibles
      uint16_t* line = buf + yy*stride;
      for (uint8_t ii=0; ii<out_size; ii++) {//itera sobre pixeles
        uint16_t xx = ((uint16_t)(xx_pos+ii-viewport.x_origin+layer_offset_x))
                      %(full_tile_size*layer_tile_number_x);
        if (xx >= viewport.width) continue;//discriminar pixeles visibles
        uint8_t pix = tile_get_pixel(tile, size, ii >> dbl, src_row);
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


/* Dibuja una frame del juego
*/
static void render_frame(void)
{
  uint16_t *buf    = frame_buf;
  uint16_t stride  = viewport.width; // Stride igual a ancho de viewport
  uint16_t *line   = buf;


  //background rendering
  uint32_t yy_vp;
  uint32_t xx_vp;
  uint32_t eightpixdata=0;
  uint32_t tilemap_index=0;
  uint32_t tileset_index=0;
  uint8_t  palette_index=0;
  color_16bit colordata;
  color_16bit clearbuf;

  ///*
  for (uint32_t yy=0; yy<viewport.height; yy++, line+=stride) {
    yy_vp=yy+viewport.y_origin-bg[0].offset_y[yy];
    for (uint32_t xx=0; xx<viewport.width; xx++) {
      xx_vp=xx+viewport.x_origin-bg[0].offset_x[yy];
      tilemap_index = ( (xx_vp/full_tile_size)%layer_tile_number_x
                      + (yy_vp/full_tile_size)*layer_tile_number_x )
                      %(layer_tile_number_x*layer_tile_number_y);
      tilemap_index = bg[0].tilemap[tilemap_index];
      if(tilemap_index<Mask_bgtm_disable){
        palette_index=(tilemap_index&Mask_bgtm_palette)>>10;
        tileset_index=tilemap_index&Mask_bgtm_index;
        eightpixdata = bg[0].tile[tileset_index]
                       .eight_pixel_color_index[(( (yy_vp%full_tile_size)*full_tile_size
                                                +(xx_vp%full_tile_size))>>3)
                                              %(full_tile_size*full_tile_size)];
        uint8_t pixdata = (uint8_t) (eightpixdata>>(4*(7-(xx_vp%8))));//mmmhmm...
        pixdata = pixdata & 0x0F;
        if(pixdata){//check if palette index is not null, if so we gotta render BG0
          colordata=bg[0].palette[palette_index].color[pixdata];
          if(colordata<0x8000){
            //pixel is opaque
            line[xx]=colordata;
          }
          else{
            //pixel is semitransparent, render BG1
            yy_vp=yy+viewport.y_origin-bg[1].offset_y[yy];
            xx_vp=xx+viewport.x_origin-bg[1].offset_x[yy];
            tilemap_index = ( (xx_vp/full_tile_size)%layer_tile_number_x
                            + (yy_vp/full_tile_size)*layer_tile_number_x )
                            %(layer_tile_number_x*layer_tile_number_y);
            tilemap_index = bg[1].tilemap[tilemap_index];
            palette_index=(tilemap_index&Mask_bgtm_palette)>>10;
            if(tilemap_index<Mask_bgtm_disable){
              tileset_index=tilemap_index&Mask_bgtm_index;
              eightpixdata = bg[1].tile[tileset_index]
                             .eight_pixel_color_index[(( (yy_vp%full_tile_size)*full_tile_size
                                                      +(xx_vp%full_tile_size))>>3)
                                                      %(full_tile_size*full_tile_size)];
              pixdata = (uint8_t) (eightpixdata>>(4*(7-(xx_vp%8))));//mmmhmm...
              pixdata = pixdata & 0x0F;
              clearbuf=bg[1].palette[palette_index].color[pixdata];
              colordata=average_colors(clearbuf,colordata);
              line[xx]=colordata;//don't mask alpha after averaging colors
            }
            else{
              //when bg1 tile is disabled, we take the bg1 null for semitransparency
              clearbuf=bg[1].palette[palette_index].color[0];
              colordata=average_colors(clearbuf,colordata);
              line[xx]=colordata;//don't mask alpha after averaging colors
            }
          }
          continue;
        }
      }
      //BG0 is transparent or disabled, render BG1
      yy_vp=yy+viewport.y_origin-bg[1].offset_y[yy];
      xx_vp=xx+viewport.x_origin-bg[1].offset_x[yy];
      tilemap_index = ( (xx_vp/full_tile_size)%layer_tile_number_x
                      + (yy_vp/full_tile_size)*layer_tile_number_x )
                      %(layer_tile_number_x*layer_tile_number_y);
      tilemap_index = bg[1].tilemap[tilemap_index];
      palette_index=(tilemap_index&Mask_bgtm_palette)>>10;
      if(tilemap_index<Mask_bgtm_disable){
        tileset_index=tilemap_index&Mask_bgtm_index;
        eightpixdata = bg[1].tile[tileset_index]
                       .eight_pixel_color_index[(( (yy_vp%full_tile_size)*full_tile_size
                                                +(xx_vp%full_tile_size))>>3)
                                                %(full_tile_size*full_tile_size)];
        uint8_t pixdata = (uint8_t) (eightpixdata>>(4*(7-(xx_vp%8))));//mmmhmm...
        pixdata = pixdata & 0x0F;
        colordata=bg[1].palette[palette_index].color[pixdata];
        colordata=colordata<<1;
        line[xx]=colordata>>1;
      }
      else{
        uint16_t buf=bg[1].palette[palette_index].color[0]<<1;
        line[xx]=buf>>1;
      }
    }
  }
  //*/


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

//TODO: real save states — serialize game + OAM + scroll state
size_t retro_serialize_size(void)
{
   return 0;
}

bool retro_serialize(void *data_, size_t size)
{
   (void)data_;
   (void)size;
   return false;
}

bool retro_unserialize(const void *data_, size_t size)
{
   (void)data_;
   (void)size;
   return false;
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
