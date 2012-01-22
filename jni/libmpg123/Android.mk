LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE    := libmpg123
LOCAL_ARM_MODE  := arm
LOCAL_CFLAGS    += -O4 -Wall -DHAVE_CONFIG_H  \
	-fomit-frame-pointer -funroll-all-loops -finline-functions -ffast-math
	
LOCAL_SRC_FILES += \
	src/compat.c \
	src/parse.c \
	src/frame.c \
	src/format.c \
	src/dct64.c \
	src/equalizer.c \
	src/id3.c \
	src/icy.c \
	src/icy2utf8.c \
	src/optimize.c \
	src/readers.c \
	src/tabinit.c \
	src/libmpg123.c \
	src/index.c \
	src/layer1.c \
	src/layer2.c \
	src/layer3.c \
	src/dither.c \
	src/feature.c \
	src/synth.c \
	src/ntom.c \
	src/synth_8bit.c \
	src/stringbuf.c
	
ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
	LOCAL_CFLAGS += -mfloat-abi=softfp -mfpu=neon -DOPT_NEON -DREAL_IS_FLOAT
	LOCAL_SRC_FILES += 	src/synth_neon.S \
						src/synth_stereo_neon.S \
						src/dct64_neon.S
else
	LOCAL_CFLAGS += -DOPT_ARM -DREAL_IS_FIXED
	LOCAL_SRC_FILES += src/synth_arm.S
endif 

include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_ARM_MODE  := arm
LOCAL_MODULE    := mpg123-jni
LOCAL_SRC_FILES := mpg123-jni.c
LOCAL_SHARED_LIBRARIES := libmpg123
LOCAL_CFLAGS += -O4 -std=c99
LOCAL_LDLIBS := -llog
include $(BUILD_SHARED_LIBRARY) 