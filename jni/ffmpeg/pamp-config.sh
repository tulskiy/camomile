#!/bin/sh

NDK_PATH=/opt/android-ndk-r4
PREBUILT=$NDK_PATH/build/prebuilt/linux-x86/arm-eabi-4.4.0


NEON_CONFIG="\
--cpu=armv7-a \
--enable-decoder=wmav1 \
--enable-decoder=wmav2 \
--enable-decoder=aac \
--enable-parser=aac \
--enable-demuxer=aac \
--extra-cflags=\"-DANDROID --sysroot=$NDK_PATH/build/platforms/android-4/arch-arm/ -march=armv7-a -mfpu=neon -mfloat-abi=softfp\" \
"

LOWEND_CONFIG="\
--cpu=armv6 \
--disable-armvfp \
--disable-rdft \
--enable-decoder=wmav1_fixedpoint \
--enable-decoder=wmav2_fixedpoint \
--extra-cflags=\"-DANDROID --sysroot=$NDK_PATH/build/platforms/android-4/arch-arm/ -mfloat-abi=soft -ffast-math -mno-thumb -mno-thumb-interwork\" "


LDFLAGS="-Wl,-T,$PREBUILT/arm-eabi/lib/ldscripts/armelf.x -Wl,-rpath-link=$NDK_PATH/build/platforms/android-8/arch-arm/usr/lib \
-L$NDK_PATH/build/platforms/android-8/arch-arm/usr/lib -nostdlib $PREBUILT/lib/gcc/arm-eabi/4.4.0/crtbegin.o \
$PREBUILT/lib/gcc/arm-eabi/4.4.0/crtend.o -lc -lm -ldl"

COMMON_CONFIG="\
./configure --target-os=linux \
--arch=arm \
--enable-cross-compile \
--cc=$PREBUILT/bin/arm-eabi-gcc \
--as=$PREBUILT/bin/arm-eabi-gcc \
--cross-prefix=$PREBUILT/bin/arm-eabi- \
--sysinclude=$NDK_PATH/build/platforms/android-8/arch-arm/usr/include \
--nm=$PREBUILT/bin/arm-eabi-nm \
--extra-ldflags=\"$LDFLAGS\" \
--enable-small \
--enable-pic \
--enable-shared \
--disable-dxva2 \
--disable-avcore \
--disable-debug \
--disable-ffplay \
--disable-ffprobe \
--disable-ffserver \
--disable-avdevice \
--disable-swscale \
--disable-avfilter \
--disable-everything \
--disable-network \
--enable-decoder=mp2 \
--enable-decoder=mp3 \
--enable-decoder=alac \
--enable-decoder=pcm_s16be \
--enable-decoder=pcm_s16le \
--enable-decoder=pcm_u16be \
--enable-decoder=pcm_u16le \
--enable-decoder=pcm_alaw \
--enable-decoder=pcm_mulaw \
--enable-decoder=pcm_s16le_planar \
--enable-decoder=adpcm_ms \
--enable-decoder=adpcm_g726 \
--enable-decoder=gsm \
--enable-decoder=gsm_ms \
--enable-decoder=tta \
--enable-decoder=wmapro \
--enable-decoder=ape \
--enable-parser=mpegaudio \
--enable-protocol=file \
--enable-protocol=pipe \
--enable-demuxer=mp3 \
--enable-demuxer=mov \
--enable-demuxer=asf \
--enable-demuxer=pcm_s16be \
--enable-demuxer=pcm_s16le \
--enable-demuxer=pcm_u16be \
--enable-demuxer=pcm_u16le \
--enable-demuxer=pcm_alaw \
--enable-demuxer=pcm_mulaw \
--enable-demuxer=tta \
--enable-demuxer=wav \
--enable-demuxer=ape \
"
eval "$COMMON_CONFIG $LOWEND_CONFIG"
if [ $? -ne 0 ]; then
	exit 1
fi
cp config.h config-lowend.h
cp config.mak config-lowend.mak

eval "$COMMON_CONFIG $NEON_CONFIG"
if [ $? -ne 0 ]; then
	exit 1
fi
cp config.h config-neon.h
cp config.mak config-neon.mak

cp config-pamp.h config.h
cp config-pamp.mak config.mak

