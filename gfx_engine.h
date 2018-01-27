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
#define vp_tile_number_x 20
#define vp_tile_number_y 15
//The (initial) origin of the viewport may be displaced
#define vp_x_origin 0
#define vp_y_origin 0

typedef struct {
 uint16_t width;
 uint16_t height;
 uint16_t x_origin;
 uint16_t y_origin;
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
//flipped either horizontally or vertically and where one may choose between
// one of two palette sets (intended for each bg to have its own palette set)
#define bg_palette_set_count 2 // 1 bit
//Each tile can reference one of two tilesets
#define bg_tileset_count 2 // 1 bit
//Number of palettes per bg palette set
#define bg_palettes_per_set 4 // 2 bits
//Number of colors in each bg palette
#define bg_palette_color_count 16 // 4 bits
//Number of tiles in a tileset
#define bg_tileset_number 1024 // 10 bits

typedef struct {
 color_16bit colors[bg_palette_color_count];
} bg_palette;

typedef struct {
 bg_palette palettes[bg_palettes_per_set];
} bg_palette_set;

#define Mask_bg_tile_index_0	0xF0//four bits per pixel, all bits are used
#define Mask_bg_tile_index_1	0x0F//two pixels per byte

typedef struct {
 uint8_t two_pixel_color_index[full_tile_size*full_tile_size>>1]; // 128 Bytes at 16 px/tile
} bg_tile;

typedef struct {
 bg_tile tile[bg_tileset_number]; // 128 kB at 1024 tiles per tileset
} bg_tileset;

//background tilemap masks
#define Mask_bgtm_rotation	0x8000 //bit 16
#define Mask_bgtm_v_flip	0x4000 //bit 15
#define Mask_bgtm_h_flip	0x2000 //bit 14
#define Mask_bgtm_reserved	0x1000 //bit 13
#define Mask_bgtm_palette	0X0C00 //bits 11-12
#define Mask_bgtm_index		0x03FF //bits 1-10

typedef struct {
uint16_t tile_index[layer_tile_number_x*layer_tile_number_y]; //2Bytes*32*32=2kB
} bg_tilemap;

typedef struct {
  bg_palette_set palette_sets[bg_palette_set_count];
  bg_tileset tilesets[bg_tileset_count];
  bg_tilemap tilemaps[bg_layer_count];
  uint16_t offset_x[bg_layer_count];
  uint16_t offset_y[bg_layer_count];
} bg_struct;

bg_struct bg;

void initialize_bg()
{
  for(uint8_t ll=0; ll<bg_layer_count; ll++) {
    // inicializando offset de backgrounds
    bg.offset_x[ll] = 0;
    bg.offset_y[ll] = 0;
    //inicializando tilemap (region del viewport inicial)
    uint16_t kk=0;
    for(uint8_t jj=0; jj<vp_tile_number_y; jj++) {
      for(uint8_t ii=0; ii<vp_tile_number_x; ii++) {
        bg.tilemaps[ll].tile_index[jj*layer_tile_number_y+ii]=kk;
        //bg.tilemaps[ll].tile_index[jj*layer_tile_number_y+ii]=7;
        kk++;
      }
    }
  }
}

void initialize_bg_palettes()
{
 for(uint8_t ii=0;ii<bg_palettes_per_set;ii++)
 {
  for(uint8_t jj=0;jj<bg_palette_color_count;jj++)
  {
   for(uint8_t kk=0;kk<bg_palette_set_count;kk++){
    bg.palette_sets[kk].palettes[ii].colors[jj]=null_color;
    bg.palette_sets[kk].palettes[ii].colors[jj]=null_color;
   }
  }
 }
}


// TO DO: Escribir funciones de inicialización restantes


//La capa de sprites se divide en full sprites y half-sprites
//la intencion es usar los full sprites para personajes
//y half sprites para balas, score, etc
//hay 8 paletas a elegir
#define fsp_palette_number 8
#define fsp_count 32
#define fsp_palette_color_count 16
#define fsp_tileset_number 1024

typedef struct {
color_16bit colors[fsp_palette_color_count];
} fsp_palette;

#define Mask_full_sprt_index_0	0xF0//four bits per pixel, all bits are used
#define Mask_full_sprt_index_1	0x0F//two pixels per byte

typedef struct {
 uint8_t two_pixel_color_index[full_tile_size*full_tile_size>>1];
} fsp_tile;

//Full sprite Object Attribute Memory bitmasks
#define Mask_fsp_oam_rotation 0x8000 //bit  16
#define Mask_fsp_oam_v_flip   0x4000 //bit  15
#define Mask_fsp_oam_h_flip   0x2000 //bit  14
#define Mask_fsp_oam_palette  0x1C00 //bits 11-13
#define Mask_fsp_oam_index    0x03FF //bits 1-10
//OAM2 memory: geometry and toggles
#define Mask_fsp_oam2_disable    0x80000000 //bit  32
#define Mask_fsp_oam2_reserved2  0x7E000000 //bits 26-31
#define Mask_fsp_oam2_y_pos      0x01FF0000 //bits 17-25
#define Mask_fsp_oam2_reserved1  0x0000FE00 //bits 10-16
#define Mask_fsp_oam2_x_pos      0x000001FF //bits 1-9
//TODO: there's no position in OAM for the tileset, no double-size option
//TODO: refactor code so that x position of pixels is sampled at 1/8 pixel resolution

typedef struct {
 fsp_palette palettes[fsp_palette_number];
 fsp_tile tile[fsp_tileset_number]; //128 kB with 1024 tiles in the set
 uint16_t oam[fsp_count];
 uint32_t oam2[fsp_count];
 uint16_t offset_x;
 uint16_t offset_y;
 uint8_t active_number;
} fsp_struct;

fsp_struct fsp;

/* ADD FULL SPRITE

Esta función recibe 3 argumentos:
- index: indice del sprite a utilizar
- x_pos, y_pos: coordenadas del sprite

El sprite es creado en el primer espacio disponible en la estructura de sprites. El contador de sprites es incrementado en 1
*/
void add_full_sprite(uint16_t sp_index, uint16_t x_pos, uint16_t y_pos) {
  fsp.oam[fsp.active_number] = 0x0000 | sp_index;
  fsp.oam2[fsp.active_number] = x_pos|(y_pos<<16);
  fsp.active_number++;
}

void initialize_full_sprites()
{
 for(uint8_t ii=0;ii<fsp_palette_number;ii++)
 {
  for(uint8_t jj=0;jj<fsp_palette_color_count;jj++)
  {
   fsp.palettes[ii].colors[jj]=null_color;
  }
 }
 fsp.offset_x=0;
 fsp.offset_y=0;
 fsp.active_number=0;
 add_full_sprite(0,128,128);
 //fsp.oam2[0]=0x00800000|0x00000080;//wtf is this sorcery...
 //fsp.oam[0]=0x0000;
 add_full_sprite(7,32,32);
 add_full_sprite(8,48,32);
 add_full_sprite(9,64,32);
 add_full_sprite(12,120,32); 
}

#define hsp_palette_number 16
#define hsp_count 128
#define hsp_palette_color_count 8
#define hsp_tileset_number 1024

typedef struct {
color_16bit colors[hsp_palette_color_count];
} hsp_palette;

#define Mask_half_sprite_reserved2  0x80//unused padding
#define Mask_half_sprite_index_0    0x70//three bits per pixel
#define Mask_half_sprite_reserved1  0x08//unused padding
#define Mask_half_sprite_index_1    0x07//two pixels per byte

typedef struct {
uint8_t two_pixel_color_index[half_tile_size*half_tile_size>>1];
} hsp_tile;

//half sprite Object Attribute Memory bitmasks
#define Mask_hsp_oam_rotation 0x8000
#define Mask_hsp_oam_v_flip   0x4000
#define Mask_hsp_oam_h_flip   0x2000
#define Mask_hsp_oam_palette  0x1C00
#define Mask_hsp_oam_index    0x03FF
//OAM2 memory: geometry and toggles
#define Mask_hsp_oam2_disable    0x80000000 //bit 32
#define Mask_hsp_oam2_reserved2  0x7E000000 //bits 26-31
#define Mask_hsp_oam2_y_pos      0x01FF0000 //bits 17-25
#define Mask_hsp_oam2_reserved1  0x0000FE00 //bits 10-16
#define Mask_hsp_oam2_x_pos      0x000001FF //bits 1-9

typedef struct {
 hsp_palette palettes[hsp_palette_number];
 hsp_tile tile[hsp_tileset_number];
 uint16_t oam[hsp_count];
 uint32_t oam2[hsp_count];
 uint16_t offset_x;
 uint16_t offset_y;
 uint8_t active_number;
} hsp_struct;

hsp_struct hsp;

/* ADD HALF SPRITE

Esta función recibe 3 argumentos:
- index: indice del sprite a utilizar
- x_pos, y_pos: coordenadas del sprite

El sprite es creado en el primer espacio disponible en la estructura de sprites. El contador de sprites es incrementado en 1
*/
void add_half_sprite(uint16_t sp_index, uint16_t x_pos, uint16_t y_pos) {
  hsp.oam[hsp.active_number] = 0x0000 | sp_index;
  hsp.oam2[hsp.active_number] = x_pos|(y_pos<<16);
  hsp.active_number++;
}

static void move_full_sprite(int16_t sp_id, int8_t vel_x, int8_t vel_y) {
  fsp.oam2[sp_id]=(fsp.oam2[sp_id]&(~Mask_fsp_oam2_y_pos))|(((((fsp.oam2[sp_id]&Mask_fsp_oam2_y_pos)>>16)+vel_y)%(layer_tile_number_y*full_tile_size))<<16);
  fsp.oam2[sp_id]=(fsp.oam2[sp_id]&(~Mask_fsp_oam2_x_pos))|(((fsp.oam2[sp_id]&Mask_fsp_oam2_x_pos)+vel_x)%(layer_tile_number_x*full_tile_size));
}

static void set_full_sprite(int16_t sp_id, int16_t sp_index) {
  fsp.oam[sp_id] = (fsp.oam[sp_id]&(~Mask_fsp_oam_index))|sp_index;
}

static void move_half_sprite(int16_t sp_id, int8_t vel_x, int8_t vel_y) {
  hsp.oam2[sp_id]=(hsp.oam2[sp_id]&(~Mask_hsp_oam2_y_pos))|(((((hsp.oam2[sp_id]&Mask_hsp_oam2_y_pos)>>16)+vel_y)%(layer_tile_number_y*full_tile_size))<<16);
  hsp.oam2[sp_id]=(hsp.oam2[sp_id]&(~Mask_hsp_oam2_x_pos))|(((hsp.oam2[sp_id]&Mask_hsp_oam2_x_pos)+vel_x)%(layer_tile_number_x*full_tile_size));
}

static void set_half_sprite(int16_t sp_id, int16_t sp_index) {
  hsp.oam[sp_id] = (hsp.oam[sp_id]&(~Mask_hsp_oam_index))|sp_index;
}

void draw_text(char label[], int16_t x_pos, int16_t y_pos) {
  int16_t x_tile;
  int8_t len = strlen(label);
  for (uint8_t i = 0; i < len; i++) {
    x_tile = x_pos + i * 8;
    if (label[i] != 32) add_half_sprite(label[i], x_tile, y_pos);
  }
}

void initialize_half_sprites()
{
  for(uint8_t ii=0;ii<hsp_palette_number;ii++)
  {
    for(uint8_t jj=0;jj<hsp_palette_color_count;jj++) {
      hsp.palettes[ii].colors[jj]=null_color;
    }
  }
  hsp.offset_x=0;
  hsp.offset_y=0;
  hsp.active_number=0;

  for (uint8_t ii=0; ii<3; ii++) add_half_sprite('0', 294+ii*8, 230);
  for (uint8_t ii=0; ii<3; ii++) add_half_sprite('0', 262+ii*8, 230);
  draw_text("  The font sprites are now indexed", 8, 168);
  draw_text("in ASCII format! :D", 8, 184);
  draw_text("Nice! #$%&*", 8, 122 );
}

/* Dibuja un punto directamente en el buffer de video

El código comentado pertenece a una versión anterior que usaba un apuntador al
frame buffer. Parece funcionar igual al usar el frame buffer directamente.
*/
void draw_point(uint16_t *viewport_buff, int16_t x, int16_t y, int16_t color) {
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

/* Console data print functions
*/
void print_pixel_4(uint8_t value) {
  if (value==0) fprintf(stdout, " ");
  if (value==1) fprintf(stdout, "x");
  if (value==2) fprintf(stdout, "\\");
  if (value==3) fprintf(stdout, "X");
}

/* Reads graphics data from a gfx file.
*/
void read_gfx_data(FILE* file, int gfxtype) {
  uint8_t buff[4];
  uint8_t palette_size, palette_qty, colors_per_pal;
  uint8_t tile_size;
  uint16_t tile_qty, line_bytesize=0;

  /*
  //go to end of file
  fseek(file, 0, SEEK_END);

  //check the size of the file
  off_t filesize=ftell(file);

  //go to start of file
  fseek(file, 0, SEEK_SET);

  //allocate buffer
  filebuf=malloc(filesize);
  //if(!buffer) goto exit;

  off_t bytes_read = fread(filebuf,1,filesize,file);
  //if(size!=bytes_read) goto exit;
  //*/


  // Leyendo identificador GFX (TODO: cambiar a if(buf=="GFX\n") ...)
  //read(filebuf, buff, 4);
  fread(buff,1,4,file);
  fprintf(stdout,"\n%c",buff[0]);
  fprintf(stdout,"%c",buff[1]);
  fprintf(stdout,"%c",buff[2]);
  fprintf(stdout,"%c",buff[3]);

  // Leyendo información del encabezado de la paleta.
  //read(filebuf, buff, 2);
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
      //read(filebuf, buff, 2);
      fread(buff,1,2,file);
      palette_data[col_i] = (buff[0] << 8) | buff[1];
      red = (palette_data[col_i]>>10) & 31;
      green = (palette_data[col_i]>>5) & 31;
      blue = palette_data[col_i] & 31;
      fprintf(stdout,"\tColor %d: %d (R:%02x G:%02x B:%02x)\n", col_i, palette_data[col_i], red, green, blue);

      if(gfxtype==0) {
        bg.palette_sets[0].palettes[pal_i].colors[col_i] = palette_data[col_i];
      }
      else if(gfxtype==1) {
        bg.palette_sets[1].palettes[pal_i].colors[col_i] = palette_data[col_i];
      }
      else if(gfxtype==2) {
        fsp.palettes[pal_i].colors[col_i] = palette_data[col_i];
      }
      else if(gfxtype==3) {
        hsp.palettes[pal_i].colors[col_i] = palette_data[col_i];
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
        //read(filebuf, buff, 1);
        fread(buff,1,1,file);
        /* El siguiente bloque de código imprime los numeros si el tamaño de paleta es igual a 2 (creado para funcionar con el tileset de ejemplo).
        */
        if (palette_size==2) {//TODO change this...
          uint8_t pixbuffer;
          pixbuffer = buff[0];
          /*
          print_pixel_4((pixbuffer>>6)&3);
          print_pixel_4((pixbuffer>>4)&3);
          print_pixel_4((pixbuffer>>2)&3);
          print_pixel_4(pixbuffer&3);
          //*/
          uint8_t palettebuffer=0x00;
          palettebuffer=(pixbuffer>>6)&0x03;
          palettebuffer=(palettebuffer<<4)|((pixbuffer>>4)&0x03);
          hsp.tile[tile_i].two_pixel_color_index
            [ (byte_i<<1) + (line_i*line_bytesize<<1)]=palettebuffer;
          palettebuffer=0x00;
          palettebuffer=(pixbuffer>>2)&0x03;
          palettebuffer=(palettebuffer<<4)|(pixbuffer&0x03);
          if(gfxtype==3) {
          hsp.tile[tile_i].two_pixel_color_index
            [ (byte_i<<1) + ((line_i*line_bytesize<<1)+1)]=palettebuffer;
          }
        }
        else if (palette_size==4) {
          uint8_t pixbuffer;
          pixbuffer = buff[0];
          /*
          print_pixel_8(pixbuffer>>4);
          print_pixel_8(pixbuffer&0x0F);
          //*/

          if(gfxtype==0) {
            bg.tilesets[0].tile[tile_i].two_pixel_color_index
              [ byte_i + (line_i*line_bytesize)]=pixbuffer;
          }
          else if(gfxtype==1) {
            bg.tilesets[1].tile[tile_i].two_pixel_color_index
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
      }
      //fprintf(stdout,"\n");
    }
  }
  fprintf(stdout,"----- Tile Data Ends -----\n\n");
  fprintf(stdout,"*** The End ***\n\n");
  //free(filebuf);
}
