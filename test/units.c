/* Unit tests for the pure parts of the engine and game layer:
   bit-pack accessors, AABB, color math, tile pixel packing, the GFX and
   MAP parsers (round-tripped through a temp file) and the OAM free list.
   Build and run via `make test`. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "../gfx_engine.h"
#include "../game2.h"

static int checks = 0, failures = 0;
#define CHECK(cond) do { \
    checks++; \
    if (!(cond)) { \
      failures++; \
      fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    } \
  } while (0)

static void test_body_packing(void)
{
  uint32_t x = 0, y = 0;
  body_set_pos(&x, 300<<3);
  CHECK(body_get_pos(&x) == (300<<3));
  body_set_vel(&x, -3);
  CHECK(body_get_vel(&x) == -3);
  CHECK(body_get_pos(&x) == (300<<3)); //vel write must not touch pos
  body_set_vel(&x, 127);
  CHECK(body_get_vel(&x) == 127);
  body_set_vel(&x, -128);
  CHECK(body_get_vel(&x) == -128);
  //position wraps at 512 px (4096 units)
  body_set_pos(&x, 4095);
  body_set_vel(&x, 2);
  body_set_vel(&y, 0);
  body_update(&x, &y);
  CHECK(body_get_pos(&x) == 1);
}

static void test_aabb(void)
{
  uint32_t ax = 0, ay = 0, bx = 0, by = 0;
  uint16_t da = (10<<8) | 10, db = (10<<8) | 10;
  body_set_pos(&ax, 100<<3); body_set_pos(&ay, 100<<3);
  body_set_pos(&bx, 105<<3); body_set_pos(&by, 105<<3);
  CHECK(aabb_hit(ax, ay, da, bx, by, db));   //overlapping
  body_set_pos(&bx, 110<<3); body_set_pos(&by, 100<<3);
  CHECK(!aabb_hit(ax, ay, da, bx, by, db));  //touching edges: exclusive
  body_set_pos(&bx, 200<<3);
  CHECK(!aabb_hit(ax, ay, da, bx, by, db));  //disjoint
  CHECK(aabb_hit(ax, ay, da, ax, ay, da));   //self
}

static void test_average_colors(void)
{
  //red 31 + red 1 -> red 16; channels independent; alpha stripped
  color_16bit a = (color_16bit)(31<<10), b = (color_16bit)(1<<10);
  CHECK(average_colors(a, b) == (16<<10));
  CHECK(average_colors(0x7FFF, 0x0000) == ((15<<10)|(15<<5)|15));
  CHECK(average_colors(0x8000|31, 31) == 31); //alpha bit ignored and gone
  CHECK((average_colors(0xFFFF, 0xFFFF) & 0x8000) == 0);
}

static void test_tile_pixels(void)
{
  uint32_t tile16[32];
  memset(tile16, 0, sizeof(tile16));
  tile_set_pixel(tile16, 16, 0, 0, 0xF);
  tile_set_pixel(tile16, 16, 15, 15, 0xA);
  tile_set_pixel(tile16, 16, 7, 3, 0x5);
  CHECK(tile_get_pixel(tile16, 16, 0, 0) == 0xF);
  CHECK(tile_get_pixel(tile16, 16, 15, 15) == 0xA);
  CHECK(tile_get_pixel(tile16, 16, 7, 3) == 0x5);
  CHECK(tile_get_pixel(tile16, 16, 1, 0) == 0);
  //leftmost pixel sits in the high nibble of the first word
  CHECK((tile16[0] >> 28) == 0xF);
}

/* Builds a tiny 1-palette 1-tile GFX blob in a temp file, loads it, and
   verifies palette, tile data and the pre-computed transform variants. */
static void test_gfx_roundtrip(void)
{
  FILE* f = tmpfile();
  CHECK(f != NULL);
  if (!f) return;
  fwrite("GFX\n", 1, 4, f);
  fputc(4, f);  //4 bpp
  fputc(1, f);  //1 palette
  for (int c = 0; c < 16; c++) { //palette: color i = i<<10 | i
    uint16_t col = (uint16_t)((c << 10) | c);
    fputc(col >> 8, f); fputc(col & 0xFF, f);
  }
  fputc(16, f); //tile size
  fputc(0, f); fputc(1, f); //1 tile
  //tile: pixel (x,y) = (x + y) & 0xF
  for (int y = 0; y < 16; y++)
    for (int x = 0; x < 16; x += 2)
      fputc((((x + y) & 0xF) << 4) | ((x + 1 + y) & 0xF), f);
  rewind(f);
  memset(&GFX.fsp, 0, sizeof(GFX.fsp));
  read_gfx_data(&GFX, f, 2);
  fclose(f);
  CHECK(GFX.fsp.palette[0].color[5] == ((5 << 10) | 5));
  int ok_n = 1, ok_h = 1, ok_r = 1, ok_rh = 1;
  const uint32_t* tn = (const uint32_t*)GFX.fsp.tile;
  const uint32_t* th = (const uint32_t*)GFX.fsp.tile_h;
  const uint32_t* tr = (const uint32_t*)GFX.fsp.tile_r;
  const uint32_t* trh = (const uint32_t*)GFX.fsp.tile_rh;
  for (int y = 0; y < 16; y++)
    for (int x = 0; x < 16; x++) {
      uint8_t v = (uint8_t)((x + y) & 0xF);
      if (tile_get_pixel(tn, 16, (uint8_t)x, (uint8_t)y) != v) ok_n = 0;
      //mirror: dst(15-x, y) == src(x, y)
      if (tile_get_pixel(th, 16, (uint8_t)(15 - x), (uint8_t)y) != v) ok_h = 0;
      //rotate 90 CW: dst(15-y, x) == src(x, y)
      if (tile_get_pixel(tr, 16, (uint8_t)(15 - y), (uint8_t)x) != v) ok_r = 0;
      //rotate 90 CW then mirror == transpose
      if (tile_get_pixel(trh, 16, (uint8_t)y, (uint8_t)x) != v) ok_rh = 0;
    }
  CHECK(ok_n); CHECK(ok_h); CHECK(ok_r); CHECK(ok_rh);
}

