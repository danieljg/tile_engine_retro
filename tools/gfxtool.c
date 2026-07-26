/* gfxtool: asset pipeline for tile_engine_retro. Replaces bmp_to_gfx.

   gfxtool build <manifest> <out.gfx>
       Manifest-driven conversion. Directives (one per line, # comments):
         type background|fullsprite|halfsprite   (sets tile size 16/16/8)
         tileset <image>                          (repeatable, PNG or BMP)
         palette <image> [alpha=i,j,...]          (repeatable; 16 colors from
                                                   the first 16 raster pixels;
                                                   alpha= marks semitransparent
                                                   color indices)
         indexref <image>                         (optional: palette used to
                                                   map RGB pixels to indices;
                                                   defaults to first palette)
       8-bit indexed BMP tilesets use their embedded pixel indices directly
       (exact legacy behavior); any other input is matched RGB-exact against
       the index reference palette and fails loudly on unknown colors.

   gfxtool dump <in.gfx> <out.png>
       Renders the tileset as a contact sheet (16 tiles per row, palette 0).

   gfxtool import-scene <image> <palette> <out.gfx> <out.map>
       Slices a 512x512 scene image into 16x16 tiles, deduplicates identical
       tiles, and emits a background tileset plus a 32x32 tilemap.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_STATIC
#include "stb_image_write.h"
#include "qdbmp.h"

#define MAX_TILES   1024
#define MAX_PALS    16
#define MAX_INPUTS  16

static void die(const char* fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    fprintf(stderr, "gfxtool: error: ");
    vfprintf(stderr, fmt, va);
    fputc('\n', stderr);
    va_end(va);
    exit(1);
}

typedef struct { uint8_t r, g, b; } rgb8;

typedef struct { int w, h; unsigned char* px; } image; /* RGB, 3 channels */

static image load_image_rgb(const char* path)
{
    image im;
    int n;
    im.px = stbi_load(path, &im.w, &im.h, &n, 3);
    if (!im.px) die("cannot read image '%s': %s", path, stbi_failure_reason());
    return im;
}

static void load_palette16(const char* path, rgb8 out[16])
{
    image im = load_image_rgb(path);
    if (im.w * im.h < 16)
        die("palette '%s' has %d pixels, needs at least 16 (raster order)",
            path, im.w * im.h);
    for (int i = 0; i < 16; i++) {
        const unsigned char* p = im.px + 3 * i;
        out[i].r = p[0]; out[i].g = p[1]; out[i].b = p[2];
    }
    stbi_image_free(im.px);
}

static int str_ends_with(const char* s, const char* suffix)
{
    size_t ls = strlen(s), lx = strlen(suffix);
    if (lx > ls) return 0;
    for (size_t i = 0; i < lx; i++)
        if ((s[ls - lx + i] | 32) != (suffix[i] | 32)) return 0;
    return 1;
}

/* Returns a w*h array of 4-bit palette indices. 8-bit BMPs contribute
   their embedded indices; everything else is matched against ref. */
static uint8_t* load_tileset_indices(const char* path, const rgb8 ref[16],
                                     int have_ref, int* out_w, int* out_h)
{
    if (str_ends_with(path, ".bmp")) {
        BMP* b = BMP_ReadFile((char*)path);
        if (b && BMP_GetError() == BMP_OK && BMP_GetDepth(b) == 8) {
            int w = (int)BMP_GetWidth(b), h = (int)BMP_GetHeight(b);
            uint8_t* idx = malloc((size_t)w * h);
            for (int y = 0; y < h; y++)
                for (int x = 0; x < w; x++) {
                    uint8_t v;
                    BMP_GetPixelIndex(b, x, y, &v);
                    if (v >= 16)
                        die("'%s' pixel (%d,%d) uses palette index %d; "
                            "only indices 0-15 are supported", path, x, y, v);
                    idx[(size_t)y * w + x] = v;
                }
            BMP_Free(b);
            *out_w = w; *out_h = h;
            return idx;
        }
        if (b) BMP_Free(b);
        /* not an 8-bit BMP: fall through to the RGB path */
    }
    if (!have_ref)
        die("'%s' is not an 8-bit indexed BMP and no palette/indexref was "
            "given to match its colors against", path);
    image im = load_image_rgb(path);
    uint8_t* idx = malloc((size_t)im.w * im.h);
    for (int y = 0; y < im.h; y++)
        for (int x = 0; x < im.w; x++) {
            const unsigned char* p = im.px + 3 * ((size_t)y * im.w + x);
            int found = -1;
            for (int i = 0; i < 16; i++)
                if (ref[i].r == p[0] && ref[i].g == p[1] && ref[i].b == p[2]) {
                    found = i;
                    break;
                }
            if (found < 0)
                die("'%s' pixel (%d,%d) color #%02x%02x%02x is not in the "
                    "index reference palette", path, x, y, p[0], p[1], p[2]);
            idx[(size_t)y * im.w + x] = (uint8_t)found;
        }
    stbi_image_free(im.px);
    *out_w = im.w; *out_h = im.h;
    return idx;
}

