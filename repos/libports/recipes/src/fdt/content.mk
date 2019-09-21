content: src/lib/fdt lib/mk/fdt.mk LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/fdt)

src/lib/fdt:
	mkdir -p $@
	cp -r $(PORT_DIR)/src/lib/fdt/libfdt $@
	cp $(REP_DIR)/src/lib/fdt/*.cc $@
	echo "LIBS = fdt" > $@/target.mk

lib/mk/fdt.mk:
	$(mirror_from_rep_dir)

LICENSE:
	cp $(PORT_DIR)/src/lib/fdt/BSD-2-Clause $@
