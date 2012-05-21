LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_CFLAGS+= -O3 -fomit-frame-pointer -funroll-all-loops -finline-functions -ffast-math -std=c99 \
        -ffunction-sections -fdata-sections -DBUILD_STANDALONE -DCPU_ARM
LOCAL_ARM_MODE := arm
LOCAL_MODULE := libffmpegFLAC

LOCAL_SRC_FILES = \
	src/decoder.c \
	src/bitstream.c \
	src/tables.c \
	src/arm.s

include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_ARM_MODE  := arm
LOCAL_MODULE    := libffmpegFLAC-jni
LOCAL_SRC_FILES := libffmpegFLAC-jni.c
LOCAL_SHARED_LIBRARIES := libffmpegFLAC
LOCAL_CFLAGS += -O3 -std=c99 -DBUILD_STANDALONE
LOCAL_LDLIBS := -llog
include $(BUILD_SHARED_LIBRARY)