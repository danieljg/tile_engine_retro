#define MAX_PLAYERS 4
#define MAX_ENEMIES 8
#define MAX_PROJECTILES 64
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

#define MASK_PB_XDATA_OFFSET 0xFF000000
#define MASK_PB_XDATA_DIR    0x00800000
#define MASK_PB_XDATA_RES    0x00700000
#define MASK_PB_XDATA_SPD    0x000FF000
#define MASK_PB_XDATA_POS    0x00000FFF

#define MASK_PB_YDATA_OFFSET 0xFF000000
#define MASK_PB_YDATA_DIR    0x00800000
#define MASK_PB_YDATA_RES    0x00700000
#define MASK_PB_YDATA_SPD    0x000FF000
#define MASK_PB_YDATA_POS    0x00000FFF

#define MASK_PB_DIMENSIONS_WIDTH  0xFF00
#define MASK_PB_DIMENSIONS_HEIGHT 0x00FF

/* physics_body
*/
typedef struct {
  uint32_t xdata;
  uint32_t ydata;
  uint16_t dimensions;
}
physics_body;
void pbody_set_x(physics_body *pbody, uint16_t pos_x) {
  pbody->xdata=(pbody->xdata&(~MASK_PB_XDATA_POS))|(pos_x&MASK_PB_XDATA_POS);
}
uint16_t pbody_get_x(physics_body *pbody) {
  return pbody->xdata&MASK_PB_XDATA_POS;
}
void pbody_set_y(physics_body *pbody, uint16_t pos_y) {
  pbody->ydata=(pbody->ydata&(~MASK_PB_YDATA_POS))|(pos_y&MASK_PB_YDATA_POS);
}
uint16_t pbody_get_y(physics_body *pbody) {
  return pbody->ydata&MASK_PB_YDATA_POS;
}
void pbody_set_vel_x(physics_body *pbody, int16_t vel_x) {
  pbody->xdata =
    (pbody->xdata&(~MASK_PB_XDATA_SPD))|((vel_x<<12)&MASK_PB_XDATA_SPD);
}
int16_t pbody_get_vel_x(physics_body *pbody) {
  return (pbody->xdata&MASK_PB_XDATA_SPD)>>12;
}
void pbody_set_vel_y(physics_body *pbody, int16_t vel_y) {
  pbody->ydata =
    (pbody->ydata&(~MASK_PB_YDATA_SPD))|((vel_y<<12)&MASK_PB_YDATA_SPD);
}
int16_t pbody_get_vel_y(physics_body *pbody) {
  return (pbody->ydata&MASK_PB_YDATA_SPD)>>12;
}
void pbody_update(physics_body *pbody) {
  pbody_set_x(pbody, pbody_get_x(pbody)+pbody_get_vel_x(pbody));
  pbody_set_y(pbody, pbody_get_y(pbody)+pbody_get_vel_y(pbody));
}


typedef struct {
  uint8_t is_full_sprite; // 1 bit
  uint8_t sprite_id; // 5 bits
  uint16_t sprite_tile_start; // 10 bits
  uint8_t current_frame; // 4 bits
  uint8_t total_frames; // 4 bits
}
sprite_animation;
animation_update(sprite_animation *animation) {
  // updating relative frame
  if (animation->current_frame < (animation->total_frames-1))
    animation->current_frame++;
  else
    animation->current_frame = 0;
  // updating sprite
  if (animation->is_full_sprite) {
    set_full_sprite(
      animation->sprite_id,
      animation->sprite_tile_start + animation->current_frame
    );
  }
  else {
    set_half_sprite(
      animation->sprite_id,
      animation->sprite_tile_start + animation->current_frame
    );
  }
}
animation_set_pos(
  sprite_animation *animation,
  uint16_t pos_x, uint16_t pos_y
  ){
    if (animation->is_full_sprite) {
      full_sprite_set_pos(animation->sprite_id, pos_x, pos_y);
    }
    else {
    }
}

typedef struct {
  uint8_t state; // live, dead, exploding 2 bits
  physics_body body;
  uint8_t damage; // (0-15) 4 bits
  uint8_t is_enemy; // is enemy or player projectile? 1 bit
  sprite_animation animation;
}
projectile;

