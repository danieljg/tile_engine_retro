#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <fcntl.h>

#include "libretro.h"
#include "../gfx_engine.h"

#if defined(_3DS)
#endif

static uint16_t *frame_buf;
static struct retro_log_callback logging;
static retro_log_printf_t log_cb;

static void fallback_log(enum retro_log_level level, const char *fmt, ...)
{
   (void)level;
   va_list va;
   va_start(va, fmt);
   vfprintf(stderr, fmt, va);
   va_end(va);
}

// contadores de frames
static uint8_t frame_counter=0;
static uint8_t scroll_frame_counter=0;
static uint8_t animation_frame_counter=0;
static uint8_t scroll_has_updated_bgtm=0;
static uint16_t bg_scroll_per_step=1;
static uint16_t bg_scroll_wait_frames=1<<1;
static uint16_t animation_wait_frames=16;
// contador de scroll
uint16_t scrolling_tilemap_index=0;

//comment the following line to get nice pixel art in the debug console
#define NUMERIC_DEBUG_OUTPUT

void print_pixel_8(uint8_t value) {
  #ifdef NUMERIC_DEBUG_OUTPUT
    if      (value==0) fprintf(stdout, " ");
    else               fprintf(stdout, "%u",value);
  #else
    if      (value==0) fprintf(stdout, " ");
    else if (value==1) fprintf(stdout, ".");
    else if (value==2) fprintf(stdout, "x");
    else if (value==3) fprintf(stdout, "*");
    else if (value==4) fprintf(stdout, "+");
    else if (value==5) fprintf(stdout, "o");
    else if (value==6) fprintf(stdout, "^");
    else if (value==7) fprintf(stdout, "v");
    else               fprintf(stdout, "~%u~",value);
  #endif
}

void retro_init(void)
{
  initialize_bg();
  initialize_full_sprites();
  initialize_viewport();
  initialize_half_sprites();
  frame_buf = calloc(viewport.width * viewport.height, sizeof(uint16_t));
  FILE* file = fopen("bg0.gfx","rb");
  read_gfx_data(file, 0);
  fclose(file);
  file = fopen("fsp.gfx","rb");//Remember to close
  read_gfx_data(file, 2);
  fclose(file);
  file = fopen("hsp.gfx","rb");
  read_gfx_data(file,3);
  fclose(file);

char cwd[1024];
   if (getcwd(cwd, sizeof(cwd)) != NULL) fprintf(stdout, "Current working dir: %s\n", cwd);
}

void retro_deinit(void)
{
   //close(filehandler);
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
   float aspect = 4.0f / 3.0f;
   float sampling_rate = 30000.0f;

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

static unsigned x_coord;
static unsigned y_coord;

void retro_reset(void)
{
   x_coord = 0;
   y_coord = 0;
}

static void move_background(int8_t vel_x, int8_t vel_y) {
  viewport.x_origin=(viewport.x_origin+vel_x*bg_scroll_per_step)%(layer_tile_number_x*full_tile_size);
  viewport.y_origin=(viewport.y_origin+vel_y*bg_scroll_per_step)%(layer_tile_number_y*full_tile_size);
}

static void update_input(void)
{
  input_poll_cb();
  if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP))
  {
    move_full_sprite(0, 0, -1);
  }
  if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN))
  {
    move_full_sprite(0, 0, 1);
  }
  if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT))
  {
    move_full_sprite(0, -1, 0);
  }
  if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT))
  {
    move_full_sprite(0, 1, 0);
  }
  if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A))
  {
    move_background(1, 0);
    //moving hud
    for (uint8_t i=0; i<6; i++) move_half_sprite(i, 1, 0);
  }
  if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B))
  {
    move_background(0, 1);
    //moving hud
    for (uint8_t i=0; i<6; i++) move_half_sprite(i, 0, 1);
  }
  if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X))
  {
    move_background(0, -1);
    //moving hud
    for (uint8_t i=0; i<6; i++) move_half_sprite(i, 0, -1);
  }
  if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y))
  {
    move_background(-1, 0);
    //moving hud
    for (uint8_t i=0; i<6; i++) move_half_sprite(i, -1, 0);
  }
  if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L))
  {
    //fprintf(stdout, "L\t");
    move_full_sprite(1, -1, 0);
  }
  if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R))
  {
    //fprintf(stdout, "R\t");
    move_full_sprite(1, 1, 0);
  }
  if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2))
  {
    //fprintf(stdout, "L2\t");
    move_full_sprite(3, 0, -1);
  }
  if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2))
  {
    //fprintf(stdout, "R2\t");
    move_full_sprite(3, 0, 1);
  }
  if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L3))
  {
    //fprintf(stdout, "L3\t");
    move_full_sprite(2, -1, -1);
  }
  if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R3))
  {
    //fprintf(stdout, "R3\t");
    move_full_sprite(2, 1, 1);
  }
  if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT))
  {
    fprintf(stdout, "SELECT\t");
  }
  if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START))
  {
    fprintf(stdout, "START\t");
  }
}


