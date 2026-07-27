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

#define AUDIO_RATE 44100
#define SAMPLES_PER_FRAME (AUDIO_RATE/60)

static uint16_t *frame_buf;
//backgrounds are composed into a cache and only recomposed when their
//state (scroll, tilemap animation, palettes) changes
static uint16_t *bg_cache;
static uint8_t bg_cache_dirty = 1;
/* Live palettes are derived data: base palettes (saved once after the gfx
   load, re-based on scene loads) -> per-tick effects (shimmer, pulse) ->
   master fade. */
static fsp_palette fsp_palette_base[fsp_palette_number];
static hsp_palette hsp_palette_base[hsp_palette_number];
static bg_palette  bg_palette_base[bg_layer_count]; //palette 0 of each layer
static uint8_t bg0_pulse_phase = 0;
static uint8_t shimmer_phase = 0;
static uint8_t pond_light_phase = 0;
static uint8_t pond_surface_phase = 0;
static uint8_t fade_level = 32; //0 = black .. 32 = full brightness

/* ---- pond tuning: every effect parameter lives here, adjustable from
   the in-game tuner (START in the pond). Serialized with the state. ---- */
typedef struct {
  uint8_t floor_amp;    //0..6  floor warp amplitude (x/4 of wave_table)
  uint8_t floor_freq;   //0..3  floor warp frequency (scanline stride)
  uint8_t floor_drift;  //0..4  floor current drift speed
  uint8_t floor_dir;    //0/1   floor drift direction
  uint8_t floor_caustic;//0/1   slow color rotation on the lit floor cells
  uint8_t light_amp;    //0..6  caustic-layer warp amplitude
  uint8_t light_freq;   //0..3  caustic-layer warp frequency
  uint8_t light_drift;  //0..4  caustic-layer drift speed
  uint8_t light_dir;    //0/1   caustic-layer drift direction
  uint8_t light_sweep;  //0..3  caustic palette+frame sweep: off/12/6/3
  uint8_t surface_on;   //0/1   surface texture layer visible
  uint8_t surface_bob;  //0..4  whole-surface bob amplitude
  uint8_t leaf_bob;     //0..4  leaf drift amplitude
  uint8_t shadows_on;   //0/1   leaf shadows
  uint8_t shadow_disp;  //0..6  shadow displacement in pixels
  uint8_t shadow_anim;  //0..2  rigid / swaying / anchored
  uint8_t veil_density; //0..2  upper veil clouds: off / half / full
  uint8_t vaporwave;    //0..6  music slowdown, 6%% per step (to -36%%)
  uint8_t music;        //track index into music_tracks[]
} pond_tune_t;

static pond_tune_t tune = { 4, 2, 2, 0, 1,
                            2, 2, 1, 1, 2,
                            1, 2, 2, 1, 4, 1,
                            2, 2, 0 };

#define MUSIC_TRACK_COUNT 2
static const char* const music_files[MUSIC_TRACK_COUNT] =
  { "ch_jazz_n.xm", "test.xm" };
static const char* const music_names[MUSIC_TRACK_COUNT] =
  { "JAZZ N", "SPACE" };
static uint8_t tune_open = 0;
static uint8_t tune_sel = 0;
static uint8_t tune_prev_input = 0xFF;
static uint8_t tune_slots[40];
static uint8_t tune_slot_count = 0;
static uint8_t update_pond_tuner(void);
static void pond_cycle_caustic_tiles(void);
static void pond_surface_anim(void);
static struct retro_log_callback logging;
static retro_log_printf_t log_cb;

#ifdef HAVE_XMP
static xmp_context ctx;
static struct xmp_module_info mi;
static uint8_t music_playing = 0;

/* Vaporwave: consume libxmp's 44.1 kHz output slower than real time via
   nearest-neighbour resampling — tempo and pitch drop together, like a
   slowed tape. Levels: 0 off, then -8%%, -16%%, -24%%. Live-adjustable. */
#define MUSIC_FIFO_FRAMES 8192 //power of two
static int16_t music_fifo[MUSIC_FIFO_FRAMES*2];
static uint32_t music_fifo_r = 0, music_fifo_w = 0;
static uint32_t music_pos_frac = 0;

