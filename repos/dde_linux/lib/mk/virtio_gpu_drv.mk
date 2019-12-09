LX_CONTRIB_DIR := $(call select_from_ports,dde_virtio_gpu)/src/drivers/framebuffer/virtio_gpu
SRC_DIR        := $(REP_DIR)/src/drivers/framebuffer/virtio_gpu

LIBS    += virtio_gpu_include

SRC_C   :=
SRC_C   += $(notdir $(wildcard $(LX_CONTRIB_DIR)/drivers/dma-buf/*.c))
SRC_C   += $(notdir $(wildcard $(LX_CONTRIB_DIR)/drivers/gpu/drm/*.c))
SRC_C   += $(notdir $(wildcard $(LX_CONTRIB_DIR)/drivers/gpu/drm/virtio/*.c))
SRC_C   += $(notdir $(wildcard $(LX_CONTRIB_DIR)/drivers/virtio/*.c))
SRC_C   += $(notdir $(wildcard $(LX_CONTRIB_DIR)/lib/*.c))
SRC_C   += $(notdir $(wildcard $(LX_CONTRIB_DIR)/lib/math/*.c))


#
# Linux sources are C89 with GNU extensions
#
CC_C_OPT += -std=gnu89

#
# Reduce build noise of compiling contrib code
#
CC_WARN = -Wno-unused-variable

vpath %.c $(LX_CONTRIB_DIR)/drivers/dma-buf
vpath %.c $(LX_CONTRIB_DIR)/drivers/virtio
vpath %.c $(LX_CONTRIB_DIR)/drivers/gpu/drm
vpath %.c $(LX_CONTRIB_DIR)/drivers/gpu/drm/virtio
vpath %.c $(LX_CONTRIB_DIR)/lib
vpath %.c $(LX_CONTRIB_DIR)/lib/math
