LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_ARM_MODE := arm
LOCAL_MODULE := libwavpack
LOCAL_CFLAGS += -O3 -DNO_PACK -DVER4_ONLY

LOCAL_SRC_FILES = \
    src/bits.c \
    src/float.c \
    src/metadata.c \
    src/tags.c \
    src/unpack.c \
    src/words.c \
    src/wputils.c

include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_ARM_MODE  := arm
LOCAL_MODULE    := wavpack-jni
LOCAL_SRC_FILES := wavpack-jni.c
LOCAL_STATIC_LIBRARIES := libwavpack
LOCAL_CFLAGS += -O3 -std=c99
LOCAL_LDLIBS := -llog
include $(BUILD_SHARED_LIBRARY)

