TARGET   = virtio_mmio
REQUIRES = arm_v8a virtio
LIBS     = base fdt

include $(REP_DIR)/src/drivers/virtio_mmio/target.inc
