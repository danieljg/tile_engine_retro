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

    int colors_n = 0;
    int palette_elements[256];
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
            int color_i=0;
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

    uint8_t bpp = log2(colors_n+1);

    write(filehandler,&bpp,1);
    bpp = 0x01;
    write(filehandler,&bpp,1);

    /* Iterate through all tiles */
    for ( int tile = 0 ; tile < number_of_tiles ; tile++ )
    {
      /* Iterate through all the rows */
      for ( y = 0 ; y < tile_size ; ++y )
      {
        /* Iterate through all the pixels in a row*/
        for ( x = tile_size*tile ; x < tile_size*(tile+1) ; ++x )
        {
        }
      }
    }

    fprintf(stdout,"%d\n",colors_n+1);
    fprintf(stdout,"%d\n",log2(colors_n+1));
    fflush(stdout);

    /* Free all memory allocated for the image */
    BMP_Free( bmp );

    return 0;
}