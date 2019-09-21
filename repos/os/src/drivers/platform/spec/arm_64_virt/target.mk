TARGET   = virt_platform_drv
REQUIRES = arm_v8a virtio
INC_DIR  = $(REP_DIR)/src/drivers/platform/spec/arm_64_virt

include $(REP_DIR)/src/drivers/platform/spec/qemu_virt/target.inc