/*Actualiza los primeros 8 HALF SPRITES (0 - 7) con las coordenadas del Metroid */
void update_coords(uint16_t x, uint16_t y) {
  #define ASCII0 48
  char digits[6];
  for (uint8_t i=0; i<3; i++) {
    digits[2-i] = ASCII0 + x%10;
    x=x/10;
  }
  for (uint8_t i=0; i<3; i++) {
    digits[5-i] = ASCII0 + y%10;
    y=y/10;
  }
  for (uint8_t i=0; i<6; i++) {
    set_half_sprite(i, digits[i]);
  }
}
// Actualiza las mecánicas del juego.
static void update_game() {
  /* This block de code turns the green bots to yellow or red they get too close to the metroid.
  */
  #define METROID_ID 0
  #define GREENBOT_ID 1
  int16_t met_x, met_y;
  int16_t bot_x, bot_y;
  int16_t square_total;
  met_x = fsp.oam[METROID_ID].x_pos;
  met_y = fsp.oam[METROID_ID].y_pos;
  for (uint16_t bot_id = 1; bot_id <= 3; bot_id++) {
    bot_x = fsp.oam[bot_id].x_pos;
    bot_y = fsp.oam[bot_id].y_pos;
    square_total =
      (met_x - bot_x) * (met_x - bot_x) +
      (met_y - bot_y) * (met_y - bot_y);
    if (square_total > 8100) set_full_sprite(bot_id, 0x7); // Green bot
    else if (square_total > 1600) set_full_sprite(bot_id, 0x8); // Yellow bot
    else set_full_sprite(bot_id, 0x9); //Red bot
  }
  update_coords(met_x, met_y);
}

