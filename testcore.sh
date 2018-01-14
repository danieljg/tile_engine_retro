cd core
rm ./tile_engine_retro_libretro.so
make
retroarch -L ./tile_engine_retro_libretro.so
cd ..
