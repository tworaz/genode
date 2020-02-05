LIB_MK := lib/mk/virtio_gpu_drv.mk \
          lib/mk/virtio_gpu_include.mk \
          $(foreach SPEC,arm arm_64 x86_32 x86_64,lib/mk/spec/$(SPEC)/lx_kit_setjmp.mk)

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/dde_virtio_gpu)

MIRROR_FROM_REP_DIR := $(LIB_MK) \
                       lib/import/import-virtio_gpu_include.mk \
                       src/include src/lx_kit \
                       $(shell cd $(REP_DIR); find src/drivers/framebuffer/virtio_gpu -type f)

MIRROR_FROM_PORT_DIR := $(shell cd $(PORT_DIR); find src/drivers/framebuffer/virtio_gpu -type f)
MIRROR_FROM_PORT_DIR := $(filter-out $(MIRROR_FROM_REP_DIR),$(MIRROR_FROM_PORT_DIR))

content: $(MIRROR_FROM_REP_DIR) $(MIRROR_FROM_PORT_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

$(MIRROR_FROM_PORT_DIR):
	mkdir -p $(dir $@)
	cp $(PORT_DIR)/$@ $@

content: LICENSE
LICENSE:
	( echo "GNU General Public License version 2, see:"; \
	  echo "https://www.kernel.org/pub/linux/kernel/COPYING" ) > $@
