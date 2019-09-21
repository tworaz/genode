content: include lib/symbols/fdt LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/fdt)

include:
	mkdir $@
	cp -r $(PORT_DIR)/include/fdt/* $@/
	cp -r $(REP_DIR)/src/lib/fdt/include/* $@/
	cp -r $(REP_DIR)/include/fdt/* $@/

lib/symbols/fdt:
	$(mirror_from_rep_dir)

LICENSE:
	cp $(PORT_DIR)/src/lib/fdt/BSD-2-Clause $@