typedef struct {
  uint8_t state; // live, using, used
  physics_body body;
  uint8_t type; // 8 power_up types // 3 bits
  sprite_animation animation;
}
power_up;

typedef struct {
  uint8_t projectile_type; // 4 projectile types 2 bits
  uint8_t sprite_tile_id;  // 10 bits
  uint8_t sprite_palette_id; // 4 bits
  uint8_t min_delay_time; // min delay between shots in frames 8 bits
  uint8_t is_chargeable; // 1 bit
}
weapon_type;

typedef struct {
  uint8_t type_id; // 8 weapon types 3 bits
  uint8_t delay_counter; // frames left to shot again 8 bits
  uint8_t is_charging; // 1 bit
}
weapon_state;

typedef struct {
  uint8_t speed;
  uint8_t weapon_type_id_A;
  uint8_t weapon_type_id_B;
  sprite_animation animation;
  uint8_t animation_data;
}
player_type;

typedef struct {
  uint8_t state; // spawning, live, dead, exploding 2 bits
  uint8_t input_state; //4 direction buttons, 3 action buttons, 1 Start button.
  physics_body body;
  uint8_t lives; //(0-8) 3 bits
  uint16_t score; // player total score 16 bits
  weapon_state weapon_A;
  weapon_state weapon_B;
  sprite_animation animation;
}
player;

void update_player(player *plyr) {
  pbody_update(&(plyr->body));
  animation_set_pos(&(plyr->animation),
    pbody_get_x(&(plyr->body))>>3, pbody_get_y(&(plyr->body))>>3);
  animation_update(&(plyr->animation));
}

typedef struct {
  uint8_t total_hitpoints; // (0 to 64) 6 bits
  uint8_t speed;
  uint8_t animation_data;
  uint8_t weapon_type_id;
}
enemy_type;

typedef struct {
  uint8_t state; // live, dead, exploding 2 bits
  uint8_t ai_state; // 4 states 2 bits
  physics_body body;
  uint8_t hitpoints; // (0 to 64) 6 bits
  weapon_state weapon;
  sprite_animation animation;
}
enemy;

typedef struct {
  uint32_t initials; // 3 letters in ASCII (24 bits)
  uint32_t score;
}
hi_score;

typedef struct {
  uint8_t player_count;
  player players[MAX_PLAYERS];
  uint8_t enemy_count;
  enemy enemies[MAX_ENEMIES];
  uint8_t projectile_count;
  projectile projectiles[MAX_PROJECTILES];
  uint8_t powerup_count;
  power_up power_ups[MAX_POWERUPS];
  uint8_t game_state;
  hi_score top_scores[TOP_SCORES_COUNT];
}
game_control;

game_control game_ctrl;

uint8_t add_player(uint16_t pos_x, uint16_t pos_y) {
  if (game_ctrl.player_count < MAX_PLAYERS) {
    uint8_t new_id = game_ctrl.player_count;
    game_ctrl.players[new_id].lives = START_LIVES;
    pbody_set_x(&game_ctrl.players[new_id].body, pos_x);
    pbody_set_y(&game_ctrl.players[new_id].body, pos_y);
    game_ctrl.players[new_id].animation.is_full_sprite = 1;
    uint8_t new_sprite_id = add_full_sprite(12, 1+new_id, pos_x, pos_y);
    game_ctrl.players[new_id].animation.sprite_id = new_sprite_id;
    game_ctrl.players[new_id].animation.sprite_tile_start = 12;
    game_ctrl.players[new_id].animation.current_frame = 0;
    game_ctrl.players[new_id].animation.total_frames = 3;
    game_ctrl.player_count++;
    return 1;
  }
  else return 0;
}

void add_hud() {
  for (uint8_t ii=0; ii<3; ii++) add_half_sprite('0', 0, 294+ii*8, 230);
  for (uint8_t ii=0; ii<3; ii++) add_half_sprite('0', 0, 262+ii*8, 230);
  //hi-score digits (indexes 6 to 11)
  for (uint8_t ii=0; ii<6; ii++) add_half_sprite('0', 0, 84+ii*8, 5);
  draw_text("Hi-Score", 4, 4, 2);
}

