#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <fcntl.h>

#include "libretro.h"

static uint16_t *frame_buf;
static struct retro_log_callback logging;
static retro_log_printf_t log_cb;

int filehandler;

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

void retro_init(void)
{
   frame_buf = calloc(320 * 240, sizeof(uint16_t));
   filehandler = open("output.gfx",O_RDONLY);
   char buff[4];
   read(filehandler,buff,4);
   fprintf(stdout,"%d\n",buff[0]);
   fprintf(stdout,"%d\n",buff[1]);
   fprintf(stdout,"%d\n",buff[2]);
   fprintf(stdout,"%d\n",buff[3]);
   read(filehandler,buff,1);   fprintf(stdout,"%d\n",buff[0]);
   read(filehandler,buff,1);   fprintf(stdout,"%d\n",buff[0]);
}

void retro_deinit(void)
{
   close(filehandler);
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
      .base_width   = 320,
      .base_height  = 240,
      .max_width    = 320,
      .max_height   = 240,
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

/* Dibuja una pared de ladrillos
*/
static void render_bricks(void)
{
   uint16_t *buf    = frame_buf;
   unsigned stride  = 320; // Stride igual a ancho de viewport
   uint16_t color_r = (0x15<<10)|(0x08<<5)|(0x08); // rojo-ladrillo
   uint16_t color_g = (0x08<<10)|(0x08<<5)|(0x08); // gris-cemento
   uint16_t *line   = buf;

   /* Este ciclo dibuja la pantalla linea por linea
   */

   uint8_t scale_shift = 2; // 0: No escalar
   unsigned x_abs;
   unsigned y_abs;

   y_coord=2;

   for (unsigned y = 0; y < 240; y++, line += stride)
   {
      y_abs = (y - y_coord) >> scale_shift;
      unsigned index_y = y_abs % 4 == 3;
      for (unsigned x = 0; x < 320; x++)
      {
         x_abs = (x - x_coord) >> scale_shift;
         unsigned index_x; // Cierto si hay cemento
         if ((y_abs >> 2) % 2 == 1) index_x = x_abs % 8 == 3;
         else index_x = x_abs % 8 == 7;
         // Asignar color a pixel
         if (index_y || index_x) line[x] = color_g; // gris si hay cemento
         else line[x] = color_r; // rojo-ladrillo si no hay cemento
      }
   }

   for (unsigned y = mouse_rel_y - 5; y <= mouse_rel_y + 5; y++)
      for (unsigned x = mouse_rel_x - 5; x <= mouse_rel_x + 5; x++)
         buf[y * stride + x] = 0xff;

   video_cb(buf, 320, 240, stride << 1);
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

   render_bricks();
   // Desplazamiento de la pantalla
   skip++;
   if(skip==1){
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
