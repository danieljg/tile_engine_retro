//Tile-based graphics engine

//the engine uses 16 bit color in the format define by the following bitmasks:
#define Mask_alpha	0x8000
#define Mask_red	0x7C00
#define Mask_green	0x03E0
#define Mask_blue	0x001F
//for convenience, we define
typedef uint16_t color_16bit;
color_16bit null_color=0x0000;

//Full tiles are 16x16 in size
//half tiles are 8x8 in size
#define full_tile_size 16
#define half_tile_size 8

//The frame is built using a number of layers
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
//The (initial) origin of the viewport may be displaced
#define vp_x_origin 0
#define vp_y_origin 0

typedef struct {
 uint32_t width;
 uint32_t height;
 uint32_t x_origin;
 uint32_t y_origin;
} vp;

vp viewport;

void initialize_viewport() {
 viewport.width		= full_tile_size*vp_tile_number_x;
 viewport.height 	= full_tile_size*vp_tile_number_y;
 viewport.x_origin	= vp_x_origin;
 viewport.y_origin	= vp_y_origin;
}

//There are two background layers
#define bg_layer_count 2

//The bg layers are built using a tilemap, where each bg tile can be rotated,
//flipped either horizontally or vertically
//Number of palettes per bg palette set
#define bg_palettes_per_layer 4 // 2 bits
//Number of colors in each bg palette
#define bg_palette_color_count 16 // 4 bits
//Number of tiles in a tileset
#define bg_tileset_number 1024 // 10 bits

typedef struct {
 color_16bit color[bg_palette_color_count];
} bg_palette;

#define Mask_bg_tile_index_0	0xF0//four bits per pixel, all bits are used
#define Mask_bg_tile_index_1	0x0F//two pixels per byte

typedef struct {
 uint8_t two_pixel_color_index[full_tile_size*full_tile_size>>1]; // 128 Bytes at 16 px/tile
} bg_tile;

//background tilemap masks
#define Mask_bgtm_disable	0x8000 //bit 16
#define Mask_bgtm_v_flip	0x4000 //bit 15
#define Mask_bgtm_h_flip	0x2000 //bit 14
#define Mask_bgtm_reserved	0x1000 //bit 13
#define Mask_bgtm_palette	0X0C00 //bits 11-12
#define Mask_bgtm_index		0x03FF //bits 1-10

typedef struct {
  bg_palette palette[bg_palettes_per_layer];
  bg_tile tile[bg_tileset_number];
  uint16_t tilemap[layer_tile_number_x*layer_tile_number_y];
  uint32_t offset_x[full_tile_size*vp_tile_number_y];//TODO:Store two offsets per 32 bit value
  uint32_t offset_y[full_tile_size*vp_tile_number_y];
} bg_struct;

bg_struct bg[bg_layer_count];

void initialize_bg()
{
  uint16_t kk=0;
  for(uint8_t ll=0; ll<bg_layer_count; ll++) {
    // inicializando offset de backgrounds
    for(uint8_t jj=0; jj<full_tile_size*vp_tile_number_y; jj++){
      bg[ll].offset_x[jj] = 0;
      bg[ll].offset_y[jj] = 0;
    }
    //inicializando tilemap a 0
    for(uint8_t jj=0; jj<layer_tile_number_y; jj++) {
      for(uint8_t ii=0; ii<layer_tile_number_x; ii++) {
        bg[ll].tilemap[jj*layer_tile_number_y+ii]=0;
      }
    }
    ///*inicializando tilemap (region del viewport inicial)
    for(uint8_t jj=0; jj<layer_tile_number_y; jj++) {
      for(uint8_t ii=0; ii<layer_tile_number_x; ii++) {
        bg[ll].tilemap[jj*layer_tile_number_y+ii]=kk%300;
        kk=kk+7;
      }
    }
    //*/
  }
}