/* ---- gfx writing (byte-compatible with the historical format) ---- */

static void write_u8(FILE* f, uint8_t v)   { fwrite(&v, 1, 1, f); }
static void write_u16be(FILE* f, uint16_t v)
{
    write_u8(f, (uint8_t)(v >> 8));
    write_u8(f, (uint8_t)(v & 0xFF));
}

static void gfx_write_palette(FILE* f, const rgb8 pal[16], uint16_t alpha_mask)
{
    for (int i = 0; i < 16; i++) {
        uint16_t c = (uint16_t)(((pal[i].r >> 3) << 10)
                              | ((pal[i].g >> 3) << 5)
                              |  (pal[i].b >> 3));
        if (alpha_mask & (1u << i)) c |= 0x8000;
        write_u16be(f, c);
    }
}

/* Tile-major, top-left tile first, rows within a tile, 2 pixels per byte
   (high nibble first) — identical iteration to the original converter. */
static void gfx_write_tile_data(FILE* f, const uint8_t* idx, int w,
                                int tiles, int ts)
{
    for (int tile = 0; tile < tiles; tile++) {
        int ty0 = (tile / (w / ts)) * ts;
        int tx0 = (ts * tile) % w;
        for (int y = ty0; y < ty0 + ts; y++)
            for (int x = tx0; x < tx0 + ts; x += 2)
                write_u8(f, (uint8_t)((idx[(size_t)y * w + x] << 4)
                                     | idx[(size_t)y * w + x + 1]));
    }
}

/* ---- build ---- */

static uint16_t parse_alpha_list(const char* s, const char* mfst, int lineno)
{
    uint16_t mask = 0;
    while (*s) {
        char* end;
        long v = strtol(s, &end, 10);
        if (end == s || v < 0 || v > 15)
            die("%s:%d: bad alpha color index in '%s'", mfst, lineno, s);
        mask |= (uint16_t)(1u << v);
        s = end;
        if (*s == ',') s++;
        else if (*s) die("%s:%d: bad alpha list separator '%c'", mfst, lineno, *s);
    }
    return mask;
}

