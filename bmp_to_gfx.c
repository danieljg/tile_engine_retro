//Graphics preparation utility
//expects the input to be a bmp file
//produces a gfx file in tilemap format including the palette

#include "qdbmp.h"
#include <math.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static uint8_t mylog2 (uint8_t val) {
    if (val == 0) return 0;
    if (val == 1) return 0;
    uint8_t ret = 0;
    while (val > 1) {
        val >>= 1;
        ret++;
    }
    return ret;
}

int main( int argc, char* argv[] )
{
    BMP*	bmp;
    uint8_t	index;
    uint8_t	r, g, b;
    uint16_t	width, height;
    uint16_t	x, y;
    uint8_t target_argument_index = argc-2;
    uint8_t type_argument_index = argc-1;
    char help_msg[] = "Usage: %s <input files2> <output file> gfx_type\n\tinput file: One or more input files\n\tgfx_type: 0:Background, 1:Full_Sprites, 2:Half-Sprites\n";

    if ( argc < 4 )
    {
        fprintf( stderr, help_msg, argv[ 0 ] );
        return 0;
    }
    uint8_t tile_size;
    switch (atoi(argv[type_argument_index])) {
      case 0: //Type 0: Background Tileset
        tile_size = 16;
      case 1: //Type 1: Full Sprites
        tile_size = 16;
        break;
      case 2: //Type 2: Half Sprites
        tile_size = 8;
        break;
      default:
        fprintf(stderr, help_msg, argv[0]);
        return 0;
    }

    /* Read an image file */
    bmp = BMP_ReadFile( argv[ 1 ] );
    BMP_CHECK_ERROR( stderr, -1 ); /* If an error has occurred, notify and exit */
    fprintf(stdout,"input files:\n");
    for (uint8_t i=1; i < argc-2; i++) {
      fprintf(stdout,"\t%s\n", argv[i]);
    }

    /* Get image's dimensions */
    width = BMP_GetWidth( bmp );
    height = BMP_GetHeight( bmp );

    uint16_t number_of_tiles = (width/tile_size)*(height/tile_size);

    uint8_t color_i=0;
    uint8_t colors_n = 0;
    uint8_t palette_elements[256];
    for ( int jj=0 ; jj<256 ; jj++ )
    {
    palette_elements[jj]=0; //null is null
    }

    /* Open gfx file */
    int filehandler = open(argv[target_argument_index],O_RDWR|O_CREAT|O_TRUNC,
                                   S_IRUSR|S_IWUSR);
    if(filehandler<0) return -1;
    /* write header */
    write(filehandler,"GFX\n",4);

    /* Iterate through all the rows */
    for ( y = 0 ; y < height; y++ )
    {
      /* Iterate through all the pixels in a row */
      for ( x = 0 ; x < width ; x++ )
      {
          /* Get pixel's index */
          BMP_GetPixelIndex( bmp, x, y, &index );
          /* test if color exists in used palette */
          uint8_t color_exists=0;
          color_i=0;
          for( color_i=0 ; color_i<(colors_n+1) ; color_i++ )
          {
             if(palette_elements[color_i] == index)
             {
              color_exists=1;
              break;
             }
          }
          /* check if color exists */
          if(color_exists==0)
          {
          palette_elements[color_i]=index;
          colors_n++;
          }
          //if(index==10)fprintf(stdout,"x: %u, y: %u\n",x,y);//test for odd colors..
      }
    }
    /* Save result */

    uint8_t buff = log2(colors_n+1);
    uint8_t palette_size=colors_n+1;
fprintf(stdout,"0x %u\n",log2(colors_n+1));
fprintf(stdout,"00x %u\n",(uint8_t)log2(colors_n+1));
fprintf(stdout,"000x %u\n",mylog2(colors_n+1));
fprintf(stdout,"1x %u\n",palette_size);
fprintf(stdout,"2x %u\n",buff);
    if (palette_size==12){
      buff=4;
      palette_size=16;
fprintf(stdout,"sss\n");
    }
    else if(palette_size==8){
fprintf(stdout,"%u \n",buff);
      buff=4;
      palette_size=16;
    }
    write(filehandler,&buff,1);
    buff = 0x01;
    write(filehandler,&buff,1);

    /* Write the palette */
    for ( color_i=0 ; color_i<palette_size ; color_i++ )
    {
      uint16_t argb_color=0x0000;
      BMP_GetPaletteColor(bmp,palette_elements[color_i], &r, &g, &b);
      //fprintf(stdout,"0x%2x\n",r);
      r=r>>3;g=g>>3;b=b>>3;
      argb_color=(r<<10)|(g<<5)|b;
      if(color_i<(colors_n+1)){
        buff=argb_color>>8;
        write(filehandler,&buff,1);//high byte
        buff=argb_color&0x00FF;
        write(filehandler,&buff,1);//low byte
      }
      else{
      buff=0x0000;
      write(filehandler,&buff,1);
      write(filehandler,&buff,1);
      }
    }

    /* Write tile size and quantity */
    write(filehandler,&tile_size,1);
    buff=number_of_tiles>>8;
    write(filehandler,&buff,1);//high byte
    buff=number_of_tiles&0x00FF;
    write(filehandler,&buff,1);//low byte

    uint8_t bpp = log2(palette_size);//bits per pixel
fprintf(stdout,"palette_size: %u\n",palette_size);
fprintf(stdout,"log2(palette_size): %u\n",log2(palette_size));
    uint8_t ppb = 8/bpp;//pixels per byte

    /* Iterate through all tiles */
    for ( int tile = 0 ; tile < number_of_tiles ; tile++ )
    {
      /* Iterate through all the rows for the tile */
      for ( y = (tile/(width/tile_size))*tile_size ; y < (tile/(width/tile_size))*tile_size+tile_size ; ++y )
      {
        /* Iterate through all the pixels in a row for the tile */
        for ( x = (tile_size*tile)%width ; x < (tile_size*tile)%width+tile_size ; x=x+ppb )
        {
            buff=0x00;
            for ( uint8_t xp = 0 ; xp < ppb ; xp++ )
            {
              /* Get pixel's index */
              BMP_GetPixelIndex( bmp, x+xp, y, &index );
              /* cycle through the reduced palette colors */
              for ( color_i=0 ; color_i < (colors_n+1) ; color_i++ )
              {
                if( palette_elements[color_i] == index )
                {
                  buff=buff|(color_i<<(bpp*(ppb-xp-1)));
                  break;
                }
              }
            }
            write(filehandler,&buff,1);
        }
      }
    }

    fprintf(stdout,"number_of_tiles: %u\n",number_of_tiles);
    fprintf(stdout,"colors_n+1: %u\n",colors_n+1);
    /*for(uint8_t ii=0; ii<colors_n+1;ii++){
     fprintf(stdout,"%u %u\n", ii, palette_elements[ii]);
    }//*/
    fprintf(stdout,"log2(colors_n+1): %u\n",log2(colors_n+1));
    fprintf(stdout,"===\n");
    fflush(stdout);

    close(filehandler);

    /* Free all memory allocated for the image */
    BMP_Free( bmp );

    return 0;
}
