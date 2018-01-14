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

// Usada para desplazamiento de la pantalla
static int skip;

void print_pixel(unsigned value) {
  if (value==0) fprintf(stdout, " ");
  if (value==1) fprintf(stdout, "x");
  if (value==2) fprintf(stdout, "\\");
  if (value==3) fprintf(stdout, "X");
}

void read_gfx_data() {
  uint8_t buff[4];
  uint8_t palette_size, palette_qty, colors_per_pal;
  uint8_t tile_size, tile_qty, line_bytesize;

  int filehandler;
  filehandler = open("bg_stars.gfx",O_RDONLY);

  // Leyendo identificador GFX (TODO: cambiar a if(buf=="GFX\n") ...)
  read(filehandler, buff, 4);
  fprintf(stdout,"\n%c",buff[0]);
  fprintf(stdout,"%c",buff[1]);
  fprintf(stdout,"%c",buff[2]);
  fprintf(stdout,"%c",buff[3]);

  // Leyendo información del encabezado de la paleta.
  read(filehandler, buff, 2);
  palette_size = buff[0];
  palette_qty = buff[1];
  colors_per_pal = 1 << palette_size; // 2 ^ palette_size
  fprintf(stdout,"Palette size: %d (%d colors)\n", palette_size, colors_per_pal);
  fprintf(stdout,"Palette qty: %d\n", palette_qty);
  /* Leyendo datos de las paletas.
  Nota: Por el momento da por hecho que existe una unica paleta. Solo guarda un espacio en memoria para la paleta y la sobreescribe con la ultima paleta leida.
  */
  uint16_t palette_data[colors_per_pal]; // Debe de ser un arreglo bidimencional para soportar mas de una paleta.
  for (uint8_t pal_i=0; pal_i<palette_qty; pal_i++) { // este ciclo itera sobre las paletas
    fprintf(stdout,"Paleta %d:\n", pal_i);
    for (uint8_t col_i=0; col_i<colors_per_pal; col_i++) { // este ciclo itera sobre los colores
      uint8_t red, green, blue;
      read(filehandler, buff, 2);
      palette_data[col_i] = (buff[0] << 8) | buff[1];
      red = (palette_data[col_i]>>10) & 31;
      green = (palette_data[col_i]>>5) & 31;
      blue = palette_data[col_i] & 31;
      fprintf(stdout,"\tColor %d: %d (R:%02x G:%02x B:%02x)\n", col_i, palette_data[col_i], red, green, blue);
      bg.palette_sets[0].palettes[0].colors[col_i] = palette_data[col_i];
    }
  }
  fprintf(stdout,"----- Palette Data Ends -----\n\n");

  // Leyendo información del encabezado de los tiles.
  read(filehandler, buff, 3);
  tile_size = buff[0];
  tile_qty = (buff[1]<<8) | buff[2];
  fprintf(stdout,"Tile size: %d\n", tile_size);
  fprintf(stdout,"Tile qty: %d\n", tile_qty);
  line_bytesize = (tile_size * palette_size) >> 3;
  fprintf(stdout, "Tile size in bytes: %d\n", line_bytesize * tile_size);
  for (uint16_t tile_i=0; tile_i<tile_qty; tile_i++) {
    fprintf(stdout,"\nTile: %d:\n", tile_i);
    for (uint8_t line_i=0; line_i<tile_size; line_i++) {
      fprintf(stdout,"\t");
      for (uint16_t byte_i=0; byte_i < line_bytesize; byte_i++) {
        read(filehandler, buff, 1);
        /* El siguiente bloque de código imprime los numeros si el tamaño de paleta es igual a 2 (creado para funcionar con el tileset de ejemplo).
        */
        if (palette_size==2) {
          uint8_t pixbuffer;
          pixbuffer = buff[0];
          print_pixel((pixbuffer>>6)&3);
          print_pixel((pixbuffer>>4)&3);
          print_pixel((pixbuffer>>2)&3);
          print_pixel(pixbuffer&3);
          uint8_t palettebuffer=0x00;
          palettebuffer=(pixbuffer>>6)&0x03;
          palettebuffer=(palettebuffer<<4)|((pixbuffer>>4)&0x03);
          hlf_sprt_tiles.tile[tile_i].two_pixel_color_index
            [ (byte_i<<1) + (line_i*line_bytesize<<1)]=palettebuffer;
          palettebuffer=0x00;
          palettebuffer=(pixbuffer>>2)&0x03;
          palettebuffer=(palettebuffer<<4)|(pixbuffer&0x03);
          hlf_sprt_tiles.tile[tile_i].two_pixel_color_index
            [ (byte_i<<1) + ((line_i*line_bytesize<<1)+1)]=palettebuffer;
        }
        else if (palette_size==4) {
          uint8_t pixbuffer;
          pixbuffer = buff[0];
          bg.tilesets[0].tile[tile_i].two_pixel_color_index
            [ (byte_i<<1) + ((line_i*line_bytesize<<1))]=pixbuffer;
        }
      }
      fprintf(stdout,"\n");
    }
  }
  fprintf(stdout,"----- Tile Data Ends -----\n\n");
  fprintf(stdout,"*** The End ***\n\n");
}

