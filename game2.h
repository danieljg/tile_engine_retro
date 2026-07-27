//Game logic in structure-of-arrays style (layout by Luis Navarro).
//Each entity kind is one struct of parallel arrays; entity fields are
//bit-packed words manipulated through the MASK_* defines below.
#ifndef GAME2_H
#define GAME2_H

#include <stdint.h>
#include <stdio.h>

#include "gfx_engine.h"

//the single engine instance this game drives (game2.h is app-layer:
//it may use globals; the engine itself takes the context explicitly)
static gfx_context GFX;

#define MAX_PLAYERS 4
#define MAX_ENEMIES 16
#define MAX_PPROJECTILES 32  // max player projectiles
#define MAX_EPROJECTILES 256 // max enemy  projectiles
#define MAX_POWERUPS 4
#define TOP_SCORES_COUNT 10

#define START_LIVES 3

#define MASK_INPUT_START 0x80
#define MASK_INPUT_A     0x40
#define MASK_INPUT_B     0x20
#define MASK_INPUT_C     0x10
#define MASK_INPUT_UP    0x08
#define MASK_INPUT_DOWN  0x04
#define MASK_INPUT_LEFT  0x02
#define MASK_INPUT_RIGHT 0x01

#define MASK_XDATA_OFFSET 0xFF000000
#define MASK_XDATA_RES    0x00F00000
#define MASK_XDATA_VEL    0x000FF000
#define MASK_XDATA_POS    0x00000FFF

#define MASK_YDATA_OFFSET 0xFF000000
#define MASK_YDATA_RES    0x00F00000
#define MASK_YDATA_VEL    0x000FF000
#define MASK_YDATA_POS    0x00000FFF

#define MASK_DIMENSIONS_WIDTH  0xFF00
#define MASK_DIMENSIONS_HEIGHT 0x00FF

#define MASK_ANIMATION_TOTLFRM 0xF0000000
#define MASK_ANIMATION_CURRFRM 0x0F000000
#define MASK_ANIMATION_RES2    0x00FC0000
#define MASK_ANIMATION_TLESTRT 0x0003FF00
#define MASK_ANIMATION_RES1    0x000000E0
#define MASK_ANIMATION_SPINDEX 0x0000001F

//entity states (2 bit STATE fields)
#define STATE_IDLE     0
#define STATE_SPAWNING 1
#define STATE_ALIVE    2
#define STATE_DYING    3

/* Shared accessors for the packed physics words.
   Positions are 12 bits, oversampled by 3 (1 unit = 1/8 pixel, wraps at
   512 px). Velocity is a signed 8 bit field in the same units. */
static void inline body_set_pos(uint32_t* data, uint16_t pos) {
  *data = (*data & (~MASK_XDATA_POS)) | (pos & MASK_XDATA_POS);
}

static uint16_t inline body_get_pos(const uint32_t* data) {
  return *data & MASK_XDATA_POS;
}

static void inline body_set_vel(uint32_t* data, int8_t vel) {
  *data = (*data & (~MASK_XDATA_VEL)) | ((uint32_t)(uint8_t)vel << 12);
}

static int8_t inline body_get_vel(const uint32_t* data) {
  return (int8_t)((*data & MASK_XDATA_VEL) >> 12);
}

static void inline body_update(uint32_t* xdata, uint32_t* ydata) {
  body_set_pos(xdata, body_get_pos(xdata) + body_get_vel(xdata));
  body_set_pos(ydata, body_get_pos(ydata) + body_get_vel(ydata));
}

/* AABB overlap on the packed physics words; dimensions are in pixels,
   positions in 1/8 pixel units. No wraparound handling yet. */
static uint8_t inline aabb_hit(uint32_t x1, uint32_t y1, uint16_t d1,
                               uint32_t x2, uint32_t y2, uint16_t d2) {
  int32_t px1 = x1 & MASK_XDATA_POS, py1 = y1 & MASK_YDATA_POS;
  int32_t px2 = x2 & MASK_XDATA_POS, py2 = y2 & MASK_YDATA_POS;
  int32_t w1 = (int32_t)((d1 & MASK_DIMENSIONS_WIDTH)>>8) << 3;
  int32_t h1 = (int32_t)(d1 & MASK_DIMENSIONS_HEIGHT) << 3;
  int32_t w2 = (int32_t)((d2 & MASK_DIMENSIONS_WIDTH)>>8) << 3;
  int32_t h2 = (int32_t)(d2 & MASK_DIMENSIONS_HEIGHT) << 3;
  return (px1 < px2+w2) && (px2 < px1+w1) &&
         (py1 < py2+h2) && (py2 < py1+h1);
}

/* Shared accessors for the packed animation words. */
static void inline animation_init(uint32_t* anim, uint8_t total_frames,
                                  uint16_t tile_start, uint8_t sp_id) {
  *anim = ((uint32_t)total_frames << 28) |
          (((uint32_t)tile_start << 8) & MASK_ANIMATION_TLESTRT) |
          (sp_id & MASK_ANIMATION_SPINDEX);
}

static void inline animation_advance(uint32_t* anim) {
  if( ((*anim&MASK_ANIMATION_CURRFRM)>>24) < (((*anim&MASK_ANIMATION_TOTLFRM)>>28)-1))
    *anim = (((*anim&MASK_ANIMATION_CURRFRM)+0x01000000)&MASK_ANIMATION_CURRFRM)
          | (*anim&(~MASK_ANIMATION_CURRFRM));
  else
    *anim = *anim&(~MASK_ANIMATION_CURRFRM);
  set_fsp(&GFX, *anim&MASK_ANIMATION_SPINDEX,
           ((*anim&MASK_ANIMATION_TLESTRT)>>8)
          +((*anim&MASK_ANIMATION_CURRFRM)>>24) );
}

static uint8_t inline animation_sprite(const uint32_t* anim) {
  return *anim & MASK_ANIMATION_SPINDEX;
}

/* ---- sound effects: synthesized voices, mixed over the music by the
   frontend layer. sfx_play just claims a voice; rendering happens in the
   audio callback. ---- */
#define SFX_VOICES 4
#define SFX_NONE  0
#define SFX_LASER 1
#define SFX_BOOM  2
#define SFX_PLOP  3
#define SFX_BLIP  4

typedef struct { uint8_t type; uint32_t t; uint32_t seed; } sfx_voice;
static sfx_voice sfx[SFX_VOICES];

static void sfx_play(uint8_t type) {
  //take a free voice, or steal the oldest one
  uint8_t best = 0;
  uint32_t best_t = 0;
  for (uint8_t i=0; i<SFX_VOICES; i++) {
    if (sfx[i].type == SFX_NONE) { best = i; break; }
    if (sfx[i].t >= best_t) { best_t = sfx[i].t; best = i; }
  }
  sfx[best].type = type;
  sfx[best].t = 0;
  sfx[best].seed = 0x12345678u + type;
}

/* 8-bit angles: 256 units per turn, 0 = +x (right), 64 = +y (down).
   sin in 1/64 units through a 32-entry table. */
static const int8_t sin64[32] = {
    0,  12,  24,  36,  45,  53,  59,  62,
   63,  62,  59,  53,  45,  36,  24,  12,
    0, -12, -24, -36, -45, -53, -59, -62,
  -63, -62, -59, -53, -45, -36, -24, -12
};
static int8_t inline sin8(uint8_t a) { return sin64[(a>>3)&31]; }
static int8_t inline cos8(uint8_t a) { return sin64[((a>>3)+8)&31]; }

//coarse eight-direction angle toward (dx, dy), in 1/8 px units
static uint8_t inline angle_toward(int32_t dx, int32_t dy) {
  int32_t ax = dx<0?-dx:dx, ay = dy<0?-dy:dy;
  if (ax > (ay<<1)) return dx>=0 ? 0 : 128;         //mostly horizontal
  if (ay > (ax<<1)) return dy>=0 ? 64 : 192;        //mostly vertical
  if (dx>=0) return dy>=0 ? 32 : 224;               //diagonals
  return dy>=0 ? 96 : 160;
}

#define MASK_PLAYER_BASE_RES1  0xC000
#define MASK_PLAYER_BASE_STATE 0x3000 //(2 bits) idle, spawning, alive, dying
#define MASK_PLAYER_BASE_RES2  0x0800
#define MASK_PLAYER_BASE_LIVES 0x0700 //(3 bits) 8 lives max.
#define MASK_PLAYER_BASE_INPUT 0x00FF //(8 bits) 4 direction buttons, 3 action buttons, 1 Start button.

typedef struct {
  // Basic data
  uint16_t base[MAX_PLAYERS];
  uint8_t  power_ups[MAX_PLAYERS];
  uint8_t  cooldown[MAX_PLAYERS]; // frames until the weapon can fire again
  uint32_t score[MAX_PLAYERS];
  // Physics Data
  uint32_t xdata[MAX_PLAYERS];
  uint32_t ydata[MAX_PLAYERS];
  uint16_t dimensions[MAX_PLAYERS];
  // Animation data
  uint32_t animation[MAX_PLAYERS];
}
players_struct;