static int cmd_build(const char* mfst_path, const char* out_path)
{
    FILE* mf = fopen(mfst_path, "r");
    if (!mf) die("cannot open manifest '%s'", mfst_path);

    int tile_size = 0;
    char tilesets[MAX_INPUTS][512];  int n_tilesets = 0;
    char palettes[MAX_INPUTS][512];  int n_palettes = 0;
    uint16_t alpha_masks[MAX_INPUTS] = {0};
    char indexref[512] = "";

    char line[1024];
    int lineno = 0;
    while (fgets(line, sizeof(line), mf)) {
        lineno++;
        char a[512] = "", b[512] = "", c[512] = "";
        char* hash = strchr(line, '#');
        if (hash) *hash = 0;
        int n = sscanf(line, "%511s %511s %511s", a, b, c);
        if (n <= 0) continue;
        if (!strcmp(a, "type")) {
            if (!strcmp(b, "background") || !strcmp(b, "fullsprite"))
                tile_size = 16;
            else if (!strcmp(b, "halfsprite"))
                tile_size = 8;
            else die("%s:%d: unknown type '%s'", mfst_path, lineno, b);
        }
        else if (!strcmp(a, "tileset")) {
            if (n < 2) die("%s:%d: tileset needs a path", mfst_path, lineno);
            if (n_tilesets >= MAX_INPUTS) die("%s: too many tilesets (max %d)", mfst_path, MAX_INPUTS);
            strcpy(tilesets[n_tilesets++], b);
        }
        else if (!strcmp(a, "palette")) {
            if (n < 2) die("%s:%d: palette needs a path", mfst_path, lineno);
            if (n_palettes >= MAX_PALS) die("%s: too many palettes (max %d)", mfst_path, MAX_PALS);
            strcpy(palettes[n_palettes], b);
            if (n == 3) {
                if (strncmp(c, "alpha=", 6))
                    die("%s:%d: unknown palette option '%s'", mfst_path, lineno, c);
                alpha_masks[n_palettes] = parse_alpha_list(c + 6, mfst_path, lineno);
            }
            n_palettes++;
        }
        else if (!strcmp(a, "indexref")) {
            if (n < 2) die("%s:%d: indexref needs a path", mfst_path, lineno);
            strcpy(indexref, b);
        }
        else die("%s:%d: unknown directive '%s'", mfst_path, lineno, a);
    }
    fclose(mf);

    if (!tile_size)  die("%s: missing 'type' directive", mfst_path);
    if (!n_tilesets) die("%s: no tilesets", mfst_path);
    if (!n_palettes) die("%s: no palettes", mfst_path);

    rgb8 pals[MAX_PALS][16];
    for (int i = 0; i < n_palettes; i++) load_palette16(palettes[i], pals[i]);
    rgb8 ref[16];
    if (indexref[0]) load_palette16(indexref, ref);
    else memcpy(ref, pals[0], sizeof(ref));

    uint8_t* idx[MAX_INPUTS];
    int w[MAX_INPUTS], h[MAX_INPUTS], tiles[MAX_INPUTS];
    int total_tiles = 0;
    for (int i = 0; i < n_tilesets; i++) {
        idx[i] = load_tileset_indices(tilesets[i], ref, 1, &w[i], &h[i]);
        if (w[i] % tile_size || h[i] % tile_size)
            die("'%s' is %dx%d, not a multiple of tile size %d",
                tilesets[i], w[i], h[i], tile_size);
        tiles[i] = (w[i] / tile_size) * (h[i] / tile_size);
        total_tiles += tiles[i];
    }
    if (total_tiles > MAX_TILES)
        die("%d tiles total, engine maximum is %d", total_tiles, MAX_TILES);

    FILE* out = fopen(out_path, "wb");
    if (!out) die("cannot write '%s'", out_path);
    fwrite("GFX\n", 1, 4, out);
    write_u8(out, 4); /* bits per pixel */
    write_u8(out, (uint8_t)n_palettes);
    for (int i = 0; i < n_palettes; i++)
        gfx_write_palette(out, pals[i], alpha_masks[i]);
    write_u8(out, (uint8_t)tile_size);
    write_u16be(out, (uint16_t)total_tiles);
    for (int i = 0; i < n_tilesets; i++) {
        gfx_write_tile_data(out, idx[i], w[i], tiles[i], tile_size);
        free(idx[i]);
    }
    fclose(out);
    printf("gfxtool: %s -> %s (%d palettes, %d tiles of %dx%d)\n",
           mfst_path, out_path, n_palettes, total_tiles, tile_size, tile_size);
    return 0;
}

/* ---- dump ---- */

static int cmd_dump(const char* gfx_path, const char* png_path)
{
    FILE* f = fopen(gfx_path, "rb");
    if (!f) die("cannot open '%s'", gfx_path);
    uint8_t hdr[6];
    if (fread(hdr, 1, 6, f) != 6 || memcmp(hdr, "GFX\n", 4))
        die("'%s' is not a GFX file", gfx_path);
    if (hdr[4] != 4) die("'%s': only 4bpp is supported (got %d)", gfx_path, hdr[4]);
    int n_pal = hdr[5];
    uint16_t pal[MAX_PALS][16];
    for (int p = 0; p < n_pal; p++)
        for (int c = 0; c < 16; c++) {
            uint8_t b[2];
            if (fread(b, 1, 2, f) != 2) die("'%s': truncated palette", gfx_path);
            pal[p][c] = (uint16_t)((b[0] << 8) | b[1]);
        }
    uint8_t tsz[3];
    if (fread(tsz, 1, 3, f) != 3) die("'%s': truncated tile header", gfx_path);
    int ts = tsz[0], n_tiles = (tsz[1] << 8) | tsz[2];
    size_t tile_bytes = (size_t)ts * ts / 2;
    uint8_t* data = malloc(tile_bytes * n_tiles);
    if (fread(data, 1, tile_bytes * n_tiles, f) != tile_bytes * n_tiles)
        die("'%s': truncated tile data", gfx_path);
    fclose(f);

    int cols = n_tiles < 16 ? n_tiles : 16;
    int rows = (n_tiles + 15) / 16;
    int W = cols * ts, H = rows * ts;
    unsigned char* png = calloc((size_t)W * H, 3);
    for (int t = 0; t < n_tiles; t++) {
        int ox = (t % 16) * ts, oy = (t / 16) * ts;
        for (int y = 0; y < ts; y++)
            for (int x = 0; x < ts; x++) {
                uint8_t byte = data[t * tile_bytes + (y * ts + x) / 2];
                uint8_t pix = (x & 1) ? (byte & 0x0F) : (byte >> 4);
                uint16_t c = pal[0][pix];
                unsigned char* o = png + 3 * ((size_t)(oy + y) * W + ox + x);
                o[0] = (unsigned char)(((c >> 10) & 31) << 3);
                o[1] = (unsigned char)(((c >> 5) & 31) << 3);
                o[2] = (unsigned char)((c & 31) << 3);
            }
    }
    if (!stbi_write_png(png_path, W, H, 3, png, W * 3))
        die("cannot write '%s'", png_path);
    printf("gfxtool: %s -> %s (%d tiles of %dx%d, %d palettes)\n",
           gfx_path, png_path, n_tiles, ts, ts, n_pal);
    free(png);
    free(data);
    return 0;
}