/* Dibuja una frame del juego
*/
static void render_frame(void)
{
  uint16_t *buf    = frame_buf;
  uint16_t stride  = viewport.width; // Stride igual a ancho de viewport
  uint16_t *line   = buf;

  //TODO: get this logic stuff out of render_frame()
  frame_counter++;
  scroll_frame_counter=frame_counter%bg_scroll_wait_frames;
  animation_frame_counter=frame_counter%animation_wait_frames;

  if(scroll_frame_counter==0){
    //viewport.x_origin=(viewport.x_origin+bg_scroll_per_step)%(layer_tile_number_x*full_tile_size);
    //viewport.y_origin=(viewport.y_origin-bg_scroll_per_step)%(layer_tile_number_y*full_tile_size);
  }

  if(animation_frame_counter==0){
  bg[0].tilemap[12].tileset_index=(bg[0].tilemap[12].tileset_index+1)%6;
  ///*
    if(bg[0].tilemap[15].tileset_index>13) {
      bg[0].tilemap[15].tileset_index=8;
    }
    else {
      bg[0].tilemap[15].tileset_index++;
    }//*/
  ////*
    if(bg[0].tilemap[18].tileset_index>18) {
      bg[0].tilemap[18].tileset_index=17;
    }
    else {
      bg[0].tilemap[18].tileset_index++;
    }
  //*/
  fsp.oam[0].tileset_index=((fsp.oam[0].tileset_index)+1)%6;
  //fsp.oam[0].x_pos=((fsp.oam[0].x_pos)+1)%(layer_tile_number_x*full_tile_size);
  }

  //background rendering
  uint16_t yy_vp[bg_layer_count];
  uint16_t xx_vp[bg_layer_count];
  uint16_t tilemap_index=0;
  uint16_t tileset_index=0;
  uint8_t layer_counter=0;
  for (uint16_t yy=0; yy<viewport.height; yy++, line+=stride)
  {
    yy_vp[layer_counter]=yy+viewport.y_origin-bg[0].offset_y[yy];
    for (uint16_t xx=0; xx<viewport.width; xx++)
    {
      xx_vp[layer_counter]=xx+viewport.x_origin-bg[0].offset_x[yy];
      tilemap_index = ( (xx_vp[layer_counter]/full_tile_size)%layer_tile_number_x
                      + (yy_vp[layer_counter]/full_tile_size)*layer_tile_number_x
                      )%(layer_tile_number_x*layer_tile_number_y);
      tileset_index=bg[layer_counter].tilemap[tilemap_index].tileset_index;
      //TODO: transparency, rotation, h-flip, v-flip, double-size
      uint8_t pal_i = bg[layer_counter].tilemap[tilemap_index].palette;
      uint8_t col_i = bg[layer_counter].tileset[tileset_index]
                                       .pixel_color_index[ ((yy_vp[layer_counter]%full_tile_size)*full_tile_size
                                                            +(xx_vp[layer_counter]%full_tile_size))
                                                           %(full_tile_size*full_tile_size) ];
      line[xx] = bg[layer_counter].palette[pal_i].colors[col_i];
    }
  }

  //full sprite rendering
  for(uint16_t sprite_counter = fsp.active_number ;
               sprite_counter > 0 ; sprite_counter-- )
  {
    uint16_t current_sprite=sprite_counter-1;
    if(fsp.oam[current_sprite].disable) continue;//skips disabled sprites
    for(uint8_t jj=0; jj<full_tile_size; jj++)
    {
      uint16_t yy_pos=fsp.oam[current_sprite].y_pos;
      uint16_t yy_fsp=((uint16_t)(yy_pos+jj-viewport.y_origin+fsp.offset_y))
                      %(full_tile_size*layer_tile_number_y);
      if(yy_fsp>=(full_tile_size*vp_tile_number_y)%(full_tile_size*layer_tile_number_y)) continue;//discriminate visible lines
      line=buf+yy_fsp*stride;
      for(uint8_t ii=0; ii<full_tile_size; ii++)
      {
        uint16_t xx_pos=fsp.oam[current_sprite].x_pos;
        uint16_t xx_fsp=((uint16_t)(xx_pos+ii-viewport.x_origin+fsp.offset_x))
                        %(full_tile_size*layer_tile_number_x);
        if(xx_fsp>=(full_tile_size*vp_tile_number_x)%(full_tile_size*layer_tile_number_x)) continue;//discriminate again
        uint8_t color_index=fsp.tile[fsp.oam[current_sprite].tileset_index]
                               .pixel_color_index[jj*full_tile_size+ii];
        if(color_index==0) continue;//transparent pixels are skipped
        //TODO: semitransparency, rotation, h-flip, v-flip, double-size
        uint8_t palette_index=fsp.oam[current_sprite].palette;
        line[xx_fsp]=fsp.palette[palette_index].colors[color_index];
      }
    }
  }

  //Render half-sprites
  for(uint16_t sprite_counter = hsp.active_number ;
               sprite_counter > 0 ; sprite_counter --) {
    uint16_t current_sprite=sprite_counter-1;
    if(hsp.oam[current_sprite].disable) continue;//skips disabled sprites
    for (uint8_t jj=0; jj<half_tile_size; jj++) {
      uint16_t yy_pos=hsp.oam[current_sprite].y_pos;
      uint16_t yy_hsp=((uint16_t)(yy_pos+jj-viewport.y_origin+hsp.offset_y))%(full_tile_size*layer_tile_number_y);
      if ( yy_hsp >= (full_tile_size*vp_tile_number_y)%(full_tile_size*layer_tile_number_y)) continue;//discriminar los renglones_visibles
      line=buf+yy_hsp*stride;
      for (uint8_t ii=0;ii<half_tile_size;ii++) {//itera sobre pixeles
        uint16_t xx_pos = hsp.oam[current_sprite].x_pos;
        uint16_t xx_hsp = ((uint16_t)(xx_pos+ii-viewport.x_origin+hsp.offset_x))%(full_tile_size*layer_tile_number_x);
        if ( xx_hsp >= (full_tile_size*vp_tile_number_x)%(full_tile_size*layer_tile_number_x) ) continue;//discriminar los pixeles visibles
        uint8_t color_index=hsp.tile[hsp.oam[current_sprite].tileset_index]
                                 .pixel_color_index[jj*half_tile_size+ii];
        if (color_index==0) continue;
        //TODO: semitransparency, rotation, h-flip, v-flip, double-size
        uint8_t pal_id=hsp.oam[current_sprite].palette;
        line[xx_hsp] = hsp.palette[pal_id].colors[color_index];
      }
    }
  }

  video_cb(buf, viewport.width, viewport.height, stride << 1);
}


static void check_variables(void)
{
}

static void audio_callback(void)
{
   audio_cb(0, 0);
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
   if (type != 0x200)
      return false;
   if (num != 2)
      return false;
   return retro_load_game(NULL);
}

size_t retro_serialize_size(void)
{
   return 2;
}

bool retro_serialize(void *data_, size_t size)
{
   if (size < 2)
      return false;

   uint8_t *data = data_;
   data[0] = x_coord;
   data[1] = y_coord;
   return true;
}

bool retro_unserialize(const void *data_, size_t size)
{
   if (size < 2)
      return false;

   const uint8_t *data = data_;
   x_coord = data[0] & 31;
   y_coord = data[1] & 31;
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