static uint8_t inline player_state(uint8_t id);
static void inline player_set_state(uint8_t id, uint8_t state);

#define MASK_ENEMY_BASE_RES1     0xC000
#define MASK_ENEMY_BASE_STATE    0x3000 //(2 bits) idle, spawning, alive, dying
#define MASK_ENEMY_BASE_RES2     0x0FF0
#define MASK_ENEMY_BASE_AI_STATE 0x000F //(4 bits)

typedef struct {
  // Basic data
  uint16_t base[MAX_ENEMIES];
  // Physics Data
  uint32_t xdata[MAX_ENEMIES];
  uint32_t ydata[MAX_ENEMIES];
  uint16_t dimensions[MAX_ENEMIES];
  // Animation data
  uint32_t animation[MAX_ENEMIES];
}
enemies_struct;

#define MASK_PPROJECTILE_BASE_OWNER    0xC000 //(2 bits) Owner player id
#define MASK_PPROJECTILE_BASE_STATE    0x3000 //(2 bits) idle, spawning, alive, exploding
#define MASK_PPROJECTILE_BASE_RES      0x0F00
#define MASK_PPROJECTILE_BASE_DAMAGE   0x00F0 //(4 bits) 0-15 damage
#define MASK_PPROJECTILE_BASE_AI_STATE 0x000F //(4 bits)

typedef struct {
  // Basic data
  uint16_t base[MAX_PPROJECTILES];
  uint8_t  sprite_id[MAX_PPROJECTILES]; // hsp ids exceed the 5 bit SPINDEX
  // Physics Data
  uint32_t xdata[MAX_PPROJECTILES];
  uint32_t ydata[MAX_PPROJECTILES];
  uint16_t dimensions[MAX_PPROJECTILES];
  // Animation data
  uint32_t animation[MAX_PPROJECTILES];
}
player_projectiles_struct;

#define MASK_EPROJECTILE_BASE_RES1     0xC000
#define MASK_EPROJECTILE_BASE_STATE    0x3000 //(2 bits) idle, spawning, alive, exploding
#define MASK_EPROJECTILE_BASE_RES2     0x0F00
#define MASK_EPROJECTILE_BASE_DAMAGE   0x00F0 //(4 bits) 0-15 damage
#define MASK_EPROJECTILE_BASE_AI_STATE 0x000F //(4 bits)

typedef struct {
  // Basic data
  uint16_t base[MAX_EPROJECTILES];
  uint8_t  sprite_id[MAX_EPROJECTILES];
  // Physics Data
  uint32_t xdata[MAX_EPROJECTILES];
  uint32_t ydata[MAX_EPROJECTILES];
  uint16_t dimensions[MAX_EPROJECTILES];
  // Animation data
  uint32_t animation[MAX_EPROJECTILES];
}
enemy_projectiles_struct;

#define MASK_POWERUP_BASE_RES1     0xC000
#define MASK_POWERUP_BASE_STATE    0x3000 //(2 bits) idle, spawning, alive, using
#define MASK_POWERUP_BASE_RES2     0x0F80
#define MASK_POWERUP_BASE_TYPE     0x0070 //(3 bits) 8 power_up types
#define MASK_POWERUP_BASE_AI_STATE 0x000F //(4 bits)

typedef struct {
  // Basic data
  uint16_t base[MAX_POWERUPS];
  // Physics Data
  uint32_t xdata[MAX_POWERUPS];
  uint32_t ydata[MAX_POWERUPS];
  uint16_t dimensions[MAX_POWERUPS];
  // Animation data
  uint32_t animation[MAX_POWERUPS];
}
power_up_struct;

typedef struct {
  uint32_t initials[TOP_SCORES_COUNT]; // 3 letters in ASCII (24 bits)
  uint32_t score[TOP_SCORES_COUNT];
}
hi_score_struct;

#define MASK_GAMEDATA1_RES1                      0xFC000000
#define MASK_GAMEDATA1_LAST_PLAYER_ID            0x03000000 // 4 players
#define MASK_GAMEDATA1_RES2                      0x00FC0000
#define MASK_GAMEDATA1_LAST_POWERUP_ID           0x00030000 // 4 power ups
#define MASK_GAMEDATA1_LAST_ENEMY_PROJECTILE_ID  0x0000FFFF // 256 projectiles

#define MASK_GAMEDATA2_RES1                      0xFF000000
#define MASK_GAMEDATA2_LAST_ENEMY_ID             0x00FF0000 // 16 enemies
#define MASK_GAMEDATA2_RES2                      0x0000FE00
#define MASK_GAMEDATA2_LAST_PLAYER_PROJECTILE_ID 0x000001FF // 32 projectiles

static uint32_t gamedata1, gamedata2;

players_struct players;

static uint8_t inline player_state(uint8_t id) {
  return (players.base[id] & MASK_PLAYER_BASE_STATE) >> 12;
}

static void inline player_set_state(uint8_t id, uint8_t state) {
  players.base[id] = (players.base[id] & (~MASK_PLAYER_BASE_STATE))
                   | ((state & 0x3) << 12);
}

static uint8_t inline player_lives(uint8_t id) {
  return (players.base[id] & MASK_PLAYER_BASE_LIVES) >> 8;
}

static void inline player_set_lives(uint8_t id, uint8_t lives) {
  players.base[id] = (players.base[id] & (~MASK_PLAYER_BASE_LIVES))
                   | ((lives & 0x7) << 8);
}

static void inline initialize_players(void) {
  for (uint8_t i=0; i<MAX_PLAYERS; i++) {
    players.base[i] =                0x0000;
    players.power_ups[i] =             0x00;
    players.cooldown[i] =              0x00;
    players.score[i] =           0x00000000;
    players.xdata[i] =           0x00000000;
    players.ydata[i] =           0x00000000;
    players.dimensions[i] =          0x0000;
    players.animation[i] =       0x00000000;
  }
}

#define PLAYER_START_TILE 12
#define PLAYER_FRAMES 3
#define PLAYER_FIRE_COOLDOWN 12

static uint8_t add_player(uint8_t player_id, uint16_t pos_x, uint16_t pos_y) {
  if (player_id >= MAX_PLAYERS) return 0;
  uint8_t sp_id = add_fsp(&GFX, 
    PLAYER_START_TILE,
    player_id+1, // sprite palette (palettes 1-4 are ship palettes)
    (pos_x+4)>>3, (pos_y+4)>>3
  );
  if (sp_id >= fsp_count) return 0; //no free sprite slot
  set_fsp_effects(&GFX, sp_id, 0, 0, 1, 0); //rotate +90: ship faces right
  players.base[player_id] =
    (STATE_SPAWNING<<12) |
    (START_LIVES<<8);
  players.xdata[player_id] = 0; players.ydata[player_id] = 0;
  body_set_pos(&players.xdata[player_id], pos_x);
  body_set_pos(&players.ydata[player_id], pos_y);
  players.dimensions[player_id] = (12<<8) | 12; //hitbox smaller than sprite
  animation_init(&players.animation[player_id],
                 PLAYER_FRAMES, PLAYER_START_TILE, sp_id);
  return 1;
}

enemies_struct enemies;

static uint8_t inline enemy_state(uint8_t id) {
  return (enemies.base[id] & MASK_ENEMY_BASE_STATE) >> 12;
}

static void inline initialize_enemies(void) {
  for (uint8_t i=0; i<MAX_ENEMIES; i++) {
    enemies.base[i] =                0x0000;
    enemies.xdata[i] =           0x00000000;
    enemies.ydata[i] =           0x00000000;
    enemies.dimensions[i] =          0x0000;
    enemies.animation[i] =       0x00000000;
  }
}

/* The enemy "skin" is per scene: Asteroid Run flies the 2018 ships,
   Crystal Cavern hosts the six-frame metroid (fsp tiles 0-5) that sat
   unused in the sheet since 2018. Floaters bob instead of rotating. */
static uint16_t enemy_skin_tile = 12;
static uint8_t enemy_skin_frames = 3;
static uint8_t enemy_skin_rotates = 1; //ships face travel; floaters don't
#define ENEMY_PALETTE 0

