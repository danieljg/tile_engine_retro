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

#define MASK_PLAYER_BASE_RES1  0xC000
#define MASK_PLAYER_BASE_STATE 0x3000 //(2 bits) idle, spawning, alive, falling
#define MASK_PLAYER_BASE_RES1  0x0800
#define MASK_PLAYER_BASE_LIVES 0x0700 //(3 bits) 8 lives max.
#define MASK_PLAYER_BASE_INPUT 0x00FF //(8 bits) 4 direction buttons, 3 action buttons, 1 Start button.

typedef struct {
  // Basic data
  uint16_t base[MAX_PLAYERS];
  uint16_t score[MAX_PLAYERS]; // player total score 16 bits
  // Physics Data
  uint32_t xdata[MAX_PLAYERS];
  uint32_t ydata[MAX_PLAYERS];
  uint16_t dimensions[MAX_PLAYERS];
  // Animation data
  uint32_t animation[MAX_PLAYERS];
  /*
  weapon_state weapon_A;
  weapon_state weapon_B;
  */
}
players_struct;

#define MASK_ENEMY_BASE_RES1     0xC000
#define MASK_ENEMY_BASE_STATE    0x3000 //(2 bits) idle, spawning, alive, falling
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
  /*
  weapon_state weapon;
  */
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
static void inline initialize_players() {
  for (uint8_t i=0; i<MAX_PLAYERS; i++) {
    players.base[i] =                0x0000;
    players.xdata[i] =           0x00000000;
    players.ydata[i] =           0x00000000;
    players.dimensions[i] =          0x0000;
    players.animation[i] =       0x00000000;
  }
}
static void inline add_player2(uint8_t player_id) {
  #define PLAYER_START_TILE 12
  players.base[player_id] =
    (0x1<<12) | //state 1: spawning
    (START_LIVES<<8);
  players.xdata[player_id] = 100;
  players.ydata[player_id] = 100;
  players.dimensions[player_id] = (10<<8) | 5;
  uint8_t sp_id = add_fsp(
    12, // spaceship sprite
    player_id+1, // sprite palete
    100, 100 // sprite coords
  );
  players.animation[player_id] =
    (3<<28) | //  total frames:   3
    (0<<24) | //  current frame:  0
    (12<<16) | // sp tile start: 12
    sp_id;        // sp index
}

enemies_struct enemies;
static void inline initialize_enemies() {
  for (uint8_t i=0; i<MAX_ENEMIES; i++) {
    enemies.base[i] =                0x0000;
    enemies.xdata[i] =           0x00000000;
    enemies.ydata[i] =           0x00000000;
    enemies.dimensions[i] =          0x0000;
    enemies.animation[i] =       0x00000000;
  }
}
player_projectiles_struct pprojectiles;
static void inline initialize_player_projectiles() {
  for (uint8_t i=0; i<MAX_PPROJECTILES; i++) {
    pprojectiles.base[i] =           0x0000;
    pprojectiles.xdata[i] =      0x00000000;
    pprojectiles.ydata[i] =      0x00000000;
    pprojectiles.dimensions[i] =     0x0000;
    pprojectiles.animation[i] =  0x00000000;
  }
}
enemy_projectiles_struct eprojectiles;
static void inline initialize_enemy_projectiles() {
  for (uint8_t i=0; i<MAX_EPROJECTILES; i++) {
    eprojectiles.base[i] =           0x0000;
    eprojectiles.xdata[i] =      0x00000000;
    eprojectiles.ydata[i] =      0x00000000;
    eprojectiles.dimensions[i] =     0x0000;
    eprojectiles.animation[i] =  0x00000000;
  }
}
power_up_struct power_ups;
static void inline initialize_powerups() {
  for (uint8_t i=0; i<MAX_POWERUPS; i++) {
    power_ups.base[i] =              0x0000;
    power_ups.xdata[i] =         0x00000000;
    power_ups.ydata[i] =         0x00000000;
    power_ups.dimensions[i] =        0x0000;
    power_ups.animation[i] =     0x00000000;
  }
}
hi_score_struct top_scores;
static void inline initialize_topscores() {
  for (uint8_t i=0; i<TOP_SCORES_COUNT; i++) {
    top_scores.initials[i] =     0x00000000;
    top_scores.score[i] =        0x00000000;
  }
}

static void initialize_game2() {
  fprintf(stdout, "Iniciando juego\n");
  gamedata1 = 0x00000000;
  gamedata2 = 0x00000000;
  initialize_players();
  initialize_enemies();
  initialize_player_projectiles();
  initialize_enemy_projectiles();
  initialize_powerups();
}
