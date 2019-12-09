REQUIRES = virtio

TARGET   = virtio_gpu
LIBS     = base virtio_gpu_drv virtio_gpu_include lx_kit_setjmp
SRC_CC   = main.cc \
           lx_emul.cc \
           dummies.cc
SRC_C    = lx_emul_c.c

# lx_kit
SRC_CC += env.cc \
          timer.cc \
          work.cc \
          malloc.cc \
          printf.cc \
          scheduler.cc \
          virtio.cc

INC_DIR += $(REP_DIR)/src/include

vpath %.c  $(PRG_DIR)
vpath %.cc $(PRG_DIR)
vpath %.cc $(REP_DIR)/src/lx_kit

CC_CXX_WARN_STRICT =
CC_OPT += -Wno-narrowing