//enemies fly in from the right facing left (h-flip via the pre-mirrored
//tileset); double_size makes a 32x32 "heavy" using the render-time scaler
static uint8_t add_enemy(uint16_t pos_x_px, uint16_t pos_y_px,
                         uint8_t double_size) {
  for (uint8_t i=0; i<MAX_ENEMIES; i++) {
    if (enemy_state(i) != STATE_IDLE) continue;
    uint8_t sp_id = add_fsp(&GFX, enemy_skin_tile, ENEMY_PALETTE,
                            pos_x_px, pos_y_px);
    if (sp_id >= fsp_count) return 0;
    if (enemy_skin_rotates) {
      //rotation+h_flip+v_flip = rotate -90: ships face left
      set_fsp_effects(&GFX, sp_id, 1, 1, 1, double_size);
    }
    else {
      set_fsp_effects(&GFX, sp_id, 0, 0, 0, double_size);
    }
    enemies.base[i] = STATE_ALIVE<<12;
    enemies.xdata[i] = 0; enemies.ydata[i] = 0;
    body_set_pos(&enemies.xdata[i], pos_x_px<<3);
    body_set_pos(&enemies.ydata[i], pos_y_px<<3);
    body_set_vel(&enemies.xdata[i], -3);
    enemies.dimensions[i] = double_size ? ((26<<8)|26) : ((13<<8)|13);
    animation_init(&enemies.animation[i], enemy_skin_frames,
                   enemy_skin_tile, sp_id);
    return 1;
  }
  return 0;
}

static void kill_enemy(uint8_t id) {
  delete_fsp(&GFX, animation_sprite(&enemies.animation[id]));
  enemies.base[id] = 0x0000;
  enemies.animation[id] = 0x00000000;
}

static void update_enemies(uint32_t frame) {
  for (uint8_t i=0; i<MAX_ENEMIES; i++) {
    if (enemy_state(i) != STATE_ALIVE) continue;
    body_update(&enemies.xdata[i], &enemies.ydata[i]);
    uint16_t x = body_get_pos(&enemies.xdata[i]);
    /* Exiting left, the sprite slides out across the 512 wrap (positions
       run 6,5,...,0,511,510,...) and is fully invisible only once even a
       double-size body fits between the screen edge and the wrap:
       x in (430..476). Recycling there is never witnessed. */
    if ((x>>3) > 430 && (x>>3) < 476) {
      body_set_pos(&enemies.xdata[i], 420<<3);
      x = body_get_pos(&enemies.xdata[i]);
    }
    int16_t fy = (int16_t)((body_get_pos(&enemies.ydata[i])+4)>>3);
    if (!enemy_skin_rotates) {
      //floaters ride an invisible swell, phase-shifted per slot
      fy = (int16_t)(fy + (sin8((uint8_t)((frame<<1) + i*43)) >> 4));
    }
    set_pos_fsp(&GFX, animation_sprite(&enemies.animation[i]),
                (int16_t)((x+4)>>3), fy);
  }
}

player_projectiles_struct pprojectiles;

static uint8_t inline pprojectile_state(uint8_t id) {
  return (pprojectiles.base[id] & MASK_PPROJECTILE_BASE_STATE) >> 12;
}

static void inline initialize_player_projectiles(void) {
  for (uint8_t i=0; i<MAX_PPROJECTILES; i++) {
    pprojectiles.base[i] =           0x0000;
    pprojectiles.sprite_id[i] =        0x00;
    pprojectiles.xdata[i] =      0x00000000;
    pprojectiles.ydata[i] =      0x00000000;
    pprojectiles.dimensions[i] =     0x0000;
    pprojectiles.animation[i] =  0x00000000;
  }
}

#define PPROJECTILE_TILE '*' //bullets come from the ASCII half-sprite bank
#define PPROJECTILE_PALETTE 2
#define PPROJECTILE_SPEED 32 // 4 px per frame
#define PPROJECTILE_DAMAGE 5

static void fire_pprojectile(uint8_t owner) {
  for (uint8_t i=0; i<MAX_PPROJECTILES; i++) {
    if (pprojectile_state(i) != STATE_IDLE) continue;
    uint16_t bx = (body_get_pos(&players.xdata[owner]) + (17<<3)) & MASK_XDATA_POS;
    uint16_t by = (body_get_pos(&players.ydata[owner]) + (4<<3)) & MASK_YDATA_POS;
    uint8_t sp_id = add_hsp(&GFX, PPROJECTILE_TILE, PPROJECTILE_PALETTE,
                            (bx+4)>>3, (by+4)>>3);
    if (sp_id >= hsp_count) return;
    pprojectiles.base[i] = (owner<<14) | (STATE_ALIVE<<12)
                         | (PPROJECTILE_DAMAGE<<4);
    pprojectiles.sprite_id[i] = sp_id;
    pprojectiles.xdata[i] = 0; pprojectiles.ydata[i] = 0;
    body_set_pos(&pprojectiles.xdata[i], bx);
    body_set_pos(&pprojectiles.ydata[i], by);
    body_set_vel(&pprojectiles.xdata[i], PPROJECTILE_SPEED);
    pprojectiles.dimensions[i] = (6<<8) | 6;
    sfx_play(SFX_LASER);
    return;
  }
}

static void kill_pprojectile(uint8_t id) {
  delete_hsp(&GFX, pprojectiles.sprite_id[id]);
  pprojectiles.base[id] = 0x0000;
}

static void kill_pellet_quiet(uint8_t id);

static void update_pprojectiles(void) {
  for (uint8_t i=0; i<MAX_PPROJECTILES; i++) {
    if (pprojectile_state(i) != STATE_ALIVE) continue;
    body_update(&pprojectiles.xdata[i], &pprojectiles.ydata[i]);
    uint16_t x = body_get_pos(&pprojectiles.xdata[i]);
    if ((x>>3) > 410) { //off the right edge
      kill_pprojectile(i);
      continue;
    }
    set_pos_hsp(&GFX, pprojectiles.sprite_id[i],
                (x+4)>>3, (body_get_pos(&pprojectiles.ydata[i])+4)>>3);
  }
}

enemy_projectiles_struct eprojectiles;
static void inline initialize_enemy_projectiles(void) {
  for (uint16_t i=0; i<MAX_EPROJECTILES; i++) { //uint16_t: 256 slots
    eprojectiles.base[i] =           0x0000;
    eprojectiles.sprite_id[i] =        0x00;
    eprojectiles.xdata[i] =      0x00000000;
    eprojectiles.ydata[i] =      0x00000000;
    eprojectiles.dimensions[i] =     0x0000;
    eprojectiles.animation[i] =  0x00000000;
  }
}

power_up_struct power_ups;
static void inline initialize_powerups(void) {
  for (uint8_t i=0; i<MAX_POWERUPS; i++) {
    power_ups.base[i] =              0x0000;
    power_ups.xdata[i] =         0x00000000;
    power_ups.ydata[i] =         0x00000000;
    power_ups.dimensions[i] =        0x0000;
    power_ups.animation[i] =     0x00000000;
  }
}

hi_score_struct top_scores;
static void inline initialize_topscores(void) {
  for (uint8_t i=0; i<TOP_SCORES_COUNT; i++) {
    top_scores.initials[i] =     0x00000000;
    top_scores.score[i] =        0x00000000;
  }
}

#define INITIALS(a,b,c) ((uint32_t)(' '<<24)|((a)<<16)|((b)<<8)|(c))

static void default_scores(void) {
  top_scores.initials[0] = INITIALS('A','B','C');
  top_scores.score[0] = 450000;
  top_scores.initials[1] = INITIALS('D','E','F');
  top_scores.score[1] = 350000;
  top_scores.initials[2] = INITIALS('G','H','I');
  top_scores.score[2] = 100000;
  top_scores.initials[3] = INITIALS('J','K','L');
  top_scores.score[3] = 50000;
  top_scores.initials[4] = INITIALS('M','N','O');
  top_scores.score[4] = 25000;
  top_scores.initials[5] = INITIALS('P','Q','R');
  top_scores.score[5] = 10000;
  top_scores.initials[6] = INITIALS('S','T','U');
  top_scores.score[6] = 5000;
  top_scores.initials[7] = INITIALS('V','W','X');
  top_scores.score[7] = 2500;
}

/* HUD: half-sprite slots are claimed in add_hud in a fixed order:
   0-4 player 1 score, 5-9 player 2 score, 10-15 hi-score, 16+ label */
#define ASCII0 48

static void inline add_hud(void) {
  for (uint8_t ii=0; ii<5; ii++) add_hsp(&GFX, '0', 0, 204+ii*8, 230);
  for (uint8_t ii=0; ii<5; ii++) add_hsp(&GFX, '0', 0, 252+ii*8, 230);
  //hi-score digits (slots 10 to 15)
  for (uint8_t ii=0; ii<6; ii++) add_hsp(&GFX, '0', 0, 84+ii*8, 5);
  draw_text(&GFX, "Hi-Score", 4, 4, 2);
}

static void inline update_score_hud(void) {
  uint32_t s = players.score[0];
  for (uint8_t i=0; i<5; i++) { set_hsp(&GFX, 4-i, ASCII0 + s%10); s/=10; }
  s = players.score[1];
  for (uint8_t i=0; i<5; i++) { set_hsp(&GFX, 9-i, ASCII0 + s%10); s/=10; }
}

