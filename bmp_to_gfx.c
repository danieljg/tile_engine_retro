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
#include <string.h>
#include <errno.h>

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
    BMP*	bmp[16];
    FILE*       clrfile[16];
    uint8_t	index, buff;
    uint8_t     number_of_inputs;
    uint8_t	r, g, b;
    uint16_t	width[16], height[16];
    uint16_t	x, y;
    uint16_t    tiles_number=0;
    uint16_t    number_of_tiles[16];
    uint8_t     target_argument_index = argc-2;
    uint8_t     type_argument_index = argc-1;

    for (uint8_t ii=0;ii<16;ii++){
      clrfile[ii]=NULL;
      number_of_tiles[ii]=0;
      width[ii]=0;
      height[ii]=0;
    }

    char help_msg[] = "Usage: %s <input files> <output file> gfx_type\n\t<input files>: One or more input files\n\tgfx_type: 0:Background, 1:Full_Sprites, 2:Half-Sprites\n";

    if ( argc < 4 )
    {
        fprintf( stderr, help_msg, argv[ 0 ] );
        return 0;
    }
    uint8_t tile_size;
    switch (atoi(argv[type_argument_index])) {
      case 0: //Type 0: Background Tileset
        tile_size = 16;
        break;
      case 1: //Type 1: Full Sprites
        tile_size = 16;
        break;
      case 2: //Type 2: Half Sprites TODO also check
        tile_size = 8;
        break;
      default:
        fprintf(stderr, help_msg, argv[0]);
        return 0;
    }

    number_of_inputs=argc-3;

    // Open gfx file
    int filehandler = open(argv[target_argument_index],O_RDWR|O_CREAT|O_TRUNC,
                                                       S_IRUSR|S_IWUSR        );
    if(filehandler<0) return -1;
    // write header
    write(filehandler,"GFX\n",4);

    uint8_t  palette_number=0;
    uint16_t tile_number=0;
    //cycle through input files
    for (uint8_t ii=0; ii < number_of_inputs; ii++)
    {
      fprintf(stdout, "Pre-processing input file:");
      fprintf(stdout, "\t%s\n",argv[ii+1]);
      //Read image file
      bmp[ii] = BMP_ReadFile(argv[ii+1]);
      BMP_CHECK_ERROR(stderr, -1);
      //Get image size
      width[ii] = BMP_GetWidth( bmp[ii] );
      height[ii] = BMP_GetHeight( bmp[ii] );
      //Calculate tile number
      number_of_tiles[ii] = (width[ii]/tile_size)*(height[ii]/tile_size);
      //test if this is a pure palette file or tileset file
      if(width[ii]==4&&height[ii]==4){  //(number_of_tiles[ii]==0)
      //this is a palette file
      palette_number++;
      }
      else{
      //this is a tileset file
      tiles_number=tiles_number+number_of_tiles[ii];
      }
    }
    if(palette_number==0){
      palette_number=1;
    }
    else{
      //gotta open the *.clr files
      for(uint8_t ii=0;ii<number_of_inputs;ii++)
      {
      if(number_of_tiles[ii]==0)
      {
        char buf[1024];
        strcpy(buf,argv[ii+1]);
        char clr[1024]=".clr";
        strcat(buf,clr);
        clrfile[ii]=fopen(buf,"r");
if(clrfile[ii]==NULL)printf("Oh dear! %s\n", strerror(errno));
      }
      }
    }

    fprintf(stdout, "Done with preprocessing\n");

    //Write palette data
    //just set it to 16 and be done with it
    buff=4;
    uint8_t palette_size=16;
    write(filehandler,&buff,1);//write bits per pixel
    buff = palette_number;
    write(filehandler,&buff,1);//write number of palettes

   //cycle again through input files
   for (uint8_t ii=0; ii < number_of_inputs; ii++)
   {
     fprintf(stdout,"Processing input file:\t%s...",argv[ii+1]);
     //check if this is a palette file
     if( (number_of_tiles[ii]==0) || ( (number_of_inputs==1) ) )
     {
       for ( uint8_t jj=0 ; jj<16 ; jj++ )
       {
         uint16_t argb_color=0x0000;
         //in the case of a pure palette file, we check the .clr file to set transparency
         if(number_of_tiles[ii]==0)
         {
           char str[1024];
           fgets(str,1,clrfile[ii]);
           if(str=="1")
           {
             argb_color=0x8000;
           }
         }
         BMP_GetPaletteColor(bmp[ii],jj, &r, &g, &b);
         r=r>>3;g=g>>3;b=b>>3;
         argb_color+=(r<<10)|(g<<5)|b;
         buff=argb_color>>8;
         write(filehandler,&buff,1);//high byte
         buff=argb_color&0x00FF;
         write(filehandler,&buff,1);//low byte
       }
       fprintf(stdout,"\tdone\n");
     }
     else
     {
       fprintf(stdout,"\tnot a palette file...\n");
     }
   }

   //closing clearfiles
   for (uint8_t ii=0; ii<16;ii++){
     if(!(clrfile[ii]==NULL)){
       fclose(clrfile[ii]);
     }
   }

   fprintf(stdout, "Done with processing palettes... on to tilesets\n");

   // Write tile size and quantity
   write(filehandler,&tile_size,1);
   buff=tiles_number>>8;
   write(filehandler,&buff,1);//high byte
   buff=tiles_number&0x00FF;
   write(filehandler,&buff,1);//low byte
   uint8_t bpp = mylog2(palette_size);//bits per pixel
   uint8_t ppb = 8/bpp;//pixels per byte

   for (uint8_t ii=0; ii < number_of_inputs; ii++) {
     fprintf(stdout, "Processing input file:");
     fprintf(stdout, "\t%s... ",argv[ii+1]);

     if( !(number_of_tiles[ii]==0) )
     {

       /* Iterate through all tiles */
       for ( int tile = 0 ; tile < number_of_tiles[ii] ; tile++ )
       {
         /* Iterate through all the rows for the tile */
         for ( y = (tile/(width[ii]/tile_size))*tile_size ; y < (tile/(width[ii]/tile_size))*tile_size+tile_size ; ++y )
         {
           /* Iterate through all the pixels in a row for the tile */
           for ( x = (tile_size*tile)%width[ii] ; x < (tile_size*tile)%width[ii]+tile_size ; x=x+ppb )
           {
             buff=0x00;
             for ( uint8_t xp = 0 ; xp < ppb ; xp++ )
             {
               /* Get pixel's index */
               BMP_GetPixelIndex( bmp[ii], x+xp, y, &index );
               buff=buff|(index<<(bpp*(ppb-xp-1)));
             }
             write(filehandler,&buff,1);
           }
         }
       }
       fprintf(stdout,"done\n");
     }
     else
     {
       fprintf(stdout,"not a tileset file\n");
     }


  }

    fprintf(stdout,"number of tiles: %u\n",tiles_number);
    /*for(uint8_t ii=0; ii<colors_n+1;ii++){
     fprintf(stdout,"%u %u\n", ii, palette_elements[ii]);
    }//*/
    fprintf(stdout,"===\n");
    fflush(stdout);

    close(filehandler);

    /* Free all memory allocated for the image */
    for (uint8_t ii=0; ii<number_of_inputs; ii++)
    {
    BMP_Free( bmp[ii] );
    }

    return 0;
}
