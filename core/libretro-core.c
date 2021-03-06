#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <fcntl.h>
#include <xmp.h>

#include "libretro.h"
#include "../gfx_engine.h"
//#include "../events.h"
#include "../game.h"

#if defined(_3DS)
#endif

static uint16_t *frame_buf;
static struct retro_log_callback logging;
static retro_log_printf_t log_cb;

static xmp_context ctx;
static struct xmp_module_info mi;

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

// Audio variables
static FILE* audiofile;
static unsigned phase;
static uint8_t makesound=0;

void retro_init(void)
{
  initialize_viewport();
  initialize_bg();
  initialize_full_sprites();
  initialize_half_sprites();
  frame_buf = calloc(viewport.width * viewport.height, sizeof(uint16_t));
  FILE* file = fopen("bg1.gfx","rb");
  read_gfx_data(file, 0);
  fclose(file);
  file = fopen("bg1.gfx","rb");
  read_gfx_data(file, 1);
  fclose(file);
  file = fopen("fsp.gfx","rb");//Remember to close
  read_gfx_data(file, 2);
  fclose(file);
  file = fopen("hsp.gfx","rb");
  read_gfx_data(file,3);
  fclose(file);
  fprintf(stdout,"=============================\n");
  fprintf(stdout,"%u ------\n",bg[0].tilemap[464]);
  char cwd[1024];
  if (getcwd(cwd, sizeof(cwd)) != NULL)
    fprintf(stdout, "Current working dir: %s\n", cwd);
  //initialize_game();
  initialize_game();
  default_scores();
  fprintf(stdout, "mark1s\n");
  ctx = xmp_create_context();
  audiofile = fopen("airwolf2.mod","rb");
  //if( xmp_load_module_from_file(ctx,&file,135862) == 0 ){
  char* audiofilepath="/home/daniel/build/tileengineretro_test/tile_engine_retro/core/test.xm";
  if( xmp_load_module(ctx,audiofilepath) == 0 ){
    fprintf(stdout, "mark2\n");
    if (xmp_start_player(ctx, 31920, XMP_FORMAT_MONO) == 0) {
      xmp_get_module_info(ctx, &mi);
      display_audiomodule_info(&mi);
    }
  } 
  fprintf(stdout, "mark3\n");
}