/* ---- import-scene ---- */

static int cmd_import_scene(const char* img_path, const char* pal_path,
                            const char* gfx_path, const char* map_path)
{
    rgb8 pal[16];
    load_palette16(pal_path, pal);
    int w, h;
    uint8_t* idx = load_tileset_indices(img_path, pal, 1, &w, &h);
    if (w != 512 || h != 512)
        die("scene image '%s' is %dx%d; must be 512x512 (32x32 tiles)",
            img_path, w, h);

    uint8_t tiles[MAX_TILES][128]; /* 16x16 at 2 px per byte */
    uint16_t map[32 * 32];
    int n_tiles = 0;
    for (int ty = 0; ty < 32; ty++)
        for (int tx = 0; tx < 32; tx++) {
            uint8_t packed[128];
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 16; x += 2) {
                    size_t o = (size_t)(ty * 16 + y) * w + tx * 16 + x;
                    packed[(y * 16 + x) / 2] =
                        (uint8_t)((idx[o] << 4) | idx[o + 1]);
                }
            int found = -1;
            for (int t = 0; t < n_tiles; t++)
                if (!memcmp(tiles[t], packed, 128)) { found = t; break; }
            if (found < 0) {
                if (n_tiles >= MAX_TILES)
                    die("scene '%s' needs more than %d unique tiles",
                        img_path, MAX_TILES);
                memcpy(tiles[n_tiles], packed, 128);
                found = n_tiles++;
            }
            map[ty * 32 + tx] = (uint16_t)found;
        }
    free(idx);

    FILE* out = fopen(gfx_path, "wb");
    if (!out) die("cannot write '%s'", gfx_path);
    fwrite("GFX\n", 1, 4, out);
    write_u8(out, 4);
    write_u8(out, 1);
    gfx_write_palette(out, pal, 0);
    write_u8(out, 16);
    write_u16be(out, (uint16_t)n_tiles);
    for (int t = 0; t < n_tiles; t++) fwrite(tiles[t], 1, 128, out);
    fclose(out);

    FILE* mout = fopen(map_path, "wb");
    if (!mout) die("cannot write '%s'", map_path);
    fwrite("MAP\n", 1, 4, mout);
    write_u8(mout, 32);
    write_u8(mout, 32);
    for (int i = 0; i < 32 * 32; i++) write_u16be(mout, map[i]);
    fclose(mout);

    printf("gfxtool: %s -> %s + %s (1024 cells, %d unique tiles, %.0f%% saved)\n",
           img_path, gfx_path, map_path, n_tiles,
           100.0 * (1024 - n_tiles) / 1024.0);
    return 0;
}

int main(int argc, char* argv[])
{
    if (argc == 4 && !strcmp(argv[1], "build"))
        return cmd_build(argv[2], argv[3]);
    if (argc == 4 && !strcmp(argv[1], "dump"))
        return cmd_dump(argv[2], argv[3]);
    if (argc == 6 && !strcmp(argv[1], "import-scene"))
        return cmd_import_scene(argv[2], argv[3], argv[4], argv[5]);
    fprintf(stderr,
        "usage: gfxtool build <manifest> <out.gfx>\n"
        "       gfxtool dump <in.gfx> <out.png>\n"
        "       gfxtool import-scene <image> <palette> <out.gfx> <out.map>\n");
    return 1;
}