static uint32_t vapor_step(void) {
   static const uint32_t steps[7] =
     { 65536, 61604, 57672, 53740, 49807, 45875, 41943 };
   return steps[tune.vaporwave <= 6 ? tune.vaporwave : 6];
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
static uint16_t bg_scroll_wait_frames=2;
static uint16_t animation_wait_frames=16;

static void animate_bg0_blocks(void);
static void save_base_palettes(void);
static void shimmer_ship_palettes(void);
static void pulse_bg0_palette(void);
static void set_fade(uint8_t level);
static void pond_water_warp(void);
static void pond_caustic_shimmer(void);
static void apply_pond_caustics(void);

//loads a gfx file from the frontend's working directory (core/ via make run)
static void load_gfx(const char* path, int gfxtype)
{
  FILE* file = fopen(path,"rb");
  if (!file) {
    log_cb(RETRO_LOG_ERROR, "Missing graphics file: %s\n", path);
    return;
  }
  read_gfx_data(&GFX, file, gfxtype);
  fclose(file);
}

/* ---- scenes ---- */
#define SCENE_KIND_SHMUP 0
#define SCENE_KIND_POND  1

typedef struct {
  const char* gfx[bg_layer_count]; //front..base; NULL = layer off
  const char* map[bg_layer_count]; //NULL with a gfx = procedural builder
  uint8_t kind;
} scene_def;

static const scene_def scene_defs[SCENE_COUNT] = {
  //0: Asteroid Run (procedural maps; only front + base layers)
  { { "bg0.gfx", NULL, NULL, NULL, "bg1.gfx" },
    { NULL, NULL, NULL, NULL, NULL }, SCENE_KIND_SHMUP },
  //1: Crystal Cavern
  { { "scene2.gfx", NULL, NULL, NULL, "bg1.gfx" },
    { "scene2.map", NULL, NULL, NULL, NULL }, SCENE_KIND_SHMUP },
  /*2: Koi Pond, top to bottom:
      0 surface texture -> 1 depth veil 1 -> 2 depth veil 2 ->
      3 caustics -> 4 floor (sprite passes interleave between them) */
  { { "scene3_surface.gfx", "scene3_depths.gfx", "scene3_depths.gfx",
      "scene3_caustics.gfx", "scene3_depths.gfx" },
    { "scene3_surface.map", "scene3_depth1.map", "scene3_depth2.map",
      "scene3_caustics.map", "scene3_floor.map" },
    SCENE_KIND_POND },
};

//the classic test pattern for bg0, with the black tile swapped for the
//gray block (tile 15 read as a hole as a static cluster member)
static void build_scene1_map(void)
{
  uint16_t kk = 0;
  for (uint16_t i=0; i<layer_tile_number_x*layer_tile_number_y; i++) {
    GFX.bg[0].tilemap[i] = kk % 300;
    kk += 7;
  }
  for (uint16_t i=0; i<layer_tile_number_x*layer_tile_number_y; i++) {
    if ((GFX.bg[0].tilemap[i] & Mask_bgtm_index) == 15) {
      GFX.bg[0].tilemap[i] = (GFX.bg[0].tilemap[i] & (~Mask_bgtm_index)) | 6;
    }
  }
}

//the starfield's slice of the boot test pattern (kk continues from bg0's);
//it lives on the base layer, bg[2]
static void build_starfield_map(void)
{
  uint16_t kk = 7*layer_tile_number_x*layer_tile_number_y;
  for (uint16_t i=0; i<layer_tile_number_x*layer_tile_number_y; i++) {
    GFX.bg[bg_layer_count-1].tilemap[i] = kk % 300;
    kk += 7;
  }
}

static void load_scene_layer(const char* gfx, const char* map, uint8_t layer)
{
  if (gfx == NULL) { //layer unused in this scene
    disable_bg_layer(&GFX, layer);
    return;
  }
  static const uint8_t layer_gfxtype[bg_layer_count] =
    { 0, 1, GFXTYPE_BG2, GFXTYPE_BG3, GFXTYPE_BG4 };
  load_gfx(gfx, layer_gfxtype[layer]);
  if (map == NULL) {
    if (layer == 0) build_scene1_map();
    else if (layer == bg_layer_count-1) build_starfield_map();
  }
  else {
    FILE* mf = fopen(map, "rb");
    if (mf) {
      read_map_data(&GFX, mf, layer);
      fclose(mf);
    }
    else {
      log_cb(RETRO_LOG_ERROR, "Missing map file: %s\n", map);
    }
  }
  bg_palette_base[layer] = GFX.bg[layer].palette[0]; //re-base pulse/fade
}

static void start_scene(uint8_t id)
{
  //pond scenes drive per-scanline offsets; clear them across kind changes
  if (scene_defs[current_scene].kind != scene_defs[id].kind) {
    for (uint16_t yy=0; yy<full_tile_size*vp_tile_number_y; yy++) {
      for (uint8_t l=0; l<bg_layer_count; l++) {
        GFX.bg[l].offset_x[yy] = 0; GFX.bg[l].offset_y[yy] = 0;
      }
    }
  }
  current_scene = id;
  for (uint8_t l=0; l<bg_layer_count; l++) {
    load_scene_layer(scene_defs[id].gfx[l], scene_defs[id].map[l], l);
  }
  bg_cache_dirty = 1;
  set_fade(0);
  frame_counter = 0; //replays the fade-in
  if (id == 1) { //the cavern is metroid territory
    enemy_skin_tile = 0;
    enemy_skin_frames = 6;
    enemy_skin_rotates = 0;
  }
  else { //everywhere else flies the 2018 ships
    enemy_skin_tile = 12;
    enemy_skin_frames = 3;
    enemy_skin_rotates = 1;
  }
  if (scene_defs[id].kind == SCENE_KIND_POND) begin_pond();
  else begin_play();
}

#ifdef HAVE_XMP
static uint8_t applied_music = 0xFF;

static void music_load(uint8_t idx)
{
  if (music_playing) {
    xmp_end_player(ctx);
    xmp_release_module(ctx);
    music_playing = 0;
  }
  music_fifo_r = 0;
  music_fifo_w = 0;
  music_pos_frac = 0;
  if (xmp_load_module(ctx, (char*)music_files[idx]) == 0) {
    if (xmp_start_player(ctx, AUDIO_RATE, 0) == 0) {
      music_playing = 1;
      xmp_get_module_info(ctx, &mi);
      log_cb(RETRO_LOG_INFO, "Music: %s\n", mi.mod->name);
    }
  }
  else {
    log_cb(RETRO_LOG_WARN, "Could not load music module %s\n",
           music_files[idx]);
  }
  applied_music = idx;
}
#endif

void retro_init(void)
{
  initialize_viewport(&GFX);
  initialize_bg(&GFX);
  build_scene1_map();
  build_starfield_map();
  for (uint8_t l=1; l<bg_layer_count-1; l++) disable_bg_layer(&GFX, l);
  initialize_full_sprites(&GFX);
  initialize_half_sprites(&GFX);
  frame_buf = calloc(GFX.viewport.width * GFX.viewport.height, sizeof(uint16_t));
  bg_cache  = calloc(GFX.viewport.width * GFX.viewport.height, sizeof(uint16_t));
  load_gfx("bg0.gfx", 0);
  load_gfx("bg1.gfx", GFXTYPE_BG4);
  load_gfx("fsp.gfx", 2);
  load_gfx("hsp.gfx", 3);
  save_base_palettes();
  initialize_game();
  default_scores();
#ifdef HAVE_XMP
  ctx = xmp_create_context();
  //the track itself loads lazily in retro_run, following tune.music
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
   float sampling_rate = (float)AUDIO_RATE;

   info->timing = (struct retro_system_timing) {
      .fps = 60.0,
      .sample_rate = sampling_rate,
   };

   info->geometry = (struct retro_game_geometry) {
      .base_width   = GFX.viewport.width,
      .base_height  = GFX.viewport.height,
      .max_width    = GFX.viewport.width,
      .max_height   = GFX.viewport.height,
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
  if (game_mode == MODE_MENU) {
    uint8_t selected = update_menu();
    if (selected != 0xFF) start_scene(selected);
  }
  else if (scene_defs[current_scene].kind == SCENE_KIND_POND) {
    uint8_t tuning = update_pond_tuner();
    if (game_mode == MODE_PLAYING) //the tuner may have exited to the menu
      update_pond(frame_counter, !tuning);
  }
  else if (play_wants_menu()) {
    enter_menu();
  }
  else {
    for (uint8_t i=0; i<MAX_PLAYERS; i++) {
      update_player(i);
    }
    update_pprojectiles();
    update_enemies(frame_counter);
    update_timeline(shmup_timeline, frame_counter);
    check_collisions();
    update_hud();
  }

  frame_counter++;
  scroll_frame_counter=frame_counter%bg_scroll_wait_frames;
  animation_frame_counter=frame_counter%animation_wait_frames;

  //boot fade-in: black to full brightness over the first 64 frames
  if(frame_counter<=64 && (frame_counter&1)==0) set_fade((uint8_t)(frame_counter>>1));

  //water moves every frame while the pond scene is loaded (menu included)
  if (scene_defs[current_scene].kind == SCENE_KIND_POND) {
    pond_water_warp();
    if (tune.light_sweep) { //palette sweep + web frames on the caustics
      static const uint8_t sweep_period[4] = { 0, 12, 6, 3 };
      uint8_t period = sweep_period[tune.light_sweep];
      if (frame_counter % period == 0) {
        pond_light_phase = (pond_light_phase+1) & 3;
        apply_pond_caustics();
      }
      if (frame_counter % period == 0) {
        pond_cycle_caustic_tiles();
      }
    }
    if (frame_counter % 7 == 0) { //the surface lives on its own clock
      pond_surface_anim();
    }
  }

 ///*
  if(scroll_frame_counter==0 && scene_defs[current_scene].kind==SCENE_KIND_SHMUP){
    for(uint32_t yy=0;yy<(vp_tile_number_y*full_tile_size);yy++){
      GFX.bg[0].offset_x[yy]--;
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
      GFX.bg[bg_layer_count-1].offset_x[yy]-=speed;
    }
    bg_cache_dirty=1;
    //GFX.viewport.x_origin=(GFX.viewport.x_origin+bg_scroll_per_step)%(layer_tile_number_x*full_tile_size);
    //GFX.viewport.y_origin=(GFX.viewport.y_origin-bg_scroll_per_step)%(layer_tile_number_y*full_tile_size);
  }//*/

  if(animation_frame_counter==0){
    update_animations();
    shimmer_ship_palettes();
    if (scene_defs[current_scene].kind == SCENE_KIND_POND) {
      if (tune.floor_caustic) pond_caustic_shimmer();
    }
    else {
      animate_bg0_blocks(); //the block bands belong to the shmup tilesets
      pulse_bg0_palette();
    }
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
    uint16_t entry = GFX.bg[0].tilemap[i];
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
    GFX.bg[0].tilemap[i] = (entry & (~Mask_bgtm_index))
                     | seq[(bg0_anim_step + i) % len];
  }
}

static void save_base_palettes(void) {
  memcpy(fsp_palette_base, GFX.fsp.palette, sizeof(fsp_palette_base));
  memcpy(hsp_palette_base, GFX.hsp.palette, sizeof(hsp_palette_base));
  for (uint8_t l=0; l<bg_layer_count; l++) bg_palette_base[l] = GFX.bg[l].palette[0];
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
      GFX.fsp.palette[p].color[c] = apply_fade(fsp_palette_base[p].color[c]);
  for (uint8_t p=0; p<hsp_palette_number; p++)
    for (uint8_t c=0; c<hsp_palette_color_count; c++)
      GFX.hsp.palette[p].color[c] = apply_fade(hsp_palette_base[p].color[c]);
  for (uint8_t l=0; l<bg_layer_count; l++)
    for (uint8_t c=0; c<bg_palette_color_count; c++)
      GFX.bg[l].palette[0].color[c] = apply_fade(bg_palette_base[l].color[c]);
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

/* Shimmer and pulse are split into apply (recompute from base at the
   current phase — also used after a save-state restore) and tick
   (advance the phase, then apply). */

//ship palette shimmer: oscillates the brightness of fsp palettes 1-4
//(one per player ship) by +/-1 around the values loaded from fsp.gfx
static void apply_shimmer(void) {
  int8_t delta = (shimmer_phase==1) - (shimmer_phase==3); // 0,+1,0,-1
  for (uint8_t p=1; p<=4; p++) {
    for (uint8_t c=1; c<fsp_palette_color_count; c++) { //index 0 is transparent
      GFX.fsp.palette[p].color[c] =
        apply_fade(shift_brightness(fsp_palette_base[p].color[c], delta));
    }
  }
}

static void shimmer_ship_palettes(void) {
  shimmer_phase = (shimmer_phase+1) & 3;
  apply_shimmer();
}

//bg0 palette pulse: the colors participating in the block animations
//(indices 6-15; 0-5 are backdrop and structural grays) breathe +/-1
//around their loaded values, each index offset in phase
static void apply_pulse(void) {
  static const int8_t deltas[4] = {0, 1, 0, -1};
  for (uint8_t c=6; c<bg_palette_color_count; c++) {
    GFX.bg[0].palette[0].color[c] =
      apply_fade(shift_brightness(bg_palette_base[0].color[c],
                                  deltas[(bg0_pulse_phase + c) & 3]));
  }
  bg_cache_dirty = 1;
}

static void pulse_bg0_palette(void) {
  bg0_pulse_phase = (bg0_pulse_phase+1) & 3;
  apply_pulse();
}

/* ---- pond water effects ---- */

//per-scanline sine warp on both layers: the surface sways gently, the
//depths sway out of phase and drift, so the water appears to refract
static const int8_t wave_table[32] = {
   0, 1, 2, 3, 4, 4, 5, 5,  5, 5, 4, 4, 3, 2, 1, 0,
   0,-1,-2,-3,-4,-4,-5,-5, -5,-5,-4,-4,-3,-2,-1, 0
};

static uint8_t pond_caustic_phase = 0;

static void pond_water_warp(void) {
  /* The deeper, the wavier: the surface texture only translates as a
     whole; the depth veils sway at half and three-quarter strength; the
     caustics carry their own warp; the floor sways fully and drifts
     with the current. */
  uint8_t fsh = (uint8_t)(4 - tune.floor_freq); //scanline stride shifts
  uint8_t lsh = (uint8_t)(4 - tune.light_freq);
  int32_t fdrift = (int32_t)((frame_counter*tune.floor_drift)>>6);
  if (tune.floor_dir) fdrift = -fdrift;
  int32_t ldrift = (int32_t)((frame_counter*tune.light_drift)>>6);
  if (tune.light_dir) ldrift = -ldrift;
  int32_t sway0 =
    ((int32_t)wave_table[(frame_counter>>3) & 31] * tune.surface_bob) >> 2;
  for (uint32_t yy=0; yy<GFX.viewport.height; yy++) {
    int32_t wfloor = ((int32_t)wave_table[((frame_counter>>2) + (yy>>fsh)) & 31]
                      * tune.floor_amp) >> 2;
    int32_t wlight = ((int32_t)wave_table[(((frame_counter*3)>>2)
                                           + (yy>>lsh) + 16) & 31]
                      * tune.light_amp) >> 2;
    GFX.bg[0].offset_x[yy] = (uint32_t)sway0;                //surface
    GFX.bg[1].offset_x[yy] = 0;                              //veil 1
    GFX.bg[2].offset_x[yy] = (uint32_t)((wfloor*3>>2) + fdrift); //veil 2
    GFX.bg[3].offset_x[yy] = (uint32_t)(wlight + ldrift);        //caustics
    GFX.bg[4].offset_x[yy] = (uint32_t)(wfloor + fdrift);        //floor
    //the current: water planes stream downhill (positive offset_y
    //translates the pattern downward); the riverbed stays
    GFX.bg[0].offset_y[yy] = frame_counter>>2;
    GFX.bg[1].offset_y[yy] = frame_counter>>3;
    GFX.bg[2].offset_y[yy] = frame_counter>>3;
    GFX.bg[3].offset_y[yy] = (frame_counter*3)>>4;
    GFX.bg[4].offset_y[yy] = 0;
  }
  bg_cache_dirty = 1;
}

//caustic shimmer: rotates the caustic colors — slowly on the floor,
//quickly on the light-web layer for the flashy sweep
static void apply_pond_caustics(void) {
  //slow rotation of the lit-cell colors on the floor (bg4)
  for (uint8_t k=0; k<4; k++) {
    GFX.bg[4].palette[0].color[6+k] =
      apply_fade(bg_palette_base[4].color[6 + ((k + pond_caustic_phase)&3)]);
  }
  //fast sweep on the caustic layer (bg3): web colors 1-3, ray colors 4-6
  for (uint8_t k=0; k<3; k++) {
    GFX.bg[3].palette[0].color[1+k] =
      apply_fade(bg_palette_base[3].color[1 + ((k + pond_light_phase)%3)]);
    GFX.bg[3].palette[0].color[4+k] =
      apply_fade(bg_palette_base[3].color[4 + ((k + pond_light_phase)%3)]);
  }
  bg_cache_dirty = 1;
}

//surface water: contour frames 0..3 drift through the tilemap, and the
//contour colors 2..4 rotate so whole regions light up as reflections
static void pond_surface_anim(void) {
  for (uint16_t i=0; i<layer_tile_number_x*layer_tile_number_y; i++) {
    uint16_t entry = GFX.bg[0].tilemap[i];
    if (entry & Mask_bgtm_disable) continue;
    uint16_t idx = entry & Mask_bgtm_index;
    if (idx >= 512) continue; //reeds hold still
    GFX.bg[0].tilemap[i] = (uint16_t)((entry & ~Mask_bgtm_index)
                                      | ((idx + 64) & 511));
  }
  pond_surface_phase = (uint8_t)((pond_surface_phase + 1) % 3);
  for (uint8_t k=0; k<3; k++) {
    GFX.bg[0].palette[0].color[2+k] =
      apply_fade(bg_palette_base[0].color[2 + ((k + pond_surface_phase) % 3)]);
  }
  bg_cache_dirty = 1;
}

//caustic web tiles 0..3 are animation frames; advance them cell by cell
static void pond_cycle_caustic_tiles(void) {
  for (uint16_t i=0; i<layer_tile_number_x*layer_tile_number_y; i++) {
    uint16_t entry = GFX.bg[3].tilemap[i];
    if (entry & Mask_bgtm_disable) continue;
    uint16_t idx = entry & Mask_bgtm_index;
    if (idx >= 512) continue; //pools and sparkles hold still
    GFX.bg[3].tilemap[i] = (uint16_t)((entry & ~Mask_bgtm_index)
                                      | ((idx + 64) & 511));
  }
  bg_cache_dirty = 1;
}

static void pond_caustic_shimmer(void) {
  pond_caustic_phase = (pond_caustic_phase+1) & 3;
  apply_pond_caustics();
}

/* ---- the pond tuner: an OSD over the live scene. START toggles it,
   up/down pick a parameter, left/right adjust with instant effect,
   B (outside the tuner) returns to the scene menu. ---- */

typedef struct {
  const char* name;
  uint8_t* value;
  uint8_t max;      //values run 0..max; max 1 renders as OFF/ON
} tune_entry;

static const tune_entry tune_entries[] = {
  { "FLOOR WARP AMP",  &tune.floor_amp,     6 },
  { "FLOOR WARP FREQ", &tune.floor_freq,    3 },
  { "FLOOR DRIFT",     &tune.floor_drift,   4 },
  { "FLOOR DRIFT DIR", &tune.floor_dir,     1 },
  { "FLOOR CAUSTIC",   &tune.floor_caustic, 1 },
  { "LIGHT WARP AMP",  &tune.light_amp,     6 },
  { "LIGHT WARP FREQ", &tune.light_freq,    3 },
  { "LIGHT DRIFT",     &tune.light_drift,   4 },
  { "LIGHT DRIFT DIR", &tune.light_dir,     1 },
  { "LIGHT SWEEP",     &tune.light_sweep,   3 },
  { "SURFACE LAYER",   &tune.surface_on,    1 },
  { "SURFACE BOB",     &tune.surface_bob,   4 },
  { "LEAF BOB",        &tune.leaf_bob,      4 },
  { "SHADOWS",         &tune.shadows_on,    1 },
  { "SHADOW DISP",     &tune.shadow_disp,   6 },
  { "SHADOW ANIM",     &tune.shadow_anim,   2 },
  { "VEIL DENSITY",    &tune.veil_density,  2 },
  { "VAPORWAVE",       &tune.vaporwave,     6 },
  { "MUSIC",           &tune.music,         MUSIC_TRACK_COUNT-1 },
};
#define TUNE_ENTRY_COUNT (sizeof(tune_entries)/sizeof(tune_entries[0]))

static void tune_clear_text(void) {
  for (uint8_t i=0; i<tune_slot_count; i++) delete_hsp(&GFX, tune_slots[i]);
  tune_slot_count = 0;
}

static void tune_add_text(const char* s, int16_t x, int16_t y, uint8_t pal) {
  for (; *s; s++, x = (int16_t)(x+8)) {
    if (*s == ' ') continue;
    uint8_t id = add_hsp(&GFX, (uint16_t)*s, pal, (uint16_t)x, (uint16_t)y);
    if (id < hsp_count && tune_slot_count < sizeof(tune_slots))
      tune_slots[tune_slot_count++] = id;
  }
}

static void tune_draw(void) {
  tune_clear_text();
  const tune_entry* e = &tune_entries[tune_sel];
  char line[8];
  uint8_t v = *e->value;
  tune_add_text(e->name, 24, 16, 2);
  if (e->value == &tune.music) {
    char nb[16];
    nb[0]='<'; nb[1]=' ';
    uint8_t k=2;
    for (const char* s = music_names[v]; *s && k<12; s++) nb[k++]=*s;
    nb[k++]=' '; nb[k++]='>'; nb[k]=0;
    tune_add_text(nb, 24, 28, 0);
  }
  else if (e->max == 1) {
    tune_add_text(v ? "< ON >" : "< OFF >", 24, 28, 0);
  }
  else {
    line[0]='<'; line[1]=' '; line[2]=(char)('0'+v); line[3]=' ';
    line[4]='>'; line[5]=0;
    tune_add_text(line, 24, 28, 0);
  }
}

//reload a pond overlay layer's tilemap after re-enabling it
static void pond_reload_layer(uint8_t layer) {
  const char* map = scene_defs[current_scene].map[layer];
  if (!map) return;
  FILE* mf = fopen(map, "rb");
  if (mf) {
    read_map_data(&GFX, mf, layer);
    fclose(mf);
  }
}

//applies side effects of the entry that just changed
static void tune_apply(uint8_t sel) {
  if (tune_entries[sel].value == &tune.surface_on) {
    if (tune.surface_on) pond_reload_layer(0);
    else disable_bg_layer(&GFX, 0);
  }
  else if (tune_entries[sel].value == &tune.shadows_on) {
    tune_shadows_hint = tune.shadows_on;
    for (uint8_t i=0; i<MAX_LEAVES; i++)
      fsp_set_enabled(leaves.shadow[i], tune.shadows_on);
    for (uint8_t i=0; i<MAX_KOI; i++)
      fsp_set_enabled(koi.shadow[i], tune.shadows_on && !koi.wait[i]);
  }
  else if (tune_entries[sel].value == &tune.leaf_bob) {
    leaf_bob_amp = tune.leaf_bob;
  }
  else if (tune_entries[sel].value == &tune.shadow_disp) {
    leaf_shadow_gap = tune.shadow_disp;
  }
  else if (tune_entries[sel].value == &tune.shadow_anim) {
    leaf_shadow_anim = tune.shadow_anim;
  }
  else if (tune_entries[sel].value == &tune.veil_density) {
    if (tune.veil_density == 0) {
      disable_bg_layer(&GFX, 1);
    }
    else {
      pond_reload_layer(1);
      if (tune.veil_density == 1) { //half: checker the column cells
        for (uint16_t i=0; i<layer_tile_number_x*layer_tile_number_y; i++) {
          uint16_t cx = i & 31, cy = i >> 5;
          if ((cx + cy) & 1) GFX.bg[1].tilemap[i] = Mask_bgtm_disable;
        }
      }
    }
  }
  else if (tune_entries[sel].value == &tune.floor_caustic) {
    if (!tune.floor_caustic) { //freeze the floor colors at base
      pond_caustic_phase = 0;
      apply_pond_caustics();
    }
  }
  bg_cache_dirty = 1;
}

//pond input frame: returns 1 while the tuner owns the controls
static uint8_t update_pond_tuner(void) {
  uint8_t input = players.base[0] & MASK_PLAYER_BASE_INPUT;
  uint8_t edge = input & (uint8_t)(~tune_prev_input);
  tune_prev_input = input;
  if (edge & MASK_INPUT_START) {
    tune_open ^= 1;
    if (tune_open) tune_draw();
    else tune_clear_text();
    return tune_open;
  }
  if (!tune_open) {
    if (edge & MASK_INPUT_B) enter_menu();
    return 0;
  }
  if (edge & MASK_INPUT_UP)
    tune_sel = (uint8_t)((tune_sel + TUNE_ENTRY_COUNT - 1) % TUNE_ENTRY_COUNT);
  if (edge & MASK_INPUT_DOWN)
    tune_sel = (uint8_t)((tune_sel + 1) % TUNE_ENTRY_COUNT);
  if (edge & (MASK_INPUT_LEFT | MASK_INPUT_RIGHT)) {
    const tune_entry* e = &tune_entries[tune_sel];
    uint8_t v = *e->value;
    if (edge & MASK_INPUT_RIGHT) v = (v >= e->max) ? 0 : (uint8_t)(v+1);
    else v = (v == 0) ? e->max : (uint8_t)(v-1);
    *e->value = v;
    tune_apply(tune_sel);
  }
  if (edge & (MASK_INPUT_UP|MASK_INPUT_DOWN|MASK_INPUT_LEFT|MASK_INPUT_RIGHT))
    tune_draw();
  return 1;
}



/* Dibuja una frame del juego
*/
static void render_frame(void)
{
  if (scene_defs[current_scene].kind == SCENE_KIND_POND) {
    /* The pond's layer stack, back to front — sprite passes interleave
       between background layers so fish dive under the depth veils:
       floor, caustics, shadows, deep koi, veil 2, mid koi, veil 1,
       shallow koi, surface texture, sparkles, leaves, top HUD */
    uint16_t* buf = frame_buf;
    gfx_render_bg_layer(&GFX, buf, 4, 1);            //floor (base)
    gfx_render_bg_layer(&GFX, buf, 3, 0);            //caustics
    gfx_render_fsp_pass(&GFX, buf, PRIO_SHADOW);
    gfx_render_fsp_pass(&GFX, buf, PRIO_KOI_DEEP);
    gfx_render_bg_layer(&GFX, buf, 2, 0);            //depth veil 2
    gfx_render_fsp_pass(&GFX, buf, PRIO_KOI_MID);
    gfx_render_bg_layer(&GFX, buf, 1, 0);            //depth veil 1
    gfx_render_fsp_pass(&GFX, buf, PRIO_KOI_SHALLOW);
    gfx_render_bg_layer(&GFX, buf, 0, 0);            //surface texture
    gfx_render_fsp_pass(&GFX, buf, PRIO_SPARKLE);    //contact rings
    gfx_render_hsp_pass(&GFX, buf, PRIO_SPARKLE);    //surface highlights
    gfx_render_fsp_pass(&GFX, buf, PRIO_LEAF);
    gfx_render_fsp_pass(&GFX, buf, 7);               //top full sprites
    gfx_render_hsp_pass(&GFX, buf, 7);               //hand/menu/tuner
  }
  else {
    //shmup path: backgrounds from the cache, recomposed only when dirty
    if (bg_cache_dirty) {
      gfx_render_backgrounds(&GFX, bg_cache);
      bg_cache_dirty = 0;
    }
    memcpy(frame_buf, bg_cache,
           (size_t)GFX.viewport.width * GFX.viewport.height * sizeof(uint16_t));
    gfx_render_sprites(&GFX, frame_buf);
  }
  video_cb(frame_buf, GFX.viewport.width, GFX.viewport.height,
           GFX.viewport.width << 1);
}

static void check_variables(void)
{
}

/* Renders one synthesized SFX sample for a voice; returns 0 and frees the
   voice when its envelope ends. All voices are deterministic. */
static int16_t sfx_sample(sfx_voice* v)
{
   if (v->type == SFX_NONE) return 0;
   uint32_t t = v->t++;
   int32_t out = 0;
   uint32_t len, period;
   switch (v->type) {
      case SFX_LASER: //descending square zap
         len = 4410;
         if (t >= len) { v->type = SFX_NONE; return 0; }
         period = 50 + t/16;
         out = ((t / period) & 1) ? 3000 : -3000;
         out = out * (int32_t)(len - t) / (int32_t)len;
         break;
      case SFX_BOOM: //noise burst with decay
         len = 14700;
         if (t >= len) { v->type = SFX_NONE; return 0; }
         v->seed = v->seed*1664525u + 1013904223u;
         out = (int32_t)((v->seed >> 16) & 0x3FFF) - 0x2000;
         out = out * (int32_t)(len - t) / (int32_t)len;
         break;
      case SFX_PLOP: //short low blub, pitch sinking
         len = 3300;
         if (t >= len) { v->type = SFX_NONE; return 0; }
         period = 100 + t/8;
         out = ((t / period) & 1) ? 2500 : -2500;
         out = out * (int32_t)(len - t) / (int32_t)len;
         break;
      case SFX_BLIP: //happy pickup blip
         len = 2200;
         if (t >= len) { v->type = SFX_NONE; return 0; }
         out = ((t / 33) & 1) ? 2000 : -2000;
         out = out * (int32_t)(len - t) / (int32_t)len;
         break;
      default:
         v->type = SFX_NONE;
         return 0;
   }
   return (int16_t)out;
}

static void audio_callback(void)
{
   static int16_t buf[SAMPLES_PER_FRAME*2]; //stereo interleaved
#ifdef HAVE_XMP
   if (tune.music != applied_music) music_load(tune.music);
   if (music_playing) {
      uint32_t step = vapor_step();
      uint32_t need = (uint32_t)((((uint64_t)SAMPLES_PER_FRAME*step)>>16) + 2);
      while (music_fifo_w - music_fifo_r < need) {
         int16_t chunk[512*2];
         xmp_play_buffer(ctx, chunk, sizeof(chunk), 1);
         for (uint32_t k=0; k<512; k++) {
            uint32_t slot = (music_fifo_w + k) & (MUSIC_FIFO_FRAMES-1);
            music_fifo[slot*2]   = chunk[k*2];
            music_fifo[slot*2+1] = chunk[k*2+1];
         }
         music_fifo_w += 512;
      }
      for (uint32_t i=0; i<SAMPLES_PER_FRAME; i++) {
         uint32_t idx = (music_fifo_r + (music_pos_frac>>16))
                        & (MUSIC_FIFO_FRAMES-1);
         buf[i*2]   = music_fifo[idx*2];
         buf[i*2+1] = music_fifo[idx*2+1];
         music_pos_frac += step;
      }
      music_fifo_r += music_pos_frac >> 16;
      music_pos_frac &= 0xFFFF;
   }
   else memset(buf, 0, sizeof(buf));
#else
   memset(buf, 0, sizeof(buf));
#endif
   for (unsigned ii=0; ii<SAMPLES_PER_FRAME; ii++) {
      int32_t mix = 0;
      for (uint8_t v=0; v<SFX_VOICES; v++) mix += sfx_sample(&sfx[v]);
      int32_t l = buf[2*ii] + mix;
      int32_t r = buf[2*ii+1] + mix;
      if (l > 32767) l = 32767; else if (l < -32768) l = -32768;
      if (r > 32767) r = 32767; else if (r < -32768) r = -32768;
      buf[2*ii]   = (int16_t)l;
      buf[2*ii+1] = (int16_t)r;
   }
   audio_batch_cb(buf, SAMPLES_PER_FRAME);
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
#define SAVESTATE_VERSION 12

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
  {&tl_index, sizeof(tl_index)},
  {&tl_base, sizeof(tl_base)},
  {&enemy_skin_tile, sizeof(enemy_skin_tile)},
  {&enemy_skin_frames, sizeof(enemy_skin_frames)},
  {&enemy_skin_rotates, sizeof(enemy_skin_rotates)},
  {sfx, sizeof(sfx)},
  //pond state
  {&leaves, sizeof(leaves)},
  {&koi, sizeof(koi)},
  {&pellets, sizeof(pellets)},
  {&ripples, sizeof(ripples)},
  {&hand_x, sizeof(hand_x)},
  {&hand_y, sizeof(hand_y)},
  {&hand_sprite, sizeof(hand_sprite)},
  {&pond_prev_input, sizeof(pond_prev_input)},
  {&pond_caustic_phase, sizeof(pond_caustic_phase)},
  {&pond_light_phase, sizeof(pond_light_phase)},
  {&pond_surface_phase, sizeof(pond_surface_phase)},
  {&tune, sizeof(tune)},
  {&tune_open, sizeof(tune_open)},
  {&tune_sel, sizeof(tune_sel)},
  {&tune_prev_input, sizeof(tune_prev_input)},
  {tune_slots, sizeof(tune_slots)},
  {&tune_slot_count, sizeof(tune_slot_count)},
  {&leaf_bob_amp, sizeof(leaf_bob_amp)},
  {&leaf_shadow_gap, sizeof(leaf_shadow_gap)},
  {&leaf_shadow_anim, sizeof(leaf_shadow_anim)},
  //mode and scene state
  {&game_mode, sizeof(game_mode)},
  {&menu_cursor, sizeof(menu_cursor)},
  {&current_scene, sizeof(current_scene)},
  {&menu_prev_input, sizeof(menu_prev_input)},
  {&play_prev_input, sizeof(play_prev_input)},
  {&menu_cursor_sprite, sizeof(menu_cursor_sprite)},
  //full sprite bank: OAM, free list, scroll, live palettes
  {GFX.fsp.oam, sizeof(GFX.fsp.oam)},
  {GFX.fsp.oam2, sizeof(GFX.fsp.oam2)},
  {GFX.fsp.oam3, sizeof(GFX.fsp.oam3)},
  {GFX.fsp.free_stack, sizeof(GFX.fsp.free_stack)},
  {&GFX.fsp.free_count, sizeof(GFX.fsp.free_count)},
  {&GFX.fsp.offset_x, sizeof(GFX.fsp.offset_x)},
  {&GFX.fsp.offset_y, sizeof(GFX.fsp.offset_y)},
  {GFX.fsp.palette, sizeof(GFX.fsp.palette)},
  //half sprite bank
  {GFX.hsp.oam, sizeof(GFX.hsp.oam)},
  {GFX.hsp.oam2, sizeof(GFX.hsp.oam2)},
  {GFX.hsp.oam3, sizeof(GFX.hsp.oam3)},
  {GFX.hsp.free_stack, sizeof(GFX.hsp.free_stack)},
  {&GFX.hsp.free_count, sizeof(GFX.hsp.free_count)},
  {&GFX.hsp.offset_x, sizeof(GFX.hsp.offset_x)},
  {&GFX.hsp.offset_y, sizeof(GFX.hsp.offset_y)},
  {GFX.hsp.palette, sizeof(GFX.hsp.palette)},
  //backgrounds: tilemaps and palettes mutate through the animations
  {GFX.bg[0].tilemap, sizeof(GFX.bg[0].tilemap)},
  {GFX.bg[0].offset_x, sizeof(GFX.bg[0].offset_x)},
  {GFX.bg[0].offset_y, sizeof(GFX.bg[0].offset_y)},
  {GFX.bg[0].palette, sizeof(GFX.bg[0].palette)},
  {GFX.bg[1].tilemap, sizeof(GFX.bg[1].tilemap)},
  {GFX.bg[1].offset_x, sizeof(GFX.bg[1].offset_x)},
  {GFX.bg[1].offset_y, sizeof(GFX.bg[1].offset_y)},
  {GFX.bg[1].palette, sizeof(GFX.bg[1].palette)},
  {GFX.bg[2].tilemap, sizeof(GFX.bg[2].tilemap)},
  {GFX.bg[2].offset_x, sizeof(GFX.bg[2].offset_x)},
  {GFX.bg[2].offset_y, sizeof(GFX.bg[2].offset_y)},
  {GFX.bg[2].palette, sizeof(GFX.bg[2].palette)},
  {GFX.bg[3].tilemap, sizeof(GFX.bg[3].tilemap)},
  {GFX.bg[3].offset_x, sizeof(GFX.bg[3].offset_x)},
  {GFX.bg[3].offset_y, sizeof(GFX.bg[3].offset_y)},
  {GFX.bg[3].palette, sizeof(GFX.bg[3].palette)},
  {GFX.bg[4].tilemap, sizeof(GFX.bg[4].tilemap)},
  {GFX.bg[4].offset_x, sizeof(GFX.bg[4].offset_x)},
  {GFX.bg[4].offset_y, sizeof(GFX.bg[4].offset_y)},
  {GFX.bg[4].palette, sizeof(GFX.bg[4].palette)},
  //palette derivation state (bases are re-based on scene loads)
  {fsp_palette_base, sizeof(fsp_palette_base)},
  {hsp_palette_base, sizeof(hsp_palette_base)},
  {bg_palette_base, sizeof(bg_palette_base)},
  {&fade_level, sizeof(fade_level)},
  //viewport and timeline
  {&GFX.viewport, sizeof(GFX.viewport)},
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
   /* The bg tilesets are scene-dependent and not part of the state; reload
      them for the restored scene (tilemaps stay as restored — only tiles
      and palettes come from the files), then rebuild all live palettes
      from the restored bases, fade and effect phases so the restore is
      exact. */
   if (current_scene < SCENE_COUNT) {
      for (uint8_t l=0; l<bg_layer_count; l++) {
         if (scene_defs[current_scene].gfx[l]) {
            load_gfx(scene_defs[current_scene].gfx[l],
                     l == 2 ? GFXTYPE_BG2 : l);
         }
      }
   }
   refresh_palettes();
   apply_shimmer();
   if (scene_defs[current_scene].kind == SCENE_KIND_POND) {
      apply_pond_caustics();
   }
   else {
      apply_pulse();
   }
   bg_cache_dirty = 1;
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