static void inline update_hiscore(uint32_t score) {
  for (uint8_t i=0; i<6; i++) {
    set_hsp(&GFX, 15-i, ASCII0 + score%10);
    score/=10;
  }
}

static void inline update_hud(void) {
  update_score_hud();
  for (uint8_t i=0; i<MAX_PLAYERS; i++) {
    if (players.score[i] > top_scores.score[0]) {
      top_scores.score[0] = players.score[i];
    }
  }
  update_hiscore(top_scores.score[0]);
}

#define ORT_SPD 12
#define DIA_SPD 8 //~= ORT_SPD/sqrt(2), keeps diagonal speed uniform

static void update_player(uint8_t id) {
  uint8_t state = player_state(id);
  if (state == STATE_IDLE) return;
  if (state == STATE_SPAWNING) player_set_state(id, STATE_ALIVE);

  uint8_t input = players.base[id] & MASK_PLAYER_BASE_INPUT;

  // updating horizontal and vertical velocity
  uint8_t speed;
  if ((input & (MASK_INPUT_UP|MASK_INPUT_DOWN)) &&
      (input & (MASK_INPUT_LEFT|MASK_INPUT_RIGHT))) {
    speed = DIA_SPD;
  }
  else speed = ORT_SPD;

  if (input & MASK_INPUT_UP) body_set_vel(&players.ydata[id], -1*speed);
  else if (input & MASK_INPUT_DOWN) body_set_vel(&players.ydata[id], speed);
  else body_set_vel(&players.ydata[id], 0);
  if (input & MASK_INPUT_LEFT) body_set_vel(&players.xdata[id], -1*speed);
  else if (input & MASK_INPUT_RIGHT) body_set_vel(&players.xdata[id], speed);
  else body_set_vel(&players.xdata[id], 0);

  body_update(&players.xdata[id], &players.ydata[id]);
  set_pos_fsp(&GFX, animation_sprite(&players.animation[id]),
              (body_get_pos(&players.xdata[id])+4)>>3,
              (body_get_pos(&players.ydata[id])+4)>>3);

  // weapon
  if (players.cooldown[id]) players.cooldown[id]--;
  if ((input & MASK_INPUT_A) && players.cooldown[id]==0) {
    fire_pprojectile(id);
    players.cooldown[id] = PLAYER_FIRE_COOLDOWN;
  }
}

static void check_collisions(void) {
  // player projectiles vs enemies
  for (uint8_t p=0; p<MAX_PPROJECTILES; p++) {
    if (pprojectile_state(p) != STATE_ALIVE) continue;
    for (uint8_t e=0; e<MAX_ENEMIES; e++) {
      if (enemy_state(e) != STATE_ALIVE) continue;
      if (aabb_hit(pprojectiles.xdata[p], pprojectiles.ydata[p],
                   pprojectiles.dimensions[p],
                   enemies.xdata[e], enemies.ydata[e],
                   enemies.dimensions[e])) {
        uint8_t owner = (pprojectiles.base[p] & MASK_PPROJECTILE_BASE_OWNER)>>14;
        players.score[owner] += 100;
        kill_enemy(e);
        sfx_play(SFX_BOOM);
        kill_pprojectile(p);
        break;
      }
    }
  }
  // players vs enemies
  for (uint8_t i=0; i<MAX_PLAYERS; i++) {
    if (player_state(i) != STATE_ALIVE) continue;
    for (uint8_t e=0; e<MAX_ENEMIES; e++) {
      if (enemy_state(e) != STATE_ALIVE) continue;
      if (aabb_hit(players.xdata[i], players.ydata[i], players.dimensions[i],
                   enemies.xdata[e], enemies.ydata[e],
                   enemies.dimensions[e])) {
        uint8_t lives = player_lives(i);
        if (lives > 0) player_set_lives(i, lives-1);
        kill_enemy(e);
        sfx_play(SFX_BOOM);
      }
    }
  }
}

/* ---- timeline: "at frame N, do X" (the events.h successor) ----
   Each scene plays a scripted event list against the play-relative frame
   counter. TL_LOOP rebases time and restarts the list, so waves repeat. */
#define TL_END   0
#define TL_ENEMY 1 //a = x, b = y, c = double_size
#define TL_LOOP  2 //restart the timeline, rebasing time at this frame

typedef struct {
  uint16_t frame;
  uint8_t op;
  int16_t a, b;
  uint8_t c;
} tl_event;

//reinforcement waves for the shmup scenes (same cadence the old
//120-frame spawner had: lanes 1, 2, 3-heavy, 0, then loop)
static const tl_event shmup_timeline[] = {
  {120, TL_ENEMY, 412,  80, 0},
  {240, TL_ENEMY, 414, 120, 0},
  {360, TL_ENEMY, 416, 160, 1},
  {480, TL_ENEMY, 410,  40, 0},
  {480, TL_LOOP,    0,   0, 0},
};

static uint16_t tl_index = 0;
static uint32_t tl_base = 0;

//executes every event whose time has come
static void update_timeline(const tl_event* tl, uint32_t frame) {
  for (;;) {
    tl_event e = tl[tl_index];
    if (e.op == TL_END) return;
    if (frame - tl_base < e.frame) return;
    if (e.op == TL_LOOP) {
      tl_base += e.frame;
      tl_index = 0;
      continue;
    }
    if (e.op == TL_ENEMY) add_enemy((uint16_t)e.a, (uint16_t)e.b, e.c);
    tl_index++;
  }
}

static void inline update_animations(void) {
  for (uint8_t i=0; i<MAX_PLAYERS; i++) {
    if (player_state(i) != STATE_IDLE)
      animation_advance(&players.animation[i]);
  }
  for (uint8_t i=0; i<MAX_ENEMIES; i++) {
    if (enemy_state(i) == STATE_ALIVE)
      animation_advance(&enemies.animation[i]);
  }
}

/* game modes (shared by the pond and the shmup scenes) */
#define MODE_MENU    0
#define MODE_PLAYING 1
static uint8_t game_mode = MODE_MENU;

/* ---- koi pond (scene kind POND): a wholly different use of the same
   engine. Top-down water; koi are full sprites whose heading is expressed
   through the OAM transforms (art faces right: L = h-flip, D = rotation,
   U = rotation+h+v = -90). Ripples and pellets are half sprites; ripple
   rings are drawn in a semitransparent color so they blend with the
   water. The frontend layer adds per-scanline sine warping and caustic
   palette rotation on top. ---- */

#define MAX_KOI 10
#define MAX_PELLETS 6
#define MAX_RIPPLES 8
#define KOI_TILE 18        //adult, 2 frames: 18 tail up, 19 tail down
#define KOI_TILE_SM 20     //small fry, 2 frames: 20, 21
#define KOI_PAL_ORANGE 5
#define KOI_PAL_CALICO 6
#define RIPPLE_TILE 128    //3 growth stages: 128..130
#define PELLET_TILE 131

/* Sprite render priorities for the pond compositor (7 = topmost is the
   default for every add): */
#define PRIO_SHADOW      1
#define PRIO_KOI_DEEP    2
#define PRIO_KOI_MID     3
#define PRIO_KOI_SHALLOW 4
#define PRIO_SPARKLE     5
#define PRIO_LEAF        6

/* Lily pads are full sprites: they bob and drift as whole objects (motion
   without deformation) and, allocated before the koi, they draw ON TOP of
   the fish — the koi swim under the leaves. Each leaf casts a
   semitransparent shadow sprite allocated after the koi (under the fish).*/
#define MAX_LEAVES 10
#define LEAF_TILE_SMALL    22
#define LEAF_TILE_MEDIUM   23
#define LEAF_TILE_LOTUS    24
#define LEAF_TILE_TINY     25
#define LEAF_TILE_SHADOW   26
#define KOI_TILE_SHADOW    27
#define KOI_TILE_SHADOW_SM 28
#define RING_TILE          29 //+1 = second animation frame
#define FROND_TILE_A       31
#define FROND_TILE_B       32
#define RING_SM_TILE       33 //+1 = second animation frame
#define LEAF_PAL 7

typedef struct {
  uint8_t sprite[MAX_LEAVES];
  uint8_t shadow[MAX_LEAVES];
  uint8_t ring[MAX_LEAVES]; //contact ring on the surface, tracks the pad
  uint16_t base_x[MAX_LEAVES]; //anchor, pixels (wraps at 512)
  uint16_t base_y[MAX_LEAVES];
  uint8_t phase[MAX_LEAVES];
  uint8_t drift[MAX_LEAVES];  //downstream speed, 1/32 px per frame
  uint8_t dfrac[MAX_LEAVES];  //drift accumulator
} leaf_struct;
static leaf_struct leaves;

