TARGET   = virt_platform_drv
REQUIRES = arm_v7a virtio
INC_DIR  = $(REP_DIR)/src/drivers/platform/spec/arm_virt

include $(REP_DIR)/src/drivers/platform/spec/qemu_virt/target.inc
