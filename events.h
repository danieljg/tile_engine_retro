#define OBJECTS_MAX 256
#define EVENTS_MAX 256

#define ENT_GAMECONTROL 0
#define ENT_PLAYER1 1
#define ENT_PLAYER2 2
#define ENT_ENEMY1 3
#define ENT_ENEMY2 4
#define ENT_ENEMY3 5
#define ENT_ENEMY4 6
#define ENT_ENEMY5 7

static uint8_t entities_ids[OBJECTS_MAX];

typedef struct {
  uint8_t sprite_id;
  uint8_t vel_x;
  uint8_t vel_y;
} game_object;

typedef struct {
  uint8_t object_id;
  uint16_t event_type;
} game_event;

typedef struct {
  uint8_t objects_count;
  game_object objects[OBJECTS_MAX];
  uint8_t total_events;
  game_event events[EVENTS_MAX];
} game_struct;

game_struct game;

uint8_t add_object(
    uint16_t sprite_tile_id, uint8_t sprite_pal,
    uint16_t pos_x, uint16_t pos_y
  ) {
  uint8_t new_id = game.objects_count;
  if (new_id < OBJECTS_MAX) {
    uint8_t sprite_id = add_full_sprite(sprite_tile_id, sprite_pal, pos_x, pos_y);
    game.objects[new_id].sprite_id = sprite_id;
    game.objects_count++;
    return new_id;
  }
  else return 0;
}

void game_set() {
  entities_ids[ENT_PLAYER1] = add_object(12,3,64,80);
  entities_ids[ENT_PLAYER2] = add_object(12,2,96,80);
}

void initialize_game() {
  fprintf(stdout, "Initializing Game...\n");
  game.objects_count = 1;
  game.total_events = 0;
  fprintf(stdout, "\t- Initializing Game objects...\n");
  for(uint16_t i=0; i<OBJECTS_MAX; i++) {
    game.objects[i].sprite_id = 0;
    game.objects[i].vel_x = 0;
    game.objects[i].vel_y = 0;
  }
  fprintf(stdout, "\t\t...done.\n");
  fprintf(stdout, "\t- Initializing Event Queque...\n");
  for(uint16_t i=0; i<EVENTS_MAX; i++) {
    game.events[i].object_id = 0;
    game.events[i].event_type = 0;
  }
  fprintf(stdout, "\t\t...done.\n");
  game_set();
  fprintf(stdout, "\t... Game Initialized.\n");
}


void update_entities() {
  for(uint16_t i=0; i<OBJECTS_MAX; i++) {
    move_full_sprite(
      game.objects[i].sprite_id,
      game.objects[i].vel_x, game.objects[i].vel_y
    );
  }
}
