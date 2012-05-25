$(call clear-vars, $(FFMPEG_CONFIG_VARS))

ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
	include $(LOCAL_PATH)/../config-neon.mak
else
	include $(LOCAL_PATH)/../config-lowend.mak
endif	

OBJS :=
OBJS-yes :=
MMX-OBJS-yes :=
LOCAL_ARM_MODE := arm
SUBDIR := $(LOCAL_PATH)/

include $(LOCAL_PATH)/Makefile

OBJS-$(HAVE_MMX) += $(MMX-OBJS-yes)
OBJS += $(OBJS-yes)

FFNAME := lib$(NAME)
FFLIBS := $(foreach,NAME,$(FFLIBS),lib$(NAME))

FFCFLAGS = -DHAVE_AV_CONFIG_H -std=c99 -Wno-sign-compare -Wno-switch -Wno-pointer-sign -ffast-math

ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
	FFCFLAGS += -DPAMP_NEON -mfloat-abi=softfp -mfpu=neon -march=armv7-a
else
	FFCFLAGS += -march=armv6 
endif	

ALL_S_FILES := $(wildcard $(LOCAL_PATH)/$(TARGET_ARCH)/*.S)
ALL_S_FILES := $(addprefix $(TARGET_ARCH)/, $(notdir $(ALL_S_FILES)))

ifneq ($(ALL_S_FILES),)
ALL_S_OBJS := $(patsubst %.S,%.o,$(ALL_S_FILES))
C_OBJS := $(filter-out $(ALL_S_OBJS),$(OBJS))
S_OBJS := $(filter $(ALL_S_OBJS),$(OBJS))
else
C_OBJS := $(OBJS)
S_OBJS :=
endif

C_FILES := $(patsubst %.o,%.c,$(C_OBJS))
S_FILES := $(patsubst %.o,%.S,$(S_OBJS))

FFFILES := $(sort $(S_FILES)) $(sort $(C_FILES))
