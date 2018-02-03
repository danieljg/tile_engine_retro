#define MAX_PLAYERS 4
#define MAX_ENEMIES 8
#define MAX_PROJECTILES 64
#define MAX_POWERUPS 4
#define TOP_SCORES_COUNT 10

#define MASK_PB_XDATA_OFFSET 0xFF000000
#define MASK_PB_XDATA_VEL    0x00FFF000
#define MASK_PB_XDATA_POS    0x00000FFF

#define MASK_PB_YDATA_OFFSET 0xFF000000
#define MASK_PB_YDATA_VEL    0x00FFF000
#define MASK_PB_YDATA_POS    0x00000FFF

#define MASK_PB_DIMENSIONS_WIDTH  0xFF00
#define MASK_PB_DIMENSIONS_HEIGHT 0x00FF

typedef struct {
  uint32_t xdata;
  uint32_t ydata;
  uint16_t dimensions;
} physics_body;

typedef struct {
  uint8_t is_full_sprite; // 1 bit
  uint8_t sprite_id; // 5 bits
  uint16_t sprite_tile_start; // 10 bits
  uint8_t current_frame; // 4 bits
  uint8_t total_frames; // 4 bits
} sprite_animation;

typedef struct {
  uint8_t state; // live, dead, exploding 2 bits
  physics_body body;
  uint8_t damage; // (0-15) 4 bits
  uint8_t is_enemy; // is enemy or player projectile? 1 bit
  sprite_animation animation;
} projectile;

typedef struct {
  uint8_t state; // live, using, used
  physics_body body;
  uint8_t type; // 8 power_up types // 3 bits
  sprite_animation animation;
} power_up;

typedef struct {
  uint8_t projectile_type; // 4 projectile types 2 bits
  uint8_t sprite_tile_id;  // 10 bits
  uint8_t sprite_palette_id; // 4 bits
  uint8_t min_delay_time; // min delay between shots in frames 8 bits
  uint8_t is_chargeable; // 1 bit
} weapon_type;

typedef struct {
  uint8_t type_id; // 8 weapon types 3 bits
  uint8_t delay_counter; // frames left to shot again 8 bits
  uint8_t is_charging; // 1 bit
} weapon_state;

typedef struct {
  uint8_t speed;
  uint8_t weapon_type_id_A;
  uint8_t weapon_type_id_B;
  sprite_animation animation;
  uint8_t animation_data;
} player_type;

typedef struct {
  uint8_t state; // spawning, live, dead, exploding 2 bits
  uint8_t input_state; //4 direction buttons, 3 action buttons, 1 Start button.
  physics_body body;
  uint16_t score; // player total score 16 bits
  weapon_state weapon_A;
  weapon_state weapon_B;
  sprite_animation animation;
//TODO:ADD 3 bits for player lives
} player;

typedef struct {
  uint8_t total_hitpoints; // (0 to 64) 6 bits
  uint8_t speed;
  uint8_t animation_data;
  uint8_t weapon_type_id;
} enemy_type;

typedef struct {
  uint8_t state; // live, dead, exploding 2 bits
  uint8_t ai_state; // 4 states 2 bits
  physics_body body;
  uint8_t hitpoints; // (0 to 64) 6 bits
  weapon_state weapon;
  sprite_animation animation;
} enemy;

typedef struct {
  player players[MAX_PLAYERS];
  enemy enemies[MAX_ENEMIES];
  projectile projectiles[MAX_PROJECTILES];
  power_up power_ups[MAX_POWERUPS];
  uint8_t game_state;
  uint16_t scoreboard[TOP_SCORES_COUNT];
} game_control;

game_control game_ctrl;

void initialize_game2() {
  game_ctrl.game_state = 0x00;
  for (uint8_t i; i<TOP_SCORES_COUNT; i++) {
    game_ctrl.scoreboard[TOP_SCORES_COUNT] = 0;
  }
}
