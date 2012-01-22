LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_CFLAGS+= -O3 -D_ARM_ASSEM_ -fomit-frame-pointer -funroll-all-loops -finline-functions -ffast-math
LOCAL_ARM_MODE := arm
LOCAL_MODULE := libtremolo

LOCAL_SRC_FILES = \
	src/bitwise.c \
	src/codebook.c \
	src/dsp.c \
	src/floor0.c \
	src/floor1.c \
	src/floor_lookup.c \
	src/framing.c \
	src/mapping0.c \
	src/mdct.c \
	src/misc.c \
	src/res012.c \
	src/info.c \
	src/vorbisfile.c \
	src/bitwiseARM.s \
	src/dpen.s \
	src/floor1ARM.s \
	src/mdctARM.s

include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_ARM_MODE  := arm
LOCAL_MODULE    := tremolo-jni
LOCAL_SRC_FILES := tremolo-jni.c
LOCAL_SHARED_LIBRARIES := libtremolo
LOCAL_CFLAGS += -O3 -std=c99
LOCAL_LDLIBS := -llog
include $(BUILD_SHARED_LIBRARY)
