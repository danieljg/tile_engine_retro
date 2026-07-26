//Game logic in structure-of-arrays style (layout by Luis Navarro).
//Each entity kind is one struct of parallel arrays; entity fields are
//bit-packed words manipulated through the MASK_* defines below.
#ifndef GAME2_H
#define GAME2_H

#include <stdint.h>
#include <stdio.h>

#include "gfx_engine.h"

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
  set_fsp(*anim&MASK_ANIMATION_SPINDEX,
           ((*anim&MASK_ANIMATION_TLESTRT)>>8)
          +((*anim&MASK_ANIMATION_CURRFRM)>>24) );
}

static uint8_t inline animation_sprite(const uint32_t* anim) {
  return *anim & MASK_ANIMATION_SPINDEX;
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
  uint8_t sp_id = add_fsp(
    PLAYER_START_TILE,
    player_id+1, // sprite palette (palettes 1-4 are ship palettes)
    (pos_x+4)>>3, (pos_y+4)>>3
  );
  if (sp_id >= fsp_count) return 0; //no free sprite slot
  set_fsp_effects(sp_id, 0, 0, 1, 0); //rotate +90: ship faces right
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

#define ENEMY_START_TILE 12
#define ENEMY_FRAMES 3
#define ENEMY_PALETTE 0

//enemies fly in from the right facing left (h-flip via the pre-mirrored
//tileset); double_size makes a 32x32 "heavy" using the render-time scaler
static uint8_t add_enemy(uint16_t pos_x_px, uint16_t pos_y_px,
                         uint8_t double_size) {
  for (uint8_t i=0; i<MAX_ENEMIES; i++) {
    if (enemy_state(i) != STATE_IDLE) continue;
    uint8_t sp_id = add_fsp(ENEMY_START_TILE, ENEMY_PALETTE,
                            pos_x_px, pos_y_px);
    if (sp_id >= fsp_count) return 0;
    //rotation+h_flip+v_flip = rotate -90: enemies face left
    set_fsp_effects(sp_id, 1, 1, 1, double_size);
    enemies.base[i] = STATE_ALIVE<<12;
    enemies.xdata[i] = 0; enemies.ydata[i] = 0;
    body_set_pos(&enemies.xdata[i], pos_x_px<<3);
    body_set_pos(&enemies.ydata[i], pos_y_px<<3);
    body_set_vel(&enemies.xdata[i], -3);
    enemies.dimensions[i] = double_size ? ((26<<8)|26) : ((13<<8)|13);
    animation_init(&enemies.animation[i], ENEMY_FRAMES, ENEMY_START_TILE, sp_id);
    return 1;
  }
  return 0;
}

static void kill_enemy(uint8_t id) {
  delete_fsp(animation_sprite(&enemies.animation[id]));
  enemies.base[id] = 0x0000;
  enemies.animation[id] = 0x00000000;
}

static void update_enemies(void) {
  for (uint8_t i=0; i<MAX_ENEMIES; i++) {
    if (enemy_state(i) != STATE_ALIVE) continue;
    body_update(&enemies.xdata[i], &enemies.ydata[i]);
    uint16_t x = body_get_pos(&enemies.xdata[i]);
    if ((x>>3) < 8) { //wrap back to the right edge
      body_set_pos(&enemies.xdata[i], 310<<3);
      x = body_get_pos(&enemies.xdata[i]);
    }
    set_pos_fsp(animation_sprite(&enemies.animation[i]),
                (x+4)>>3, (body_get_pos(&enemies.ydata[i])+4)>>3);
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
    uint8_t sp_id = add_hsp(PPROJECTILE_TILE, PPROJECTILE_PALETTE,
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
    return;
  }
}

static void kill_pprojectile(uint8_t id) {
  delete_hsp(pprojectiles.sprite_id[id]);
  pprojectiles.base[id] = 0x0000;
}

static void update_pprojectiles(void) {
  for (uint8_t i=0; i<MAX_PPROJECTILES; i++) {
    if (pprojectile_state(i) != STATE_ALIVE) continue;
    body_update(&pprojectiles.xdata[i], &pprojectiles.ydata[i]);
    uint16_t x = body_get_pos(&pprojectiles.xdata[i]);
    if ((x>>3) > 336) { //off the right edge
      kill_pprojectile(i);
      continue;
    }
    set_pos_hsp(pprojectiles.sprite_id[i],
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
  for (uint8_t ii=0; ii<5; ii++) add_hsp('0', 0, 204+ii*8, 230);
  for (uint8_t ii=0; ii<5; ii++) add_hsp('0', 0, 252+ii*8, 230);
  //hi-score digits (slots 10 to 15)
  for (uint8_t ii=0; ii<6; ii++) add_hsp('0', 0, 84+ii*8, 5);
  draw_text("Hi-Score", 4, 4, 2);
}

static void inline update_score_hud(void) {
  uint32_t s = players.score[0];
  for (uint8_t i=0; i<5; i++) { set_hsp(4-i, ASCII0 + s%10); s/=10; }
  s = players.score[1];
  for (uint8_t i=0; i<5; i++) { set_hsp(9-i, ASCII0 + s%10); s/=10; }
}

static void inline update_hiscore(uint32_t score) {
  for (uint8_t i=0; i<6; i++) {
    set_hsp(15-i, ASCII0 + score%10);
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
  set_pos_fsp(animation_sprite(&players.animation[id]),
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
      }
    }
  }
}

//keeps the wave populated: an idle slot respawns every couple of seconds
//(state at file scope so save states can capture it)
static uint16_t spawner_wait = 0;
static uint8_t spawner_lane = 0;

static void update_enemy_spawner(void) {
  spawner_wait++;
  if (spawner_wait < 120) return;
  spawner_wait = 0;
  spawner_lane = (spawner_lane+1) & 0x3;
  add_enemy(300+(spawner_lane<<1), 40+spawner_lane*40, spawner_lane==3);
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
  add_hud();
  add_player(0, 20<<3, 200<<3);
  add_player(1, 60<<3, 200<<3);
  add_player(2, 100<<3, 200<<3);
  add_player(3, 140<<3, 200<<3);
  //first wave: three scouts and one double-size heavy
  add_enemy(300,  40, 0);
  add_enemy(310,  80, 0);
  add_enemy(290, 120, 0);
  add_enemy(305, 160, 1);
}

#endif //GAME2_H