void initialize_bg_palettes()
{
 for(uint8_t kk=0;kk<bg_layer_count;kk++)
 {
  for(uint8_t jj=0;jj<bg_palettes_per_layer;jj++)
  {
   for(uint8_t ii=0;ii<bg_palette_color_count;ii++)
   {
    bg[kk].palette[jj].color[ii]=null_color;
    bg[kk].palette[jj].color[ii]=null_color;
   }
  }
 }
}

//La capa de sprites se divide en full sprites y half-sprites
//la intencion es usar los full sprites para personajes
//y half sprites para balas, score, etc
//hay 8 paletas a elegir
#define fsp_palette_number 8
#define fsp_count 32
#define fsp_palette_color_count 16
#define fsp_tileset_number 1024

typedef struct {
color_16bit color[fsp_palette_color_count];
} fsp_palette;

#define Mask_full_sprt_index_0	0xF0//four bits per pixel, all bits are used
#define Mask_full_sprt_index_1	0x0F//two pixels per byte

typedef struct {
 uint8_t two_pixel_color_index[full_tile_size*full_tile_size>>1];
} fsp_tile;

//Full sprite Object Attribute Memory bitmasks
#define Mask_fsp_oam_in_use   0x8000 //signals if slot is occupied
#define Mask_fsp_oam_disable  0x4000 //disable rendering of sprite
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
#define Mask_fsp_oam3_reserved 0xF000 //not in use
#define Mask_fsp_oam3_x_pos    0x0FFF //oversampled by 3 bits

typedef struct {
 fsp_palette palette[fsp_palette_number];
 fsp_tile tile[fsp_tileset_number]; //128 kB with 1024 tiles in the set
 uint16_t oam[fsp_count];
 uint16_t oam2[fsp_count];
 uint16_t oam3[fsp_count];
 uint32_t offset_x;//TODO:Combine offsets into one 32 bit variable
 uint32_t offset_y;
 uint32_t active_number;
} fsp_struct;

fsp_struct fsp;

/* ADD FULL SPRITE

Esta función recibe 3 argumentos:
- index: indice del sprite a utilizarpal_in
- x_pos, y_pos: coordenadas del sprite

El sprite es creado en el primer espacio disponible en la estructura de sprites. El contador de sprites es incrementado en 1
*/
//TODO: ciclar las estructuras disponibles y colocarlo en el primero disponible
uint8_t add_full_sprite(
    uint16_t sp_index,
    uint8_t pal_index,
    uint16_t x_pos, uint16_t y_pos
  ) {
  uint8_t new_id = fsp.active_number;
  fsp.oam[new_id] = (pal_index<<10) | sp_index;
  fsp.oam2[new_id] = y_pos;
  fsp.oam3[new_id] = x_pos;
  fsp.active_number++;
  return new_id;
}

void initialize_full_sprites()
{
 for(uint8_t ii=0;ii<fsp_palette_number;ii++)
 {
  for(uint8_t jj=0;jj<fsp_palette_color_count;jj++)
  {
   fsp.palette[ii].color[jj]=null_color;
  }
 }
 fsp.offset_x=0;
 fsp.offset_y=0;
 fsp.active_number=0;
}

#define hsp_palette_number 16
#define hsp_count 128
#define hsp_palette_color_count 8
#define hsp_tileset_number 1024

typedef struct {
color_16bit color[hsp_palette_color_count];
} hsp_palette;

#define Mask_half_sprite_reserved2  0x80//unused padding
#define Mask_half_sprite_index_0    0x70//three bits per pixel
#define Mask_half_sprite_reserved1  0x08//unused padding
#define Mask_half_sprite_index_1    0x07//two pixels per byte

typedef struct {
uint8_t two_pixel_color_index[half_tile_size*half_tile_size>>1];
} hsp_tile;

