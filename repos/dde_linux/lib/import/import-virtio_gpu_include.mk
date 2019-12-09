LX_CONTRIB_DIR := $(call select_from_ports,dde_virtio_gpu)/src/drivers/framebuffer/virtio_gpu
SRC_DIR        := $(REP_DIR)/src/drivers/framebuffer/virtio_gpu

# architecture-dependent includes
ifeq ($(filter-out $(SPECS),x86),)
  ARCH_SRC_INC_DIR += $(REP_DIR)/src/include/spec/x86
  ifeq ($(filter-out $(SPECS),32bit),)
    ARCH_SRC_INC_DIR += $(REP_DIR)/src/include/spec/x86_32
  endif # 32bit
  ifeq ($(filter-out $(SPECS),64bit),)
    ARCH_SRC_INC_DIR += $(REP_DIR)/src/include/spec/x86_64
  endif # 64bit
endif # x86

ifeq ($(filter-out $(SPECS),arm),)
  ARCH_SRC_INC_DIR += $(REP_DIR)/src/include/spec/arm
  ifeq ($(filter-out $(SPECS),arm_v6),)
    ARCH_SRC_INC_DIR += $(REP_DIR)/src/include/spec/arm_v6
  endif # arm_v6
  ifeq ($(filter-out $(SPECS),arm_v7),)
    ARCH_SRC_INC_DIR += $(REP_DIR)/src/include/spec/arm_v7
  endif # arm_v7
endif # arm

ifeq ($(filter-out $(SPECS),arm_64),)
  ARCH_SRC_INC_DIR += $(REP_DIR)/src/include/spec/arm_64
  ifeq ($(filter-out $(SPECS),arm_v8),)
    ARCH_SRC_INC_DIR += $(REP_DIR)/src/include/spec/arm_v8
  endif # arm_v8
endif # arm_64


INC_DIR += $(SRC_DIR)/include \
           $(REP_DIR)/src/include \
           $(ARCH_SRC_INC_DIR) \
           $(LX_CONTRIB_DIR)/drivers/gpu/drm \
           $(LX_CONTRIB_DIR)/drivers/gpu/include/drm \
           $(LX_CONTRIB_DIR)/drivers/gpu/include \
           $(LX_CONTRIB_DIR)/include \
           $(LX_CONTRIB_DIR)/include/uapi \
           $(LIB_CACHE_DIR)/virtio_gpu_include/include/include/include

CC_OPT += -U__linux__ -D__KERNEL__