void update_hud() {
  #define SHIP_ID 0
  int16_t met_x, met_y;
  met_y = fsp.oam2[SHIP_ID]&Mask_fsp_oam2_y_pos;
  met_x = fsp.oam3[SHIP_ID]&Mask_fsp_oam3_x_pos;
  update_coords(met_x,met_y);
  //fprintf(stdout, "Score:%u\n", game_ctrl.top_scores[0].score);
  update_hiscore(game_ctrl.top_scores[0].score);
}

void update_hiscore(uint32_t score) {
  #define ASCII0 48
  char digits[6];
  // Codifing Score to ASCII
  for (uint8_t i=0; i<6; i++) {
    digits[5-i] = ASCII0 + score%10;
    score=score/10;
  }
  // Updating sprites with indexes 6 to 11 (reserved for hi-score)
  for (uint8_t i=0; i<6; i++) {
    set_half_sprite(i+6, digits[i]);
  }
}

void update_coords(uint16_t x, uint16_t y) {
  #define ASCII0 48
  char digits[6];
  for (uint8_t i=0; i<3; i++) {
    digits[2-i] = ASCII0 + x%10;
    x=x/10;
  }
  for (uint8_t i=0; i<3; i++) {
    digits[5-i] = ASCII0 + y%10;
    y=y/10;
  }
  for (uint8_t i=0; i<6; i++) {
    set_half_sprite(i, digits[i]);
  }
}

void add_projectile() {
  uint8_t new_id = game_ctrl.projectile_count;
  if (new_id < MAX_PROJECTILES) {
    game_ctrl.projectiles[new_id].state = 0x01;
    game_ctrl.projectiles[new_id].body.xdata = 0x00;
    game_ctrl.projectiles[new_id].body.ydata = 0x00;
    game_ctrl.projectiles[new_id].body.dimensions = 0x00;
    game_ctrl.projectiles[new_id].damage = 5;
    game_ctrl.projectiles[new_id].is_enemy = 0;
    game_ctrl.projectiles[new_id].animation.is_full_sprite = 0x01;
    game_ctrl.projectiles[new_id].animation.sprite_id = 1;
    game_ctrl.projectiles[new_id].animation.sprite_tile_start = 13;
    game_ctrl.projectiles[new_id].animation.current_frame = 0;
    game_ctrl.projectiles[new_id].animation.total_frames = 6;
  }
}

