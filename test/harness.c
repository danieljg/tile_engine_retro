//headless libretro harness: init + N frames with A+RIGHT held
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "libretro.h"

//frame CRCs at fixed points make renderer changes verifiable: run before
//and after and compare (frames past the boot fade so output is steady)
static uint32_t frame_no = 0;
static void vid(const void *data, unsigned w, unsigned h, size_t pitch){
  (void)w;
  frame_no++;
  if (data && (frame_no==100 || frame_no==500 || frame_no==1000 || frame_no==1500)) {
    const uint8_t* p = data;
    uint32_t crc = 2166136261u;
    for (size_t i=0; i<(size_t)h*pitch; i++) { crc ^= p[i]; crc *= 16777619u; }
    printf("frame %u crc %08x\n", (unsigned)frame_no, crc);
  }
}
static void aud(int16_t l, int16_t r){ (void)l;(void)r; }
static size_t audb(const int16_t *d, size_t f){ (void)d; return f; }
static void poll(void){}
static int16_t inp(unsigned port, unsigned dev, unsigned idx, unsigned id){
  (void)port;(void)dev;(void)idx;
  return (id==RETRO_DEVICE_ID_JOYPAD_A) || (id==RETRO_DEVICE_ID_JOYPAD_RIGHT);
}
static bool env(unsigned cmd, void *data){
  (void)data;
  return cmd == RETRO_ENVIRONMENT_SET_PIXEL_FORMAT;
}

#include <stdlib.h>
#include <string.h>

int main(void){
  retro_set_environment(env);
  retro_set_video_refresh(vid);
  retro_set_audio_sample(aud);
  retro_set_audio_sample_batch(audb);
  retro_set_input_poll(poll);
  retro_set_input_state(inp);
  retro_init();
  retro_load_game(NULL);
  for (int i=0;i<1800;i++) retro_run(); //30 seconds of gameplay
  printf("HARNESS OK\n");

  //save state round trip: snapshot, play on, restore, replay, compare
  size_t sz = retro_serialize_size();
  printf("savestate size: %zu bytes\n", sz);
  uint8_t *snap = malloc(sz), *d1 = malloc(sz), *d2 = malloc(sz);
  if (!retro_serialize(snap, sz)) { printf("SERIALIZE FAILED\n"); return 1; }
  for (int i=0;i<300;i++) retro_run();
  retro_serialize(d1, sz);
  if (!retro_unserialize(snap, sz)) { printf("RESTORE FAILED\n"); return 1; }
  for (int i=0;i<300;i++) retro_run();
  retro_serialize(d2, sz);
  if (memcmp(d1, d2, sz) != 0) { printf("ROUNDTRIP MISMATCH\n"); return 1; }
  printf("ROUNDTRIP OK\n");
  retro_deinit();
  return 0;
}
