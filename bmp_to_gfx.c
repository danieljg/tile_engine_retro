//Graphics preparation utility
//expects the input to be a bmp file
//produces a gfx file in tilemap format including the palette

#include "qdbmp.h"
#include <math.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>

int main( int argc, char* argv[] )
{
    BMP*    bmp;
    UCHAR   index;
    UCHAR   r, g, b;
    UINT    width, height;
    UINT    x, y;

    if ( argc != 3 )
    {
        fprintf( stderr, "Usage: %s <input file> <output file>\n", argv[ 0 ] );
        return 0;
    }

    /* Read an image file */
    bmp = BMP_ReadFile( argv[ 1 ] );
    BMP_CHECK_ERROR( stderr, -1 ); /* If an error has occurred, notify and exit */

    /* Get image's dimensions */
    width = BMP_GetWidth( bmp );
    height = BMP_GetHeight( bmp );

    int tile_size = height;

    int number_of_tiles = width/tile_size;

    uint8_t color_i=0;
    uint8_t colors_n = 0;
    uint8_t palette_elements[256];
    for ( int jj=0 ; jj<256 ; jj++ )
    {
    palette_elements[jj]=0; //null is null
    }

    fprintf(stdout,"sss\n");
    /* Open gfx file */
    int filehandler = open(argv[2],O_RDWR|O_CREAT|O_TRUNC,
                                   S_IRUSR|S_IWUSR);
    if(filehandler<0) return -1;
    /* write header */
    write(filehandler,"GFX\n",4);

    /* Iterate through all tiles */
    for ( int tile = 0 ; tile < number_of_tiles ; tile++ )
    {
      /* Iterate through all the rows */
      for ( y = 0 ; y < tile_size ; ++y )
      {
        /* Iterate through all the pixels in a row*/
        for ( x = tile_size*tile ; x < tile_size*(tile+1) ; ++x )
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
        }
      }
    }
    /* Save result */

    uint8_t buff = log2(colors_n+1);
    write(filehandler,&buff,1);
    buff = 0x01;
    write(filehandler,&buff,1);

    /* Write the palette */
    for ( color_i=0 ; color_i<(colors_n+1) ; color_i++ )
    {
      uint16_t argb_color=0x0000;
      BMP_GetPaletteColor(bmp,palette_elements[color_i], &r, &g, &b);
      //fprintf(stdout,"0x%2x\n",r);
      r=r>>3;g=g>>3;b=b>>3;
      argb_color=(r<<10)|(g<<5)|b;
      write(filehandler,&argb_color,2);
    }

    /* Write tile size and quantity */
    write(filehandler,&tile_size,1);
    buff=number_of_tiles>>16;
    write(filehandler,&buff,1);//high byte
    buff=number_of_tiles&0x00FFl;
    write(filehandler,&buff,1);//low byte

    uint8_t bpp = log2(colors_n+1);//bits per pixel
    uint8_t ppb = 8/bpp;//pixels per byte

    /* Iterate through all tiles */
    for ( int tile = 0 ; tile < number_of_tiles ; tile++ )
    {
      /* Iterate through all the rows */
      for ( y = 0 ; y < tile_size ; ++y )
      {
        /* Iterate through all the pixels in a row*/
        for ( x = tile_size*tile ; x < tile_size*(tile+1) ; x=x+ppb )
        {
            for ( uint8_t xp = 0 ; xp < ppb ; xp++ )
            {
              /* Get pixel's index */
              BMP_GetPixelIndex( bmp, x+xp, y, &index );
              /* cycle through the reduced palette colors */
              for ( color_i=0 ; color_i < (colors_n+1) ; color_i++ )
              {
                if( palette_elements[color_i]==index ) write(filehandler,&color_i,1);
              }
            }
        }
      }
    }

    fprintf(stdout,"%d\n",colors_n+1);
    fprintf(stdout,"%d %d %d %d\n", palette_elements[0],
                                    palette_elements[1],
                                    palette_elements[2],
                                    palette_elements[3]);
    fprintf(stdout,"%d\n",log2(colors_n+1));
    fflush(stdout);

    close(filehandler);

    /* Free all memory allocated for the image */
    BMP_Free( bmp );

    return 0;
}