void retro_deinit(void)
{
   xmp_end_player(ctx);
   xmp_release_module(ctx);
   xmp_free_context(ctx);
   fclose(audiofile);
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

static unsigned x_coord;
static unsigned y_coord;

void retro_reset(void)
{
   x_coord = 0;
   y_coord = 0;
}

static void move_viewport(int8_t vel_x, int8_t vel_y) {
  viewport.x_origin=(viewport.x_origin+vel_x*bg_scroll_per_step)%(layer_tile_number_x*full_tile_size);
  viewport.y_origin=(viewport.y_origin+vel_y*bg_scroll_per_step)%(layer_tile_number_y*full_tile_size);
}

static void update_input(void)
{
  input_poll_cb();
  // Cleaning players' input states
  for (uint8_t player_id=0; player_id < game.player_count; player_id++){
    game.players[player_id].input_state = 0x00;
  }
  // Checking START button for al players (including idle players)
  for (uint8_t player_id=0; player_id < MAX_PLAYERS; player_id++) {
    if (input_state_cb(player_id, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START)) {
      game.players[player_id].input_state = MASK_INPUT_START;
    }
  }
  // Updating other buttons for all active players
  for (uint8_t player_id=0; player_id < game.player_count; player_id++){
    if (input_state_cb(player_id, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP))
    {
      game.players[player_id].input_state =
        game.players[player_id].input_state | MASK_INPUT_UP;
    }
    if (input_state_cb(player_id, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN))
    {
      game.players[player_id].input_state =
        game.players[player_id].input_state | MASK_INPUT_DOWN;
    }
    if (input_state_cb(player_id, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT))
    {
      game.players[player_id].input_state =
        game.players[player_id].input_state | MASK_INPUT_LEFT;
    }
    if (input_state_cb(player_id, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT))
    {
      game.players[player_id].input_state =
        game.players[player_id].input_state | MASK_INPUT_RIGHT;
    }
    if (input_state_cb(player_id, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A))
    {
      game.players[player_id].input_state =
        game.players[player_id].input_state | MASK_INPUT_A;
    }
    if (input_state_cb(player_id, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B))
    {
      game.players[player_id].input_state =
        game.players[player_id].input_state | MASK_INPUT_B;
    }
    if (input_state_cb(player_id, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X))
    {
      game.players[player_id].input_state =
        game.players[player_id].input_state | MASK_INPUT_C;
    }
    if (input_state_cb(player_id, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y))
    {

    }
  }
}


/*Actualiza los primeros 8 HALF SPRITES (0 - 7) con las coordenadas de entrada */

/* Actualiza las mecánicas del juego.
*/
static void update_game() {
  //update_entities();
  for (uint8_t i=0; i<game.player_count; i++) {
    update_player(&game.players[i]);
  }
  //update_hud();

  frame_counter++;
  scroll_frame_counter=frame_counter%bg_scroll_wait_frames;
  animation_frame_counter=frame_counter%animation_wait_frames;

 ///*
  if(scroll_frame_counter==0){
    for(uint32_t yy=0;yy<(vp_tile_number_y*full_tile_size);yy++){
      bg[0].offset_x[yy]--;
      if(yy%3==0){
      bg[1].offset_x[yy]-=2;
      }
      else{
      bg[1].offset_x[yy]-=(yy%3)+1;
      }
    }
    //viewport.x_origin=(viewport.x_origin+bg_scroll_per_step)%(layer_tile_number_x*full_tile_size);
    //viewport.y_origin=(viewport.y_origin-bg_scroll_per_step)%(layer_tile_number_y*full_tile_size);
  }//*/

  if(animation_frame_counter==0){
    update_animations();
    /*
    if((bg[0].tilemap[12]&Mask_bgtm_index)>5) {
      bg[0].tilemap[12]=bg[0].tilemap[12]&(~Mask_bgtm_index);
    }
    else {
      bg[0].tilemap[12]++;
    }
    if((bg[0].tilemap[15]&Mask_bgtm_index)>13) {
      bg[0].tilemap[15]=(bg[0].tilemap[15]&(~Mask_bgtm_index))|8;
    }
    else {
      bg[0].tilemap[15]++;
    }
    if((bg[0].tilemap[18]&Mask_bgtm_index)>18) {
      bg[0].tilemap[18]=(bg[0].tilemap[18]&(~Mask_bgtm_index))|17;
    }
    else {
      bg[0].tilemap[18]++;
    }//*/
    //fsp.oam[0]=(fsp.oam[0]&(~Mask_fsp_oam_index))|(((fsp.oam[0]&Mask_fsp_oam_index)+1)%6);
    //fsp.oam3[0]=(fsp.oam3[0]&(~Mask_fsp_oam3_x_pos))|(((fsp.oam3[0]&Mask_fsp_oam3_x_pos)+1)%(layer_tile_number_x*full_tile_size));
  }

}

static color_16bit inline average_colors(color_16bit color1, color_16bit color2) {
  return ( ( ( (color1&Mask_red)   + (color2&Mask_red)   )>>1 )&Mask_red   ) |
         ( ( ( (color1&Mask_green) + (color2&Mask_green) )>>1 )&Mask_green ) |
         ( ( ( (color1&Mask_blue)  + (color2&Mask_blue)  )>>1 )&Mask_blue  );
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


  //full sprite rendering
  for(uint16_t sprite_counter = fsp.active_number ;
               sprite_counter > 0 ; sprite_counter-- ) {
    uint16_t current_sprite=sprite_counter-1;
    if(Mask_fsp_oam_enable & (~fsp.oam[current_sprite])) continue;//skips disabled sprites
    uint8_t pal_id=(fsp.oam[current_sprite]&Mask_fsp_oam_palette)>>10;
    uint16_t xx_pos=fsp.oam3[current_sprite]&Mask_fsp_oam3_x_pos;
    uint16_t yy_pos=fsp.oam2[current_sprite]&Mask_fsp_oam2_y_pos;
    uint32_t xx_fsp_max = (full_tile_size*vp_tile_number_x)%(full_tile_size*layer_tile_number_x);//TODO: test effect of uint16_t here
    uint32_t yy_fsp_max = (full_tile_size*vp_tile_number_y)%(full_tile_size*layer_tile_number_y);
    for (uint8_t jj=0; jj<full_tile_size; jj++ ) {//itera sobre renglones
      uint16_t yy_fsp=((uint16_t)(yy_pos+jj-viewport.y_origin+fsp.offset_y))%(full_tile_size*layer_tile_number_y);
      if ( yy_fsp >= yy_fsp_max) continue;//discriminar los renglones visibles
      line=buf+yy_fsp*stride;
      for (uint8_t ii=0;ii<full_tile_size;ii+=8) {//itera sobre pixeles de 8 en 8
        uint16_t xx_fsp=(xx_pos+ii-viewport.x_origin+fsp.offset_x)%(full_tile_size*layer_tile_number_x);
        uint32_t eightpixdata = fsp.tile[fsp.oam[current_sprite]&Mask_fsp_oam_index]
                                   .eight_pixel_color_index[(jj*full_tile_size+ii)>>3];
        for(uint8_t kk=0;kk<8;kk++) {// aqui renderiza 8 pixeles
          if ( (xx_fsp+kk)>=xx_fsp_max) continue;//discriminar los pixeles visibles
          uint8_t pixdata = (uint8_t) (eightpixdata>>(4*(7-(kk%8))));//mmmhmm...
          pixdata = pixdata & 0x0F;
          if(pixdata==0) continue;
          colordata=fsp.palette[pal_id].color[pixdata];
          if(colordata<0x8000){
            line[xx_fsp+kk]=colordata;//don't mask when it's not needed
          }
          else{
            clearbuf = line[xx_fsp+kk];
            line[xx_fsp+kk] = average_colors(colordata,clearbuf);
          }
        }
      }
    }
  }

  //Render half-sprites
  for(uint16_t sprite_counter = hsp.active_number ;
               sprite_counter > 0 ; sprite_counter --) {
    uint16_t current_sprite=sprite_counter-1;
    if(Mask_hsp_oam_enable & (~hsp.oam[current_sprite])) continue;//skips disabled sprites
    uint8_t pal_id=(hsp.oam[current_sprite]&Mask_hsp_oam_palette)>>10;
    uint16_t xx_pos = hsp.oam3[current_sprite]&Mask_hsp_oam3_x_pos;
    uint16_t yy_pos = hsp.oam2[current_sprite]&Mask_hsp_oam2_y_pos;
    uint32_t xx_hsp_max = (full_tile_size*vp_tile_number_x)%(full_tile_size*layer_tile_number_x);
    uint32_t yy_hsp_max = (full_tile_size*vp_tile_number_y)%(full_tile_size*layer_tile_number_y);
    for (uint8_t jj=0; jj<half_tile_size; jj++) {
      uint16_t yy_hsp=((uint16_t)(yy_pos+jj-viewport.y_origin+hsp.offset_y))%(full_tile_size*layer_tile_number_y);
      if ( yy_hsp >= yy_hsp_max) continue;//discriminar los renglones_visibles
      line=buf+yy_hsp*stride;
      uint32_t offset = jj*half_tile_size;
      for (uint8_t ii=0;ii<half_tile_size;ii++) {//itera sobre pixeles
        uint16_t xx_hsp = ((uint16_t)(xx_pos+ii-viewport.x_origin+hsp.offset_x))%(full_tile_size*layer_tile_number_x);
        if ( xx_hsp >= xx_hsp_max ) continue;//discriminar los pixeles visibles
        uint32_t eightpixdata=hsp.tile[hsp.oam[current_sprite]&Mask_hsp_oam_index]
                                 .eight_pixel_color_index[(offset+ii)>>3];
        uint8_t pixdata = (uint8_t) (eightpixdata>>(4*(7-(ii%8))));//mmmhmm...
        pixdata = pixdata & 0x0F;
        if (pixdata==0) continue;
        colordata = hsp.palette[pal_id].color[pixdata];
        if(colordata<0x8000){
          line[xx_hsp] = colordata;
        }
        else{
          clearbuf = line[xx_hsp];
          line[xx_hsp] = average_colors(colordata,clearbuf);
        }
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
   struct xmp_frame_info fi;
   int16_t audiobuff[532];
   xmp_play_buffer(ctx,&audiobuff,532*2,1);
   //xmp_play_frame(ctx);
   //xmp_get_frame_info(ctx,&fi);
   //fprintf(stdout,"%i\n",fi.buffer_size);
   //fprintf(stdout,"%i\n",fi.frame_time);
   for(unsigned ii=0;ii<532;ii++,phase++) {
      //int16_t val = 0x800 * sinf(2.0f * M_PI * phase * 300.0f / 30000.0f);
      //audio_cb(val, val);
      audio_cb(audiobuff[ii], audiobuff[ii]);
   }
   //audio_cb=(0,0);
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
