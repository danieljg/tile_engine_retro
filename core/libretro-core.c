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
  initialize_hlf_sprt_palettes();
  frame_buf = calloc(viewport.width * viewport.height, sizeof(uint16_t));
  int filehandler = open("space_16x16_tilesheet.gfx",O_RDONLY);
  read_gfx_data(filehandler, 0);
  close(filehandler);
  //filehandler = open("sr388_invader.gfx",O_RDONLY);//Remember to close
  filehandler = open("fsp.gfx",O_RDONLY);//Remember to close
  read_gfx_data(filehandler, 2);
  close(filehandler);
  //filehandler = open("font_numbers_8x8.gfx",O_RDONLY);
  //read_gfx_data(filehandler);
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

static void move_sprite(int16_t sp_id, int8_t vel_x, int8_t vel_y) {
  fsp.oam2[sp_id]=(fsp.oam2[sp_id]&(~Mask_fsp_oam2_y_pos))|(((((fsp.oam2[sp_id]&Mask_fsp_oam2_y_pos)>>16)+vel_y)%(layer_tile_number_y*full_tile_size))<<16);
  fsp.oam2[sp_id]=(fsp.oam2[sp_id]&(~Mask_fsp_oam2_x_pos))|(((fsp.oam2[sp_id]&Mask_fsp_oam2_x_pos)+vel_x)%(layer_tile_number_x*full_tile_size));
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
    move_sprite(0, 0, -1);
  }
  if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN))
  {
    move_sprite(0, 0, 1);
  }
  if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT))
  {
    move_sprite(0, -1, 0);
  }
  if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT))
  {
    move_sprite(0, 1, 0);
  }
  if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A))
  {
    move_background(1, 0);
  }
  if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B))
  {
    move_background(0, 1);
  }
  if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X))
  {
    move_background(0, -1);
  }
  if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y))
  {
    move_background(-1, 0);
  }
  if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L))
  {
    //fprintf(stdout, "L\t");
    move_sprite(1, -1, 0);
  }
  if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R))
  {
    //fprintf(stdout, "R\t");
    move_sprite(1, 1, 0);
  }
  if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2))
  {
    //fprintf(stdout, "L2\t");
    move_sprite(3, 0, -1);
  }
  if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2))
  {
    //fprintf(stdout, "R2\t");
    move_sprite(3, 0, 1);
  }
  if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L3))
  {
    //fprintf(stdout, "L3\t");
    move_sprite(2, -1, -1);
  }
  if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R3))
  {
    //fprintf(stdout, "R3\t");
    move_sprite(2, 1, 1);
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

/* Actualiza las mecánicas del juego.
*/
static void update_game() {

}

/* Dibuja una frame del juego
*/
static void render_frame(void)
{
  uint16_t *buf    = frame_buf;
  uint16_t stride  = viewport.width; // Stride igual a ancho de viewport
  uint16_t *line   = buf;

  //inicializando la linea de tiles a la derecha del viewport para scroll
  if ( ((viewport.x_origin-bg.offset_x[0])%full_tile_size==0) ) {
    if (scroll_has_updated_bgtm==0) {
      for(uint8_t jj=0; jj<vp_tile_number_y; jj++) {
        bg.tilemaps[0].tile_index[ jj*layer_tile_number_y
                                  + ( vp_tile_number_x  + (viewport.x_origin-bg.offset_x[0])/full_tile_size)
                                    %layer_tile_number_x ]
                                  =scrolling_tilemap_index;
        //scrolling_tilemap_index=(scrolling_tilemap_index+7)%300;
        scrolling_tilemap_index=(scrolling_tilemap_index+1)%300;
        //TODO: do something with the palette of the newly added tiles... it seems a bit periodic, obviously
      }
      scroll_has_updated_bgtm=1;
    }
  }
  else {
    scroll_has_updated_bgtm=0;
  }

  frame_counter++;
  scroll_frame_counter=frame_counter%bg_scroll_wait_frames;
  animation_frame_counter=frame_counter%animation_wait_frames;

  if(scroll_frame_counter==0){
    //viewport.x_origin=(viewport.x_origin+bg_scroll_per_step)%(layer_tile_number_x*full_tile_size);
    //viewport.y_origin=(viewport.y_origin-bg_scroll_per_step)%(layer_tile_number_y*full_tile_size);
  }

  if(animation_frame_counter==0){
  bg.tilemaps[0].tile_index[12]=(bg.tilemaps[0].tile_index[12]+1)%6;
  ///*
    if(bg.tilemaps[0].tile_index[15]>13) {
      bg.tilemaps[0].tile_index[15]=8;
    }
    else {
      bg.tilemaps[0].tile_index[15]++;
    }//*/
  ////*
    if(bg.tilemaps[0].tile_index[18]>18) {
      bg.tilemaps[0].tile_index[18]=17;
    }
    else {
      bg.tilemaps[0].tile_index[18]++;
    }
  //*/
  fsp.oam[0]=(fsp.oam[0]&(~Mask_fsp_oam_index))|(((fsp.oam[0]&Mask_fsp_oam_index)+1)%6);
  //fsp.oam2[0]=(fsp.oam2[0]&(~Mask_fsp_oam2_x_pos))|(((fsp.oam2[0]&Mask_fsp_oam2_x_pos)+1)%(layer_tile_number_x*full_tile_size));
  }

  //background rendering
  uint16_t yy_vp=0;
  uint16_t xx_vp=0;
  uint8_t twopixdata=0;
  uint16_t tilemap_index=0;
  uint16_t tileset_index=0;
  for (uint16_t yy=0; yy<viewport.height; yy++, line+=stride) {
    yy_vp=yy+viewport.y_origin-bg.offset_y[0];
    for (uint16_t x2=0; x2<viewport.width; x2+=2) {
      //process first pixel
      xx_vp=x2+viewport.x_origin-bg.offset_x[0];
      tilemap_index= ( (xx_vp/full_tile_size)%layer_tile_number_x + (yy_vp/full_tile_size)*layer_tile_number_x )
                     %(layer_tile_number_x*layer_tile_number_y);
      tilemap_index=bg.tilemaps[0].tile_index[tilemap_index];
      tileset_index=tilemap_index&Mask_bgtm_index;
      //todo: introduce tilemap palette data, rotation, flip, etc
      twopixdata = bg.tilesets[0].tile[tileset_index]
                     .two_pixel_color_index[(( (yy_vp%full_tile_size)*full_tile_size+(xx_vp%full_tile_size))>>1)
                                            %(full_tile_size*full_tile_size)];
      if (xx_vp%2==0) {//verificamos si el pixel es par o impar
        line[x2]=bg.palette_sets[0].palettes[0].colors[twopixdata>>4];
        line[x2+1]=bg.palette_sets[0].palettes[0].colors[twopixdata&0x0F];
      }//el caso impar es mas complicado, requiere tomar dos bytes distintos
      else {
        line[x2]=bg.palette_sets[0].palettes[0].colors[twopixdata&0x0F];
        xx_vp++;
        if (xx_vp%16!=0) {//si el segundo pixel no es el inicio de un tile, es mas facil
          twopixdata = bg.tilesets[0].tile[tileset_index]
                         .two_pixel_color_index[(( (yy_vp%full_tile_size)*full_tile_size+(xx_vp%full_tile_size))>>1)
                                                %(full_tile_size*full_tile_size)];
          line[x2+1]=bg.palette_sets[0].palettes[0].colors[twopixdata>>4];
        }
        else {//si el segundo pixel es el inicio de un tile, hay que buscar el indice en el mapa
          tilemap_index=( (xx_vp/full_tile_size)%layer_tile_number_x + (yy_vp/full_tile_size)*layer_tile_number_x )
                       %(layer_tile_number_x*layer_tile_number_y);
          tilemap_index=bg.tilemaps[0].tile_index[tilemap_index];
          tileset_index=tilemap_index&Mask_bgtm_index;
          //todo: introduce tilemap palette data, rotation, flip, etc
          twopixdata = bg.tilesets[0]
                         .tile[ tileset_index ]
                         .two_pixel_color_index[(( (yy_vp%full_tile_size)*full_tile_size+(xx_vp%full_tile_size))>>1)
                                     %(full_tile_size*full_tile_size)];
          line[x2+1]=bg.palette_sets[0].palettes[0].colors[twopixdata>>4];
        }
      }
    }
  }
  //draw_point(frame_buf, 3, 10, 0x7c00); //probando función draw_point
  //draw_line(frame_buf, 104, 32, 135, 100, 0x7fff); //probando función draw_line
  //draw_line(frame_buf, 135, 100, 10, 60, 0x7c00);
  //draw_line(frame_buf, 10, 60, 104, 32,0x03e0);
  //draw_line(frame_buf, 15, 200, 16, 160, 0x0c00);

  //full sprite rendering
  for(uint16_t sprite_counter = fsp.active_number ;
               sprite_counter > 0 ; sprite_counter-- ) {
    uint16_t current_sprite=sprite_counter-1;
    if(fsp.oam2[current_sprite]>=Mask_fsp_oam2_disable) continue;//skips disabled sprites
    for (uint8_t jj=0; jj<full_tile_size; jj++ ) {//itera sobre renglones
      uint16_t yy_pos=(fsp.oam2[current_sprite]&Mask_fsp_oam2_y_pos)>>16;
      uint16_t yy_fsp=((uint16_t)(yy_pos+jj-viewport.y_origin+fsp.offset_y))%(full_tile_size*layer_tile_number_y);
      if ( yy_fsp >= (full_tile_size*vp_tile_number_y)%(full_tile_size*layer_tile_number_y)) continue;//discriminar los renglones visibles
      line=buf+yy_fsp*stride;
      for (uint8_t ii=0;ii<full_tile_size;ii++) {//itera sobre pixeles
        uint16_t xx_pos=fsp.oam2[current_sprite]&Mask_fsp_oam2_x_pos;
        uint16_t xx_fsp=((uint16_t)(xx_pos+ii-viewport.x_origin+fsp.offset_x))%(full_tile_size*layer_tile_number_x);
        if ( xx_fsp >= (full_tile_size*vp_tile_number_x)%(full_tile_size*layer_tile_number_x) ) continue;//discriminar los pixeles visibles
        uint8_t twopixdata=fsp.tile[fsp.oam[current_sprite]
                                     &Mask_fsp_oam_index]
                              .two_pixel_color_index
                               [(jj*full_tile_size+ii)>>1];
        if (ii%2==0) {
        twopixdata=twopixdata>>4;
        }
        else {
        twopixdata=twopixdata&0x0F;
        }
        if (twopixdata==0) continue;
        line[xx_fsp] = fsp.palettes[0]
                          .colors[twopixdata];
      }
    }
  }

  //Render half-sprites

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