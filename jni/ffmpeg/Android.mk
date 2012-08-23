TOP_LOCAL_PATH := $(call my-dir)
LOCAL_PATH := $(TOP_LOCAL_PATH)
# =================================================
include $(CLEAR_VARS)
# NOTE: see av.mk for modules flags

include $(call all-makefiles-under,$(LOCAL_PATH))

include $(CLEAR_VARS)
LOCAL_PATH := $(TOP_LOCAL_PATH)
LOCAL_ARM_MODE := arm
 
LOCAL_LDLIBS := -llog -lz
LOCAL_STATIC_LIBRARIES := libavformat libavcodec libavutil libavcore

PRIVATE_WHOLE_STATIC_LIBRARIES := $(call strip-lib-prefix,$(LOCAL_STATIC_LIBRARIES))
PRIVATE_WHOLE_STATIC_LIBRARIES := $(call map,static-library-path,$(PRIVATE_WHOLE_STATIC_LIBRARIES))

LOCAL_MODULE := ffmpeg-jni
LOCAL_CFLAGS=-O3 -std=c99
LOCAL_SRC_FILES := jni/ffmpeg-aac-jni.c

include $(BUILD_SHARED_LIBRARY)