//half sprite Object Attribute Memory bitmasks
#define Mask_hsp_oam_in_use    0x8000 //signals if slot is occupied
#define Mask_hsp_oam_disable   0x4000 //disable rendering of sprite
#define Mask_hsp_oam_effects   0x2000 //enable rendering of sprite effects
#define Mask_hsp_oam_palette   0x1C00 //select among 8 palettes
#define Mask_hsp_oam_index     0x03FF //10 bits for tileset index
//OAM2 bitmasks
#define Mask_hsp_oam2_v_flip   0x8000 //flip the tile vertically
#define Mask_hsp_oam2_h_flip   0x4000 //flip the tile horizontally
#define Mask_hsp_oam2_rotation 0x2000 //enable 90 degree clockwise rotation
#define Mask_hsp_oam2_double   0x1000 //double size flag
#define Mask_hsp_oam2_y_pos    0x0FFF //oversampled by 3 bits
//OAM3 bitmasks
#define Mask_hsp_oam3_reserved 0xF000 //not in use
#define Mask_hsp_oam3_x_pos    0x0FFF //oversampled by 3 bits

typedef struct {
 hsp_palette palette[hsp_palette_number];
 hsp_tile tile[hsp_tileset_number];
 uint16_t oam[hsp_count];
 uint16_t oam2[hsp_count];
 uint16_t oam3[hsp_count];
 uint32_t offset_x;//TODO: combine offsets into one 32 bit qty
 uint32_t offset_y;
 uint32_t active_number;
} hsp_struct;

hsp_struct hsp;

/* ADD HALF SPRITE

Esta función recibe 3 argumentos:
- index: indice del sprite a utilizar
- x_pos, y_pos: coordenadas del sprite

El sprite es creado en el primer espacio disponible en la estructura de sprites. El contador de sprites es incrementado en 1
*/
//TODO: buscar el primer espacio disponible
void add_half_sprite(
    uint16_t sp_index,
    uint8_t pal_index,
    uint16_t x_pos, uint16_t y_pos
  ) {
  hsp.oam[hsp.active_number] = (pal_index<<10) | sp_index;
  hsp.oam2[hsp.active_number] = y_pos;
  hsp.oam3[hsp.active_number] = x_pos;
  hsp.active_number++;
}

static void inline move_full_sprite(int16_t sp_id, int8_t vel_x, int8_t vel_y) {
  uint16_t oambuff;//using local variables may be faster
  oambuff=fsp.oam2[sp_id];
  fsp.oam2[sp_id]=(oambuff&(~Mask_fsp_oam2_y_pos))|(((oambuff&Mask_fsp_oam2_y_pos)+vel_y)%(layer_tile_number_y*full_tile_size));
  oambuff=fsp.oam3[sp_id];
  fsp.oam3[sp_id]=(oambuff&(~Mask_fsp_oam3_x_pos))|(((oambuff&Mask_fsp_oam3_x_pos)+vel_x)%(layer_tile_number_x*full_tile_size));
}

static void inline full_sprite_set_pos(int16_t sp_id, int8_t pos_x, int8_t pos_y) {
  uint16_t oambuff;//using local variables may be faster
  oambuff=fsp.oam2[sp_id];
  fsp.oam2[sp_id]=(oambuff&(~Mask_fsp_oam2_y_pos))|(pos_y%(layer_tile_number_y*full_tile_size));
  oambuff=fsp.oam3[sp_id];
  fsp.oam3[sp_id]=(oambuff&(~Mask_fsp_oam3_x_pos))|(pos_x%(layer_tile_number_x*full_tile_size));
}

static void inline set_full_sprite(int16_t sp_id, int16_t sp_index) {
  uint16_t oambuff;
  oambuff=fsp.oam[sp_id];
  fsp.oam[sp_id] = (oambuff&(~Mask_fsp_oam_index))|sp_index;
}

static void inline move_half_sprite(int16_t sp_id, int8_t vel_x, int8_t vel_y) {
  uint16_t oambuff;
  oambuff=hsp.oam2[sp_id];
  hsp.oam2[sp_id]=(oambuff&(~Mask_hsp_oam2_y_pos))|(((oambuff&Mask_hsp_oam2_y_pos)+vel_y)%(layer_tile_number_y*full_tile_size));
  oambuff=hsp.oam3[sp_id];
  hsp.oam3[sp_id]=(oambuff&(~Mask_hsp_oam3_x_pos))|(((oambuff&Mask_hsp_oam3_x_pos)+vel_x)%(layer_tile_number_x*full_tile_size));
}

