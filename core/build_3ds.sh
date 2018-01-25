$DEVKITARM/bin/arm-none-eabi-gcc -o libretro-core-3ds.o -c libretro-core.c -DARM11 -D_3DS -march=armv6k -mtune=mpcore -mfloat-abi=hard -mword-relocations -fomit-frame-pointer -ffast-math -O2 

$DEVKITARM/bin/arm-none-eabi-ar rcs libretro-core-3ds.a libretro-core-3ds.o