static uint32_t river_hash(uint32_t a, uint32_t b) {
  uint32_t v = a * 2654435761u ^ (b * 0x9E3779B9u + 0x7F4A7C15u);
  v ^= v >> 15; v *= 0x2C1B3C6Du; v ^= v >> 12;
  return v;
}

//a spawn x that keeps clear water between this leaf and the others
static uint16_t leaf_spawn_x(uint8_t self, uint32_t seed) {
  for (uint8_t attempt = 0; attempt < 8; attempt++) {
    uint16_t x = (uint16_t)(20 + river_hash(seed, attempt) % 350);
    uint8_t ok = 1;
    for (uint8_t j = 0; j < MAX_LEAVES; j++) {
      if (j == self) continue;
      int16_t dx = (int16_t)(x - leaves.base_x[j]);
      int16_t dy = (int16_t)(leaves.base_y[j] - 458); //vs entry band
      if (dx < 0) dx = -dx;
      if (dy < 0) dy = -dy;
      if (dy > 256) dy = (int16_t)(512 - dy);
      if (dx < 44 && dy < 56) { ok = 0; break; }
    }
    if (ok) return x;
  }
  return (uint16_t)(20 + river_hash(seed, 99) % 350);
}

//in the river the positions are rolled at spawn; defs give each slot its
//pad type (two big ones, a couple of lotuses, assorted sizes)
typedef struct { uint8_t tile; uint8_t big; } leaf_def;
static const leaf_def leaf_defs[MAX_LEAVES] = {
  { LEAF_TILE_MEDIUM, 1 }, { LEAF_TILE_SMALL, 0 }, { LEAF_TILE_LOTUS, 0 },
  { LEAF_TILE_TINY, 0 },   { LEAF_TILE_MEDIUM, 0 }, { LEAF_TILE_SMALL, 0 },
  { LEAF_TILE_MEDIUM, 1 }, { LEAF_TILE_LOTUS, 0 }, { LEAF_TILE_TINY, 0 },
  { LEAF_TILE_SMALL, 0 },
};

//gentle circular drift, a quarter phase apart on each axis
static const int8_t bob_table[16] =
  {0, 1, 1, 2, 2, 2, 1, 1, 0, -1, -1, -2, -2, -2, -1, -1};

/* weeds: rooted tuft on the floor map + a swaying frond sprite above it,
   in tandem — anchors match the tuft cells the generator paints */
#define MAX_FRONDS 10
static const uint16_t frond_sites[MAX_FRONDS][2] = {
  {2*16, 3*16}, {3*16, 11*16}, {10*16, 2*16}, {17*16, 8*16},
  {12*16, 13*16}, {18*16, 13*16}, {1*16, 7*16}, {8*16, 9*16},
  {22*16, 5*16}, {24*16, 12*16},
};
static uint8_t frond_sprite[MAX_FRONDS];

static void update_fronds(uint32_t frame) {
  for (uint8_t i=0; i<MAX_FRONDS; i++) {
    uint8_t t = (uint8_t)(((frame>>3) + i*3) & 15);
    set_pos_fsp(&GFX, frond_sprite[i],
                (int16_t)(frond_sites[i][0] + (bob_table[t]>>1)),
                (int16_t)(frond_sites[i][1] - 12)); //fronds reach upward
    set_fsp(&GFX, frond_sprite[i],
            (int16_t)((((frame>>4) + i) & 1) ? FROND_TILE_B : FROND_TILE_A));
  }
}


static uint8_t tune_shadows_hint = 1; //mirrors the tuner's SHADOWS flag
static uint8_t leaf_bob_amp = 2;    //0..4; big pads bob at half of it
static uint8_t leaf_shadow_gap = 12;//shadow offset in pixels (up to 36)
static uint8_t leaf_shadow_anim = 0;//0 rigid, 1 swaying, 2 anchored
static uint8_t koi_count_hint = 8;  //active fish in the pool
static uint8_t koi_growth_hint = 2; //meals per growth stage, 0 = off
static uint8_t koi_speed_hint = 10; //swim speed, 1/8 px units
static uint8_t river_flow_hint = 2; //current strength, 0..6

//enable/disable a half sprite without releasing its OAM slot
static void inline hsp_set_enabled(uint8_t sp_id, uint8_t enabled) {
  if (enabled) GFX.hsp.oam[sp_id] |= Mask_hsp_oam_enable;
  else GFX.hsp.oam[sp_id] &= (uint16_t)(~Mask_hsp_oam_enable);
}

//enable/disable a full sprite without releasing its OAM slot
static void inline fsp_set_enabled(uint8_t sp_id, uint8_t enabled) {
  if (enabled) GFX.fsp.oam[sp_id] |= Mask_fsp_oam_enable;
  else GFX.fsp.oam[sp_id] &= (uint16_t)(~Mask_fsp_oam_enable);
}

static void update_leaves(uint32_t frame) {
  for (uint8_t i=0; i<MAX_LEAVES; i++) {
    //downstream drift; past the bottom margin the pad slips away and a
    //fresh one enters above the top of the frame (positions wrap at 512)
    leaves.dfrac[i] = (uint8_t)(leaves.dfrac[i]
                                + ((leaves.drift[i]*river_flow_hint)>>1));
    if (leaves.dfrac[i] >= 32) {
      leaves.dfrac[i] -= 32;
      leaves.base_y[i] = (uint16_t)((leaves.base_y[i] + 1) & 511);
    }
    if (leaves.base_y[i] > 264 && leaves.base_y[i] < 440) {
      //448..471: even a big pad ends at 503, safely short of the wrap
      leaves.base_y[i] = (uint16_t)(448 + river_hash(frame, i) % 24);
      leaves.base_x[i] = leaf_spawn_x(i, frame + i * 7919u);
      leaves.drift[i] = 3; //one current for everyone: spacing persists
    }
    uint8_t t = (uint8_t)(((frame>>3) + leaves.phase[i]) & 15);
    //big pads sit heavier in the water: half the bob of the small ones
    uint8_t amp = leaf_defs[i].big ? (uint8_t)(leaf_bob_amp>>1)
                                   : leaf_bob_amp;
    int16_t lx = (int16_t)(leaves.base_x[i] + ((bob_table[t]*amp)>>1));
    int16_t ly = (int16_t)(leaves.base_y[i]
                           + ((bob_table[(t+4)&15]*amp)>>1));
    set_pos_fsp(&GFX, leaves.sprite[i], lx, ly);
    set_pos_fsp(&GFX, leaves.ring[i], lx, (int16_t)(ly+1));
    //the ring ripples: two dash frames, sized to the pad
    uint16_t ringt = (leaf_defs[i].tile == LEAF_TILE_TINY)
                     ? RING_SM_TILE : RING_TILE;
    set_fsp(&GFX, leaves.ring[i],
            (int16_t)(ringt + (((frame>>4) + i) & 1)));
    //pads ride the surface, above even the shallowest fish, so their
    //shadows displace farthest of all — same light, same direction
    int16_t lgap = (int16_t)((leaf_shadow_gap * 4) / 3);
    int16_t sx, sy;
    if (leaf_shadow_anim == 2) { //anchored: the leaf bobs over its shadow
      sx = (int16_t)(leaves.base_x[i] + lgap);
      sy = (int16_t)(leaves.base_y[i] + lgap);
    }
    else {
      sx = (int16_t)(lx + lgap);
      sy = (int16_t)(ly + lgap);
      if (leaf_shadow_anim == 1) //swaying: light refracts, shadow slides
        sx = (int16_t)(sx + (bob_table[((frame>>2)+i) & 15] >> 1));
    }
    set_pos_fsp(&GFX, leaves.shadow[i], sx, sy);
  }
}

#define HEAD_R 0
#define HEAD_L 1
#define HEAD_D 2
#define HEAD_U 3

typedef struct {
  uint8_t sprite[MAX_KOI];
  uint8_t shadow[MAX_KOI]; //floor shadow; its offset tracks depth
  uint8_t heading[MAX_KOI]; //displayed cardinal (nearest to theta)
  uint8_t phase[MAX_KOI];
  uint8_t size[MAX_KOI];  //0 fry, 1 adult, 2 elder (double-size)
  uint8_t fed[MAX_KOI];   //meals since the last growth spurt
  uint8_t depth[MAX_KOI]; //0 shallow, 1 mid, 2 deep (under both veils)
  uint8_t theta[MAX_KOI];  //actual swim direction, 8-bit angle
  uint8_t target[MAX_KOI]; //general direction being followed
  uint16_t wait[MAX_KOI];  //offstage frames before re-entering (0 = active)
  uint32_t xdata[MAX_KOI];
  uint32_t ydata[MAX_KOI];
} koi_struct;
static koi_struct koi;