static void test_map_roundtrip(void)
{
  FILE* f = tmpfile();
  CHECK(f != NULL);
  if (!f) return;
  fwrite("MAP\n", 1, 4, f);
  fputc(32, f); fputc(32, f);
  for (int i = 0; i < 1024; i++) {
    uint16_t v = (uint16_t)((i * 7) & 0x3FF);
    fputc(v >> 8, f); fputc(v & 0xFF, f);
  }
  rewind(f);
  read_map_data(&GFX, f, 0);
  fclose(f);
  int ok = 1;
  for (int i = 0; i < 1024; i++)
    if (GFX.bg[0].tilemap[i] != ((i * 7) & 0x3FF)) ok = 0;
  CHECK(ok);
}

static void test_free_list(void)
{
  clear_all_fsp(&GFX);
  uint8_t first = add_fsp(&GFX, 1, 0, 10, 10);
  CHECK(first == 0); //slot 0 pops first
  for (int i = 1; i < fsp_count; i++) {
    CHECK(add_fsp(&GFX, 1, 0, 10, 10) == i);
  }
  CHECK(add_fsp(&GFX, 1, 0, 10, 10) == fsp_count); //full
  delete_fsp(&GFX, 7);
  delete_fsp(&GFX, 7); //double free must be a no-op
  CHECK(GFX.fsp.free_count == 1);
  CHECK(add_fsp(&GFX, 1, 0, 10, 10) == 7); //LIFO reuse
  clear_all_fsp(&GFX);
  CHECK(GFX.fsp.free_count == fsp_count);
}

static void test_animation_word(void)
{
  clear_all_fsp(&GFX);
  uint8_t sp = add_fsp(&GFX, 12, 1, 50, 50);
  uint32_t anim = 0;
  animation_init(&anim, 3, 12, sp);
  CHECK(animation_sprite(&anim) == sp);
  //3 frames: tile cycles 12 -> 13 -> 14 -> 12
  animation_advance(&anim);
  CHECK((GFX.fsp.oam[sp] & Mask_fsp_oam_index) == 13);
  animation_advance(&anim);
  CHECK((GFX.fsp.oam[sp] & Mask_fsp_oam_index) == 14);
  animation_advance(&anim);
  CHECK((GFX.fsp.oam[sp] & Mask_fsp_oam_index) == 12);
}

static void test_blending(void)
{
  //identity coefficients pass one side through untouched
  set_blend(&GFX, 16, 0);
  CHECK(gfx_blend_colors(&GFX, 0x7FFF, 0x0000) == 0x7FFF);
  set_blend(&GFX, 0, 16);
  CHECK(gfx_blend_colors(&GFX, 0x7FFF, 0x1234) == 0x1234);
  //8/8 is bit-identical to the historical average
  set_blend(&GFX, 8, 8);
  int ok = 1;
  uint32_t s = 12345;
  for (int i = 0; i < 500; i++) {
    s = s * 1664525u + 1013904223u;
    color_16bit a = (color_16bit)(s & 0x7FFF);
    color_16bit b = (color_16bit)((s >> 16) & 0x7FFF);
    if (gfx_blend_colors(&GFX, a, b) != average_colors(a, b)) ok = 0;
  }
  CHECK(ok);
  //additive coefficients clamp at white
  set_blend(&GFX, 16, 16);
  CHECK(gfx_blend_colors(&GFX, 0x7FFF, 0x7FFF) == 0x7FFF);
  CHECK(gfx_blend_colors(&GFX, 0x7FFF, 0x0000) == 0x7FFF);
  //half + half of mid grey adds up, no clamp needed
  set_blend(&GFX, 8, 8);
  CHECK(gfx_blend_colors(&GFX, (15<<10), (15<<10)) == (15<<10));
  //fresh sprites carry no blend flag; the setter flips only that bit
  clear_all_fsp(&GFX);
  uint8_t sp = add_fsp(&GFX, 1, 0, 10, 10);
  CHECK((GFX.fsp.oam[sp] & Mask_fsp_oam_effects) == 0);
  set_fsp_blend(&GFX, sp, 1);
  CHECK((GFX.fsp.oam[sp] & Mask_fsp_oam_effects) != 0);
  CHECK((GFX.fsp.oam[sp] & Mask_fsp_oam_index) == 1);
  set_fsp_blend(&GFX, sp, 0);
  CHECK((GFX.fsp.oam[sp] & Mask_fsp_oam_effects) == 0);
  set_blend(&GFX, 8, 8); //leave defaults for other tests
}

int main(void)
{
  test_blending();
  test_body_packing();
  test_aabb();
  test_average_colors();
  test_tile_pixels();
  test_gfx_roundtrip();
  test_map_roundtrip();
  test_free_list();
  test_animation_word();
  if (failures) {
    fprintf(stderr, "UNITS: %d/%d checks FAILED\n", failures, checks);
    return 1;
  }
  printf("UNITS: %d checks OK\n", checks);
  return 0;
}
