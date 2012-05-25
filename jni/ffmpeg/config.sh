#!/bin/sh
PREBUILT=/opt/android-ndk-r4/build/prebuilt/linux-x86/arm-eabi-4.2.1
./configure --target-os=linux \
--arch=armv41 \
--enable-cross-compile \
--cc=$PREBUILT/bin/arm-eabi-gcc \
--cross-prefix=$PREBUILT/bin/arm-eabi- \
--nm=$PREBUILT/bin/arm-eabi-nm \
--extra-cflags="-fPIC -DANDROID" \
--enable-static \
--disable-shared \
--disable-asm \
--disable-yasm \
--prefix=/home/ffmpeg-android-bin \
--extra-ldflags="-Wl,-T,$PREBUILT/arm-eabi/lib/ldscripts/armelf.x -Wl,-rpath-link=/opt/android-ndk-r4/build/platforms/android-4/arch-arm/usr/lib -L/opt/android-ndk-r4/build/platforms/android-4/arch-arm/usr/lib -nostdlib /opt/android-ndk-r4/build/prebuilt/linux-x86/arm-eabi-4.2.1/lib/gcc/arm-eabi/4.2.1/crtbegin.o /opt/android-ndk-r4/build/prebuilt/linux-x86/arm-eabi-4.2.1/lib/gcc/arm-eabi/4.2.1/crtend.o -lc -lm -ldl"