//surface highlights: twinkling half sprites between the water texture
//and the leaves
#define MAX_SPARKS 10
#define SPARK_TILE 132 //2 twinkle frames: 132, 133
typedef struct {
  uint8_t sprite[MAX_SPARKS];
  uint8_t phase[MAX_SPARKS];
} spark_struct;
static spark_struct sparks;
static const uint16_t spark_defs[MAX_SPARKS][2] = {
  {36,20},{150,16},{262,60},{20,108},{104,152},
  {228,164},{56,224},{330,52},{366,140},{312,216},
};

static void update_sparks(uint32_t frame) {
  for (uint8_t i=0; i<MAX_SPARKS; i++) {
    uint8_t t = (uint8_t)(((frame>>4) + sparks.phase[i]) % 5);
    //twinkle: two frames plus a dark beat
    if (t == 4) {
      GFX.hsp.oam[sparks.sprite[i]] &= (uint16_t)(~Mask_hsp_oam_enable);
    }
    else {
      GFX.hsp.oam[sparks.sprite[i]] |= Mask_hsp_oam_enable;
      set_hsp(&GFX, sparks.sprite[i], (int16_t)(SPARK_TILE + (t & 1)));
    }
  }
}

typedef struct {
  uint8_t state[MAX_PELLETS];
  uint8_t sprite[MAX_PELLETS];
  uint16_t age[MAX_PELLETS]; //frames afloat; stale pellets sink
  uint32_t xdata[MAX_PELLETS];
  uint32_t ydata[MAX_PELLETS];
} pellet_struct;
static pellet_struct pellets;

typedef struct {
  uint8_t age[MAX_RIPPLES]; //0 = free slot
  uint8_t sprite[MAX_RIPPLES];
} ripple_struct;
static ripple_struct ripples;

static uint32_t hand_x, hand_y; //feeding cursor, 1/8 px units
static uint8_t hand_sprite;
static uint8_t pond_prev_input;

static void koi_face(uint8_t i) {
  uint8_t h = koi.heading[i];
  //NOTE: the double-size flag must be re-applied here — it lives in the
  //same OAM word as the flips, so a turn used to silently shrink elders
  set_fsp_effects(&GFX, koi.sprite[i],
                  (h==HEAD_L) || (h==HEAD_U),  //h-flip
                  h==HEAD_U,                   //v-flip
                  h>=HEAD_D,                   //rotation
                  koi.size[i] == 2);           //elders render double
}

/* The shadow answers to the fish above it: it turns with the heading
   (a fish swimming up casts a shadow pointing up), shrinks with depth
   and with the fish's own size. */
static void koi_shadow_look(uint8_t i) {
  uint8_t h = koi.heading[i];
  uint8_t deep = (uint8_t)(koi.depth[i] == 2);
  uint16_t tile = (koi.size[i] == 0 || deep) ? KOI_TILE_SHADOW_SM
                                             : KOI_TILE_SHADOW;
  set_fsp(&GFX, koi.shadow[i], (int16_t)tile);
  set_fsp_effects(&GFX, koi.shadow[i], 0, 0,
                  (uint8_t)(h >= HEAD_D),               //turn with the fish
                  (uint8_t)(koi.size[i] == 2 && !deep));//elders cast wide
}

static void spawn_ripple(uint16_t x_px, uint16_t y_px) {
  for (uint8_t i=0; i<MAX_RIPPLES; i++) {
    if (ripples.age[i]) continue;
    uint8_t sp = add_hsp(&GFX, RIPPLE_TILE, 1, x_px-4, y_px-4);
    if (sp >= hsp_count) return;
    ripples.age[i] = 1;
    ripples.sprite[i] = sp;
    return;
  }
}

static void update_ripples(void) {
  for (uint8_t i=0; i<MAX_RIPPLES; i++) {
    if (!ripples.age[i]) continue;
    ripples.age[i]++;
    if (ripples.age[i] >= 48) {
      delete_hsp(&GFX, ripples.sprite[i]);
      ripples.age[i] = 0;
      continue;
    }
    set_hsp(&GFX, ripples.sprite[i],
            (int16_t)(RIPPLE_TILE + (ripples.age[i]>>4)));
  }
}

static void kill_pellet_quiet(uint8_t id) {
  delete_hsp(&GFX, pellets.sprite[id]);
  pellets.state[id] = 0;
}

static void drop_pellet(uint16_t x_px, uint16_t y_px) {
  for (uint8_t i=0; i<MAX_PELLETS; i++) {
    if (pellets.state[i]) continue;
    uint8_t sp = add_hsp(&GFX, PELLET_TILE, 0, x_px-4, y_px-4);
    if (sp >= hsp_count) return;
    pellets.state[i] = 1;
    pellets.sprite[i] = sp;
    pellets.age[i] = 0;
    pellets.xdata[i] = 0; pellets.ydata[i] = 0;
    body_set_pos(&pellets.xdata[i], (uint16_t)(x_px<<3));
    body_set_pos(&pellets.ydata[i], (uint16_t)(y_px<<3));
    sfx_play(SFX_PLOP);
    spawn_ripple(x_px, y_px);
    return;
  }
}

#define KOI_SPEED 10 //1/8 px units per frame