static void inline set_half_sprite(int16_t sp_id, int16_t sp_index) {
  uint16_t oambuff;
  oambuff=hsp.oam[sp_id];
  hsp.oam[sp_id] = (oambuff&(~Mask_hsp_oam_index))|sp_index;
}

void draw_text( char label[],
                int16_t x_pos, int16_t y_pos,
                uint8_t pal_index) {
  int16_t x_tile;
  int8_t len = strlen(label);
  for (uint8_t i = 0; i < len; i++) {
    x_tile = x_pos + i * 8;
    if (label[i] != 32) add_half_sprite(label[i], pal_index, x_tile, y_pos);
  }
}

void initialize_half_sprites()
{
  for(uint8_t ii=0;ii<hsp_palette_number;ii++)
  {
    for(uint8_t jj=0;jj<hsp_palette_color_count;jj++) {
      hsp.palette[ii].color[jj]=null_color;
    }
  }
  hsp.offset_x=0;
  hsp.offset_y=0;
  hsp.active_number=0;

  //for (uint8_t ii=0; ii<3; ii++) add_half_sprite('0', 0, 294+ii*8, 230);
  //for (uint8_t ii=0; ii<3; ii++) add_half_sprite('0', 0, 262+ii*8, 230);
  //draw_text("Now with support for...", 8, 168, 0);
  //draw_text("...multi-palette sprites! :D", 8, 184, 1);
  //draw_text("Nice! #$%&*", 8, 122, 2);
}

/* Reads graphics data from a gfx file.
*/
void read_gfx_data(FILE* file, int gfxtype) {
  uint8_t buff[4];
  uint8_t palette_size, palette_qty, colors_per_pal;
  uint8_t tile_size;
  uint16_t tile_qty, line_bytesize=0;

  // Leyendo identificador GFX (TODO: cambiar a if(buf=="GFX\n") ...)
  fread(buff,1,4,file);
  fprintf(stdout,"\n%c",buff[0]);
  fprintf(stdout,"%c",buff[1]);
  fprintf(stdout,"%c",buff[2]);
  fprintf(stdout,"%c",buff[3]);

  // Leyendo información del encabezado de la paleta.
  fread(buff,1,2,file);
  palette_size = buff[0];
  palette_qty = buff[1];
  colors_per_pal = 1 << palette_size; // 2 ^ palette_size
  fprintf(stdout,"Palette size: %d (%d colors)\n", palette_size, colors_per_pal);
  fprintf(stdout,"Palette qty: %d\n", palette_qty);

  // Leyendo datos de las paletas.
  uint16_t palette_data[colors_per_pal];
  for (uint8_t pal_i=0; pal_i<palette_qty; pal_i++) { // este ciclo itera sobre las paletas
    fprintf(stdout,"Paleta %d:\n", pal_i);
    for (uint8_t col_i=0; col_i<colors_per_pal; col_i++) { // este ciclo itera sobre los colores
      uint8_t red, green, blue;
      fread(buff,1,2,file);
      palette_data[col_i] = (buff[0] << 8) | buff[1];
      red = (palette_data[col_i]>>10) & 31;
      green = (palette_data[col_i]>>5) & 31;
      blue = palette_data[col_i] & 31;
      fprintf(stdout,"\tColor %d: %d (R:%02x G:%02x B:%02x)\n", col_i, palette_data[col_i], red, green, blue);

      if(gfxtype==0) {
        bg[0].palette[pal_i].color[col_i] = palette_data[col_i];
      }
      else if(gfxtype==1) {
        bg[1].palette[pal_i].color[col_i] = palette_data[col_i];
      }
      else if(gfxtype==2) {
        fsp.palette[pal_i].color[col_i] = palette_data[col_i];
      }
      else if(gfxtype==3) {
        hsp.palette[pal_i].color[col_i] = palette_data[col_i];
      }
    }
  }
  fprintf(stdout,"----- Palette Data Ends -----\n\n");

  // Leyendo información del encabezado de los tiles.
  //read(filebuf, buff, 3);
  fread(buff,1,3,file);
  tile_size = buff[0];
  tile_qty = (buff[1]<<8) | buff[2];
  fprintf(stdout,"Tile size: %d\n", tile_size);
  fprintf(stdout,"Tile qty: %d\n", tile_qty);
  line_bytesize = (tile_size * palette_size) >> 3;
  fprintf(stdout, "Tile size in bytes: %d\n", line_bytesize * tile_size);
  for (uint16_t tile_i=0; tile_i<tile_qty; tile_i++) {
    //fprintf(stdout,"\nTile: %d:\n", tile_i);
    for (uint8_t line_i=0; line_i<tile_size; line_i++) {
      //fprintf(stdout,"\t");
      for (uint16_t byte_i=0; byte_i < line_bytesize; byte_i++) {
        fread(buff,1,1,file);
        if (palette_size==4) {
          uint8_t pixbuffer;
          pixbuffer = buff[0];
          if(gfxtype==0) {
            bg[0].tile[tile_i].two_pixel_color_index
              [ byte_i + (line_i*line_bytesize)]=pixbuffer;
          }
          else if(gfxtype==1) {
            bg[1].tile[tile_i].two_pixel_color_index
              [ byte_i + (line_i*line_bytesize)]=pixbuffer;
          }
          else if(gfxtype==2) {
            fsp.tile[tile_i].two_pixel_color_index
               [ byte_i + (line_i*line_bytesize)]=pixbuffer;
          }
          if(gfxtype==3) {
            hsp.tile[tile_i].two_pixel_color_index
              [ byte_i + (line_i*line_bytesize)]=pixbuffer;
          }

        }
        else
        {
        fprintf(stdout,"PALETTE IS NOT 16 COLORS!\n");
        }
      }
      //fprintf(stdout,"\n");
    }
  }
  fprintf(stdout,"----- Tile Data Ends -----\n\n");
  fprintf(stdout,"*** The End ***\n\n");
}

