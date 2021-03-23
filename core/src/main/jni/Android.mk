LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	utils/elf_util.cpp \
	hook_me.cpp \

DUMP_ASM = false
ifeq ($(DUMP_ASM),true)
$(warning "build arch/$(TARGET_ARCH)/jump_trampoline.s")
LOCAL_CFLAGS += -DDUMP_ASM_CODE
LOCAL_SRC_FILES += arch/$(TARGET_ARCH)/jump_trampoline.s
endif

LOCAL_LDLIBS:= -llog

LOCAL_MODULE:= HookME

include $(BUILD_SHARED_LIBRARY)