void retro_init(void)
{
   initialize_viewport();
   initialize_hlf_sprt_palettes();
   frame_buf = calloc(viewport.width * viewport.height, sizeof(uint16_t));
   //filehandler = open("font_numbers_8x8.gfx",O_RDONLY);
   //read_gfx_data(filehandler);
   read_gfx_data();
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
   info->library_name     = "TestCore";
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
static int mouse_rel_x;
static int mouse_rel_y;

void retro_reset(void)
{
   x_coord = 0;
   y_coord = 0;
}

static void update_input(void)
{
   input_poll_cb();
   if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP))
   {
      /* stub */
   }
}

/* Dibuja una frame del juego
*/
static void render_frame(void)
{
   uint16_t *buf    = frame_buf;
   uint16_t stride  = viewport.width; // Stride igual a ancho de viewport
   uint16_t color_r = (0x15<<10)|(0x05<<5)|(0x05); // rojo-ladrillo
   uint16_t color_g = (0x04<<10)|(0x04<<5)|(0x08); // gris-cemento
   uint16_t *line   = buf;

   /* Este ciclo dibuja la pantalla linea por linea
   */

   uint8_t scale_shift = 2; // 0: No escalar
   uint16_t x_abs;
   uint16_t y_abs;

   y_coord=2;

   for (uint16_t y = 0; y < viewport.height; y++, line += stride)
   {
      y_abs = (y - y_coord) >> scale_shift;
      uint8_t index_y = y_abs % 4 == 3;
      for (uint16_t x = 0; x < viewport.width; x++)
      {
         x_abs = (x - x_coord) >> scale_shift;
         uint8_t index_x; // Cierto si hay cemento
         if ((y_abs >> 2) % 2 == 1) index_x = x_abs % 8 == 3;
         else index_x = x_abs % 8 == 7;
         // Asignar color a pixel
         if (index_y || index_x) line[x] = color_g; // gris si hay cemento
         else line[x] = color_r; // rojo-ladrillo si no hay cemento
         //Pintar...
         uint8_t pixbuf=0x00;
         pixbuf=hlf_sprt_tiles.tile[x>>3].two_pixel_color_index[((x>>1)%4)+((y%8)<<2)];
         if( (x%2) == 0 ) pixbuf=(pixbuf&Mask_hlf_sprt_index_0)>>4;
         else pixbuf=pixbuf&Mask_hlf_sprt_index_1;
         if(pixbuf!=0) line[x]=hlf_sprt_palettes[0].colors[pixbuf];
      }
   }

   for (unsigned y = mouse_rel_y - 5; y <= mouse_rel_y + 5; y++)
      for (unsigned x = mouse_rel_x - 5; x <= mouse_rel_x + 5; x++)
         buf[y * stride + x] = 0xff;

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

   render_frame();
   // Desplazamiento de la pantalla
   skip++;
   if(skip==2){
   x_coord+=1;
   y_coord+=1;
   skip=0;
   }
   if(x_coord==64)x_coord=0;
   if(y_coord==64)y_coord=0;

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
