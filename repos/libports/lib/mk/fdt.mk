FDT_DIR = $(call select_from_ports,fdt)/src/lib/fdt/libfdt
LIBS       = base
INC_DIR   += $(FDT_DIR)
SRC_C      = $(notdir $(wildcard $(FDT_DIR)/*.c))
SRC_CC    += genode.cc

INC_DIR   += $(REP_DIR)/include/fdt
INC_DIR   += $(REP_DIR)/src/lib/fdt/include

vpath %.c       $(FDT_DIR)
vpath genode.cc $(REP_DIR)/src/lib/fdt

SHARED_LIB = yes

LD_OPT += --version-script=$(FDT_DIR)/version.lds --no-undefined