void initialize_game() {
  fprintf(stdout, "Iniciando juego\n");
  game_ctrl.player_count = 0;
  for (uint8_t i; i<MAX_PLAYERS; i++) {
    game_ctrl.players[i].state = 0;
    game_ctrl.players[i].input_state = 0;
    game_ctrl.players[i].body.xdata = 0x00;
    game_ctrl.players[i].body.ydata = 0x00;
    game_ctrl.players[i].body.dimensions = 0x00;
    game_ctrl.players[i].lives = 0;
    game_ctrl.players[i].score = 0;
    game_ctrl.players[i].weapon_A.type_id = 0;
    game_ctrl.players[i].weapon_A.delay_counter = 0;
    game_ctrl.players[i].weapon_A.is_charging = 0;
    game_ctrl.players[i].weapon_B.type_id = 0;
    game_ctrl.players[i].weapon_B.delay_counter = 0;
    game_ctrl.players[i].weapon_B.is_charging = 0;
    game_ctrl.players[i].animation.is_full_sprite = 0x00;
    game_ctrl.players[i].animation.sprite_id = 0;
    game_ctrl.players[i].animation.sprite_tile_start = 0;
    game_ctrl.players[i].animation.current_frame = 0;
    game_ctrl.players[i].animation.total_frames = 0;
  }
  game_ctrl.enemy_count = 0;
  for (uint8_t i; i<MAX_ENEMIES; i++) {
    game_ctrl.enemies[i].state = 0;
    game_ctrl.enemies[i].ai_state = 0;
    game_ctrl.enemies[i].body.xdata = 0x00;
    game_ctrl.enemies[i].body.ydata = 0x00;
    game_ctrl.enemies[i].body.dimensions = 0x00;
    game_ctrl.enemies[i].hitpoints = 0;
    game_ctrl.enemies[i].weapon.type_id = 0;
    game_ctrl.enemies[i].weapon.delay_counter = 0;
    game_ctrl.enemies[i].weapon.is_charging = 0;
    game_ctrl.enemies[i].animation.is_full_sprite = 0x00;
    game_ctrl.enemies[i].animation.sprite_id = 0;
    game_ctrl.enemies[i].animation.sprite_tile_start = 0;
    game_ctrl.enemies[i].animation.current_frame = 0;
    game_ctrl.enemies[i].animation.total_frames = 0;
  }
  game_ctrl.projectile_count = 0;
  for (uint8_t i; i<MAX_PROJECTILES; i++) {
    game_ctrl.projectiles[i].state = 0;
    game_ctrl.projectiles[i].body.xdata = 0x00;
    game_ctrl.projectiles[i].body.ydata = 0x00;
    game_ctrl.projectiles[i].body.dimensions = 0x00;
    game_ctrl.projectiles[i].damage = 0;
    game_ctrl.projectiles[i].is_enemy = 0;
    game_ctrl.projectiles[i].animation.is_full_sprite = 0x00;
    game_ctrl.projectiles[i].animation.sprite_id = 0;
    game_ctrl.projectiles[i].animation.sprite_tile_start = 0;
    game_ctrl.projectiles[i].animation.current_frame = 0;
    game_ctrl.projectiles[i].animation.total_frames = 0;
  }
  game_ctrl.powerup_count = 0;
  for (uint8_t i; i<MAX_POWERUPS; i++) {
    game_ctrl.power_ups[i].state = 0;
    game_ctrl.power_ups[i].body.xdata = 0x00;
    game_ctrl.power_ups[i].body.ydata = 0x00;
    game_ctrl.power_ups[i].body.dimensions = 0x00;
    game_ctrl.power_ups[i].type = 0;
    game_ctrl.power_ups[i].animation.is_full_sprite = 0x00;
    game_ctrl.power_ups[i].animation.sprite_id = 0;
    game_ctrl.power_ups[i].animation.sprite_tile_start = 0;
    game_ctrl.power_ups[i].animation.current_frame = 0;
    game_ctrl.power_ups[i].animation.total_frames = 0;
  }
  game_ctrl.game_state = 0x00;
  for (uint8_t i; i<TOP_SCORES_COUNT; i++) {
    game_ctrl.top_scores[i].initials = 0x00;
    game_ctrl.top_scores[i].score = 0;
  }
  add_hud();
  add_player(23<<3,134<<3);
  add_player(134<<3,23<<3);
  add_player(23<<3,23<<3);
  add_player(134<<3,134<<3);

  uint16_t pos_x = pbody_get_x(&game_ctrl.players[0].body);
  uint16_t pos_y = pbody_get_y(&game_ctrl.players[0].body);
  fprintf(stdout, "Pos X: %u Pos Y: %u\n", pos_x>>3, pos_y>>3);
}

void default_scores() {
  game_ctrl.top_scores[0].initials = (' '<<24)|('A'<<16)|('B'<<8)|'C';
  game_ctrl.top_scores[0].score = 450000;
  game_ctrl.top_scores[1].initials = (' '<<24)|('D'<<16)|('E'<<8)|'F';;
  game_ctrl.top_scores[1].score = 350000;
  game_ctrl.top_scores[2].initials = " GHI";
  game_ctrl.top_scores[2].score = 100000;
  game_ctrl.top_scores[3].initials = " JKL";
  game_ctrl.top_scores[3].score = 50000;
  game_ctrl.top_scores[4].initials = " MNO";
  game_ctrl.top_scores[4].score = 25000;
  game_ctrl.top_scores[5].initials = " PQR";
  game_ctrl.top_scores[5].score = 10000;
  game_ctrl.top_scores[6].initials = " STU";
  game_ctrl.top_scores[6].score = 5000;
  game_ctrl.top_scores[7].initials = " VWX";
  game_ctrl.top_scores[7].score = 2500;
}

void update_animations() {
  for (uint8_t plyr_id=0; plyr_id<game_ctrl.player_count; plyr_id++) {
    animation_update(&game_ctrl.players[plyr_id].animation);
  }
  /*
  // Animating spaceships
  for (uint8_t i=0; i<2; i++) {
    fsp.oam[i]=(fsp.oam[i]&(~Mask_fsp_oam_index))|((((fsp.oam[i]&Mask_fsp_oam_index)+1)%3)+13);
  }
  */
}