/* Dibuja un punto directamente en el buffer de video

El código comentado pertenece a una versión anterior que usaba un apuntador al
frame buffer. Parece funcionar igual al usar el frame buffer directamente.
*/
void inline draw_point(uint16_t *viewport_buff, int16_t x, int16_t y, int16_t color) {
 //uint16_t *line = frame_buf;
 //line[viewport.width * y + x] = color;
 viewport_buff[viewport.width * y + x] = color;
}

/* Dibuja una linea usando el algoritmo de Bresenham.

Nota: El algorito funciona, pero hay algo mal aún, tengo que dibujar
manualmente el punto inicial y el final.
*/
void draw_line(uint16_t *viewport_buff, int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t color) {
 int16_t dx = x2 - x1;
 int16_t dy = y2 - y1;
 int8_t increment_diag_x = (dx >= 0) ? 1: -1;
 dx *= increment_diag_x;
 int8_t increment_diag_y = (dy >= 0) ? 1: -1;
 dy *= increment_diag_y;
 int8_t increment_ort_x;
 int8_t increment_ort_y;
 if (dx >= dy) {
   increment_ort_x = increment_diag_x;
   increment_ort_y = 0;
 }
 else {
   increment_ort_x = 0;
   increment_ort_y = increment_diag_y;
   int16_t aux = dy;
   dy = dx;
   dx = aux;
 }
 int16_t x = x1;
 int16_t y = y1;
 int16_t step_ort = 2 * dy;
 int16_t step = step_ort - dx;
 int16_t step_diag = step - dx;
 draw_point(viewport_buff, x, y, color);
 while (x != x2) {
   if (step >= 0) {
     x += increment_diag_x;
     y += increment_diag_y;
     step += step_diag;
   }
   else {
     x += increment_ort_x;
     y += increment_ort_y;
     step += step_ort;
   }
   draw_point(viewport_buff, x, y, color);
 }
 draw_point(viewport_buff, x2, y2, color);
}
