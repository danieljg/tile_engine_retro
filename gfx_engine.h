#define tile_size 16
#define half_tile_size 8

#define bg_tile_number_x 32
#define bg_tile_number_y 20

#define vp_tile_number_x 20
#define vp_tile_number_y 15

#define bg_count 2
#define sprt_count 32
#define half_sprt_count 128

#define bg_palette_count 2
#define bg_palette_color_count 63

#define sprite_palette_count 8
#define sprt_color_count 15

#define hlf_sprt_palette_count 16
#define hlf_sprt_color_count 7

typedef struct color_16bit {
  unsigned int red_5bit : 5;
  unsigned int green_5bit : 5;
  unsigned int blue_5bit : 5;
  unsigned int semi_transparency : 1;
};

color_16bit null_color;
 null_color.red=0;
 null_color.green=0;
 null_color.blue=0;
 semi_transparency=0;

typedef struct bg_palette {
 color_16bit colors[1+bg_palette_color_count];
};

typedef struct bg_palettes_array {
 bg_palette palettes[bg_palette_count];
};

bg_palettes_array bg_palettes;
bg_palettes.palettes[0].colors[0]=null_color;
bg_palettes.palettes[1].colors[0]=null_color;

typedef struct bg_color_index {
 unsigned int index   :6;//64 colors
 unsigned int padding :2;//bits disponibles para uso general
};

typedef struct bg_map_tile {
 bg_color_index pixel_color[tile_size*tile_size];
};