static void update_koi(uint32_t frame) {
  //stale pellets sink out of reach of stubborn geometry
  for (uint8_t p=0; p<MAX_PELLETS; p++) {
    if (!pellets.state[p]) continue;
    body_set_pos(&pellets.ydata[p],
                 (uint16_t)(body_get_pos(&pellets.ydata[p])
                            + (river_flow_hint>>1)));
    set_pos_hsp(&GFX, pellets.sprite[p],
                (int16_t)((body_get_pos(&pellets.xdata[p])>>3)-4),
                (int16_t)((body_get_pos(&pellets.ydata[p])>>3)-4));
    if (body_get_pos(&pellets.ydata[p]) > (260<<3)
        && body_get_pos(&pellets.ydata[p]) < (480<<3)) {
      kill_pellet_quiet(p); //washed downstream
      continue;
    }
    if (++pellets.age[p] >= 600) {
      spawn_ripple((uint16_t)(body_get_pos(&pellets.xdata[p])>>3),
                   (uint16_t)(body_get_pos(&pellets.ydata[p])>>3));
      kill_pellet_quiet(p);
    }
  }
  for (uint8_t i=0; i<MAX_KOI; i++) {
    //the tuner's KOI COUNT benches everyone above the line
    if (i >= koi_count_hint) {
      if (koi.wait[i] == 0) {
        koi.wait[i] = 0xFFFF;
        fsp_set_enabled(koi.sprite[i], 0);
        fsp_set_enabled(koi.shadow[i], 0);
      }
      continue;
    }
    if (koi.wait[i] == 0xFFFF) koi.wait[i] = 60; //benched fish return
    //offstage fish wait in the wings, then slip in from an edge —
    //usually upstream, sometimes a side or from below
    if (koi.wait[i]) {
      if (--koi.wait[i] == 0) {
        uint32_t h = river_hash(frame, i * 31u);
        uint8_t side = (uint8_t)(h & 7);
        uint16_t sx, sy;
        uint8_t th;
        if (side < 5)      { sx = (uint16_t)(30+(h>>4)%340);
                             sy = (uint16_t)(450+((h>>12)%25));
                             th = 64; }  //above the top, drifts in
        else if (side == 5){ sx = (uint16_t)(460+((h>>12)%19));
                             sy = (uint16_t)(30+(h>>4)%180);
                             th = 0; }   //past the left edge
        else if (side == 6){ sx = (uint16_t)(408+((h>>12)%12));
                             sy = (uint16_t)(30+(h>>4)%180);
                             th = 128; } //past the right edge
        else               { sx = (uint16_t)(30+(h>>4)%340);
                             sy = (uint16_t)(248+((h>>12)%10));
                             th = 192; } //below the bottom
        body_set_pos(&koi.xdata[i], (uint16_t)(sx<<3));
        body_set_pos(&koi.ydata[i], (uint16_t)(sy<<3));
        koi.theta[i] = (uint8_t)(th + ((h>>12) & 31) - 16);
        koi.target[i] = koi.theta[i];
        koi.depth[i] = (uint8_t)((h>>16) % 3);
        set_fsp_priority(&GFX, koi.sprite[i],
                         (uint8_t)(PRIO_KOI_SHALLOW - koi.depth[i]));
        fsp_set_enabled(koi.sprite[i], 1);
        fsp_set_enabled(koi.shadow[i], tune_shadows_hint);
      }
      continue;
    }
    {
      uint16_t px = body_get_pos(&koi.xdata[i]) >> 3;
      uint16_t py = body_get_pos(&koi.ydata[i]) >> 3;
      if ((px > 424 && px < 456) || (py > 264 && py < 440)) {
        //slipped out of frame: rest, then return
        koi.wait[i] = (uint16_t)(90 + river_hash(frame, i * 13u) % 300);
        fsp_set_enabled(koi.sprite[i], 0);
        fsp_set_enabled(koi.shadow[i], 0);
        continue;
      }
    }
    //slow dive/surface cycle, phase-shifted per fish: 0 -> 1 -> 2 -> 1 ->
    uint8_t stage = (uint8_t)((((frame>>9) + koi.phase[i])) & 3);
    uint8_t want_depth = (stage == 3) ? 1 : stage;
    uint16_t x = body_get_pos(&koi.xdata[i]);
    uint16_t y = body_get_pos(&koi.ydata[i]);
    //find the nearest pellet
    int8_t target = -1;
    int32_t best = 0x7FFFFFFF;
    for (uint8_t p=0; p<MAX_PELLETS; p++) {
      if (!pellets.state[p]) continue;
      int32_t dx = (int32_t)body_get_pos(&pellets.xdata[p]) - x;
      int32_t dy = (int32_t)body_get_pos(&pellets.ydata[p]) - y;
      int32_t d = (dx<0?-dx:dx) + (dy<0?-dy:dy);
      if (d < best) { best = d; target = (int8_t)p; }
    }
    /* Swimming: each koi follows a general direction (target) that it
       keeps for a few seconds before a deterministic hash picks the
       next one; the actual heading turns toward it at a limited rate
       and a sine wiggle sways around it, so paths curve and meander
       instead of snapping between cardinals. */
    if (target >= 0) {
      int32_t dx = (int32_t)body_get_pos(&pellets.xdata[target]) - x;
      int32_t dy = (int32_t)body_get_pos(&pellets.ydata[target]) - y;
      koi.target[i] = angle_toward(dx, dy);
      want_depth = 0; //food floats: rise to the surface to eat
      if (best < ((10 + koi.size[i]*4)<<3)) { //bigger mouths reach farther
        //the ripple belongs to the food, not to the fish's corner
        uint16_t bite_x = (uint16_t)(body_get_pos(&pellets.xdata[target])>>3);
        uint16_t bite_y = (uint16_t)(body_get_pos(&pellets.ydata[target])>>3);
        delete_hsp(&GFX, pellets.sprite[target]);
        pellets.state[target] = 0;
        sfx_play(SFX_BLIP);
        spawn_ripple(bite_x, bite_y);
        //a well-fed koi grows: fry -> adult -> elder
        if (koi_growth_hint && koi.size[i] < 2
            && ++koi.fed[i] >= koi_growth_hint) {
          koi.fed[i] = 0;
          koi.size[i]++;
          koi_face(i);
          koi_shadow_look(i);
          sfx_play(SFX_PLOP);
        }
      }
    }
    else if (((frame + (uint32_t)koi.phase[i]*53u) & 255u) == 0) {
      //a new general direction every ~4 s, staggered per fish
      uint32_t h = ((frame>>8)*2654435761u) ^ ((uint32_t)i*0x9E3779B9u);
      koi.target[i] = (uint8_t)(h >> 24);
    }
    //apply the depth (feeding overrides the dive cycle)
    if (want_depth != koi.depth[i]) {
      koi.depth[i] = want_depth;
      set_fsp_priority(&GFX, koi.sprite[i],
                       (uint8_t)(PRIO_KOI_SHALLOW - want_depth));
      //only submerged fish blend with the water; shallow ones stay crisp
      set_fsp_blend(&GFX, koi.sprite[i], (uint8_t)(want_depth > 0));
      koi_shadow_look(i);
    }
    //no walls in a river: gently favor swimming with the current
    if (target < 0 && ((frame + i*17u) & 63u) == 0
        && sin8(koi.target[i]) < 0) {
      koi.target[i] = (uint8_t)(64 + (int8_t)((river_hash(frame, i) & 63))
                                - 32); //re-aim loosely downstream
    }
    //turn toward the target at a limited rate (shortest way around)
    int8_t diff = (int8_t)(koi.target[i] - koi.theta[i]);
    if (diff > 2) diff = 2;
    else if (diff < -2) diff = -2;
    koi.theta[i] = (uint8_t)(koi.theta[i] + (uint8_t)diff);
    //the wiggle: sway around the heading, phase-shifted per fish
    uint8_t th = (uint8_t)(koi.theta[i]
                 + (sin8((uint8_t)((frame<<1) + koi.phase[i]*40)) >> 3));
    //display: snap the art to the nearest cardinal
    static const uint8_t quad_to_head[4] = { HEAD_R, HEAD_D, HEAD_L, HEAD_U };
    uint8_t head = quad_to_head[((uint8_t)(th + 32)) >> 6];
    if (head != koi.heading[i]) {
      koi.heading[i] = head;
      koi_face(i);
      koi_shadow_look(i);
    }
    //swim along theta (speed scaled from the 1/64-unit sine table);
    //the big ones cruise a touch slower and statelier
    //fry dart, elders cruise
    uint8_t spd = (uint8_t)(koi_speed_hint + 2 - 2*koi.size[i]);
    int8_t vx = (int8_t)(((int16_t)cos8(th) * spd) >> 6);
    int8_t vy = (int8_t)(((int16_t)sin8(th) * spd) >> 6);
    body_set_vel(&koi.xdata[i], vx);
    body_set_vel(&koi.ydata[i], vy);
    body_update(&koi.xdata[i], &koi.ydata[i]);
    //the current carries every fish a little downstream
    body_set_pos(&koi.ydata[i],
                 (uint16_t)(body_get_pos(&koi.ydata[i])
                            + (river_flow_hint>>1)));
    int16_t fx = (int16_t)((body_get_pos(&koi.xdata[i])+4)>>3);
    int16_t fy = (int16_t)((body_get_pos(&koi.ydata[i])+4)>>3);
    set_pos_fsp(&GFX, koi.sprite[i], fx, fy);
    //the shadow falls farther from the fish the nearer the surface it
    //is: full displacement shallow, 2/3 mid, 1/3 deep — rising and
    //diving reads directly off the shadow's travel
    int16_t off = (int16_t)((leaf_shadow_gap
                             * (uint8_t)(3 - koi.depth[i])) / 3);
    set_pos_fsp(&GFX, koi.shadow[i],
                (int16_t)(fx + off), (int16_t)(fy + off));
    //tail flap, phase-shifted per fish; fry use the small frames
    uint16_t base = koi.size[i] ? KOI_TILE : KOI_TILE_SM;
    set_fsp(&GFX, koi.sprite[i],
            (int16_t)(base + (((frame>>3) + koi.phase[i]) & 1)));
  }
}

//allow_input == 0 keeps the world alive but ignores the hand (used while
//the tuning overlay owns the controls)
static void update_pond(uint32_t frame, uint8_t allow_input) {
  uint8_t input = players.base[0] & MASK_PLAYER_BASE_INPUT;
  uint8_t edge = input & (uint8_t)(~pond_prev_input);
  pond_prev_input = input;
  if (allow_input) {
    //the feeding hand
    if (input & MASK_INPUT_LEFT)  hand_x -= 24;
    if (input & MASK_INPUT_RIGHT) hand_x += 24;
    if (input & MASK_INPUT_UP)    hand_y -= 24;
    if (input & MASK_INPUT_DOWN)  hand_y += 24;
    //clamped to the region the koi can actually reach (their edge
    //avoidance turns them at 24..288 x 24..208)
    if ((int32_t)hand_x < (20<<3)) hand_x = 20<<3;
    if (hand_x > (372<<3)) hand_x = 372<<3;
    if ((int32_t)hand_y < (20<<3)) hand_y = 20<<3;
    if (hand_y > (204<<3)) hand_y = 204<<3;
    set_pos_hsp(&GFX, hand_sprite,
                (int16_t)((hand_x>>3)-4), (int16_t)((hand_y>>3)-4));
    if (edge & MASK_INPUT_A) {
      drop_pellet((uint16_t)(hand_x>>3), (uint16_t)(hand_y>>3));
    }
  }
  update_koi(frame);
  update_leaves(frame);
  update_fronds(frame);
  if (allow_input) update_sparks(frame); //paused under the debug menu
  update_ripples();
}

