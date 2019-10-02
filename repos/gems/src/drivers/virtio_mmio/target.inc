SRC_CC   = main.cc bus.cc
INC_DIR += $(REP_DIR)/src/drivers/virtio_mmio

vpath main.cc $(REP_DIR)/src/drivers/virtio_mmio
vpath bus.cc  $(PRG_DIR)