static void begin_pond(void) {
  clear_all_fsp(&GFX);
  clear_all_hsp(&GFX);
  initialize_players(); //players idle; only the input byte is used
  //leaves first, spread down the whole 512-tall river so the frame
  //starts populated; they drift in from the top thereafter
  for (uint8_t i=0; i<MAX_LEAVES; i++) {
    const leaf_def* d = &leaf_defs[i];
    leaves.base_y[i] = (uint16_t)((i * 51 + 8) & 511);
    if (leaves.base_y[i] > 264 && leaves.base_y[i] < 440)
      leaves.base_y[i] = (uint16_t)(leaves.base_y[i] + 200) & 511;
    leaves.base_x[i] = leaf_spawn_x(i, i * 7919u);
    leaves.drift[i] = 3;
    leaves.dfrac[i] = 0;
    uint8_t sp = add_fsp(&GFX, d->tile, LEAF_PAL,
                         leaves.base_x[i], leaves.base_y[i]);
    if (d->big) set_fsp_effects(&GFX, sp, 0, 0, 0, 1);
    set_fsp_priority(&GFX, sp, PRIO_LEAF);
    leaves.sprite[i] = sp;
    uint8_t rg = add_fsp(&GFX,
                         (d->tile == LEAF_TILE_TINY) ? RING_SM_TILE
                                                     : RING_TILE,
                         LEAF_PAL,
                         leaves.base_x[i], (uint16_t)(leaves.base_y[i]+1));
    if (d->big) set_fsp_effects(&GFX, rg, 0, 0, 0, 1);
    set_fsp_priority(&GFX, rg, PRIO_SPARKLE);
    leaves.ring[i] = rg;
    leaves.phase[i] = (uint8_t)(i*5+2);
  }
  for (uint8_t i=0; i<MAX_KOI; i++) {
    uint16_t px = (uint16_t)(50 + i*52);
    uint16_t py = (uint16_t)(60 + ((i*77)%120));
    uint8_t sp = add_fsp(&GFX, KOI_TILE,
                         (i&1) ? KOI_PAL_CALICO : KOI_PAL_ORANGE, px, py);
    koi.sprite[i] = sp;
    set_fsp_priority(&GFX, sp, PRIO_KOI_SHALLOW);
    uint8_t ksh = add_fsp(&GFX, KOI_TILE_SHADOW, LEAF_PAL,
                          (uint16_t)(px+6), (uint16_t)(py+6));
    set_fsp_priority(&GFX, ksh, PRIO_SHADOW);
    koi.shadow[i] = ksh;
    //a mixed school: fry, adults, and a couple of elders
    koi.size[i] = (uint8_t)((i % 4 == 3) ? 2 : ((i % 4 == 0) ? 0 : 1));
    koi.fed[i] = 0;
    koi.wait[i] = (uint16_t)((i >= 3) ? 120 + i*90 : 0); //staggered entries
    if (koi.wait[i]) {
      fsp_set_enabled(sp, 0);
      fsp_set_enabled(ksh, 0);
    }
    koi.depth[i] = 0;
    koi.heading[i] = (i&1) ? HEAD_L : HEAD_R;
    koi.theta[i] = (uint8_t)((i&1) ? 128 : 0);
    koi.target[i] = (uint8_t)(i * 51);
    koi.phase[i] = (uint8_t)(i*3+1);
    koi_face(i);
    koi_shadow_look(i);
    koi.xdata[i] = 0; koi.ydata[i] = 0;
    body_set_pos(&koi.xdata[i], (uint16_t)(px<<3));
    body_set_pos(&koi.ydata[i], (uint16_t)(py<<3));
    koi_face(i);
  }
  //leaf shadows last: highest OAM slots render beneath the koi
  for (uint8_t i=0; i<MAX_LEAVES; i++) {
    const leaf_def* d = &leaf_defs[i];
    uint8_t sh = add_fsp(&GFX, LEAF_TILE_SHADOW, LEAF_PAL,
                         (uint16_t)(leaves.base_x[i]+4),
                         (uint16_t)(leaves.base_y[i]+4));
    if (d->big) set_fsp_effects(&GFX, sh, 0, 0, 0, 1);
    set_fsp_priority(&GFX, sh, PRIO_SHADOW);
    leaves.shadow[i] = sh;
  }
  for (uint8_t i=0; i<MAX_FRONDS; i++) {
    uint8_t fs = add_fsp(&GFX, FROND_TILE_A, LEAF_PAL,
                         frond_sites[i][0],
                         (uint16_t)(frond_sites[i][1]-12));
    set_fsp_priority(&GFX, fs, PRIO_KOI_SHALLOW);
    frond_sprite[i] = fs;
  }
  for (uint8_t i=0; i<MAX_SPARKS; i++) {
    uint8_t sp = add_hsp(&GFX, SPARK_TILE, 1,
                         spark_defs[i][0], spark_defs[i][1]);
    set_hsp_priority(&GFX, sp, PRIO_SPARKLE);
    sparks.sprite[i] = sp;
    sparks.phase[i] = (uint8_t)(i*7+3);
  }
  for (uint8_t i=0; i<MAX_PELLETS; i++) pellets.state[i] = 0;
  for (uint8_t i=0; i<MAX_RIPPLES; i++) ripples.age[i] = 0;
  hand_x = 160<<3;
  hand_y = 120<<3;
  hand_sprite = add_hsp(&GFX, '+', 2, 156, 116);
  pond_prev_input = 0xFF;
  game_mode = MODE_PLAYING;
}

/* ---- scene-select menu ---- */

#define SCENE_COUNT  3

static uint8_t menu_cursor = 0;
static uint8_t current_scene = 0;
static uint8_t menu_prev_input = 0;
static uint8_t play_prev_input = 0;
static uint8_t menu_cursor_sprite = 0;

#define MENU_OPT_X 144
#define MENU_OPT_Y 120
#define MENU_OPT_SPACING 20

//the menu draws over whatever background is loaded, which keeps scrolling
static void enter_menu(void) {
  clear_all_fsp(&GFX);
  clear_all_hsp(&GFX);
  draw_text(&GFX, "TILE ENGINE RETRO", 132, 60, 2);
  draw_text(&GFX, "ASTEROID RUN", MENU_OPT_X, MENU_OPT_Y, 0);
  draw_text(&GFX, "CRYSTAL CAVERN", MENU_OPT_X, MENU_OPT_Y+MENU_OPT_SPACING, 0);
  draw_text(&GFX, "KOI POND", MENU_OPT_X, MENU_OPT_Y+2*MENU_OPT_SPACING, 0);
  //tight 7 px advance: the full credit fits the 320 px viewport
  {
    const char* credit = "POR DANIEL JIMENEZ Y LUIS NAVARRO 2017-2026";
    int16_t cx = 50;
    for (const char* p = credit; *p; p++, cx += 7) {
      if (*p != ' ') add_hsp(&GFX, (uint16_t)*p, 1, (uint16_t)cx, 208);
    }
  }
  menu_cursor_sprite = add_hsp(&GFX, '>', 2, MENU_OPT_X-12,
                               MENU_OPT_Y + menu_cursor*MENU_OPT_SPACING);
  menu_prev_input = 0xFF; //swallow buttons held on entry
  game_mode = MODE_MENU;
}

//returns the selected scene id, or 0xFF while still browsing
static uint8_t update_menu(void) {
  uint8_t input = players.base[0] & MASK_PLAYER_BASE_INPUT;
  uint8_t edge = input & (uint8_t)(~menu_prev_input);
  menu_prev_input = input;
  if (edge & MASK_INPUT_UP)
    menu_cursor = (menu_cursor + SCENE_COUNT - 1) % SCENE_COUNT;
  if (edge & MASK_INPUT_DOWN)
    menu_cursor = (menu_cursor + 1) % SCENE_COUNT;
  set_pos_hsp(&GFX, menu_cursor_sprite, MENU_OPT_X-12,
              MENU_OPT_Y + menu_cursor*MENU_OPT_SPACING);
  if (edge & (MASK_INPUT_A | MASK_INPUT_START)) return menu_cursor;
  return 0xFF;
}

//in play mode, START (edge) returns to the menu
static uint8_t play_wants_menu(void) {
  uint8_t input = players.base[0] & MASK_PLAYER_BASE_INPUT;
  uint8_t edge = input & (uint8_t)(~play_prev_input);
  play_prev_input = input;
  return (edge & MASK_INPUT_START) != 0;
}

//spawns everything for a fresh play session (scene assets already loaded)
static void begin_play(void) {
  clear_all_fsp(&GFX);
  clear_all_hsp(&GFX);
  initialize_players();
  initialize_enemies();
  initialize_player_projectiles();
  initialize_enemy_projectiles();
  initialize_powerups();
  tl_index = 0;
  tl_base = 0;
  add_hud();
  add_player(0, 20<<3, 200<<3);
  add_player(1, 60<<3, 200<<3);
  add_player(2, 100<<3, 200<<3);
  add_player(3, 140<<3, 200<<3);
  //first wave: three scouts and one double-size heavy, from off-screen
  add_enemy(410,  40, 0);
  add_enemy(420,  80, 0);
  add_enemy(405, 120, 0);
  add_enemy(415, 160, 1);
  play_prev_input = 0xFF; //swallow the button that started the scene
  game_mode = MODE_PLAYING;
}

static void initialize_game(void) {
  fprintf(stdout, "Iniciando juego\n");
  gamedata1 = 0x00000000;
  gamedata2 = 0x00000000;
  initialize_players();
  initialize_enemies();
  initialize_player_projectiles();
  initialize_enemy_projectiles();
  initialize_powerups();
  initialize_topscores();
  enter_menu();
}

#endif //GAME2_H
