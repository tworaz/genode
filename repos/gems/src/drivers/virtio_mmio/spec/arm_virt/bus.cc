/*
 * \brief  VirtIO MMIO bus
 * \author Piotr Tworek
 * \date   2019-09-27
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <bus.h>
#include <device_mmio.h>

#include <base/attached_rom_dataspace.h>
#include <base/env.h>
#include <base/log.h>
#include <virtio_session/virtio_session.h>

extern "C" {
#include <libfdt.h>
}


using namespace Genode;
using namespace Virtio;


Genode::size_t Virtio_mmio::Bus::scan(Env         &env,
                                      Allocator   &md_alloc,
                                      Device_list &out_list)
{
	Attached_rom_dataspace rom { env, "platform.dtb" };
	size_t device_count = 0;
	const int ROOT_NODE = 0;

	auto *dtb = rom.local_addr<const void>();
	if (fdt_check_header(dtb) != 0) {
		error("Device tree header invalid!");
		return 0;
	}

	const size_t address_cells = fdt_address_cells(dtb, ROOT_NODE);
	const size_t size_cells = fdt_size_cells(dtb, ROOT_NODE);

	if (address_cells <= 0 || size_cells <= 0) {
		error("Filed to read #address-cells / #size-cells properties!");
		return 0;
	}

	/**
	 * For unknown reasons qemu always uses 2 cells to store address
	 * related values, even on 32bit architectures.
	 */
	if (address_cells != 2 && size_cells != 2) {
		error("Invalid #address-cells / #size-cells values!");
		return 0;
	}

	int current_node = -1;
	int len = -1;
	fdt_for_each_subnode(current_node, dtb, ROOT_NODE) {
		const char *name = fdt_get_name(dtb, current_node, &len);
		if (!name) {
			warning("fdt_get_name() failed: ", fdt_strerror(len));
			continue;
		}

		if (strcmp(name, "virtio_mmio", 11) != 0)
			continue;

		const fdt32_t* irq_data = static_cast<const fdt32_t *>(
			fdt_getprop(dtb, current_node, "interrupts", &len));
		if (!irq_data) {
			warning("fdt_getprop() failed: ", fdt_strerror(len));
			continue;
		}

		if (len != 12) {
			warning("Unhandled virtio_mmio IRQ cell size!");
			continue;
		}

		// 0 - IRQ type SPI/PPI
		// 1 - IRQ number
		// 2 - IRQ flags
		enum { IRQ_TYPE_SPI = 0, IRQ_TYPE_PPI = 1 };
		enum { EDGE_LO_HI = 1, EDGE_HI_LO = 2, LEVEL_HI = 4, LEVEL_LO = 8 };

		const uint32_t irq_type = fdt32_to_cpu(irq_data[0]);
		const uint32_t irq_base = (irq_type == IRQ_TYPE_SPI) ? 32 : 16;
		const uint32_t irq_num = irq_base + fdt32_to_cpu(irq_data[1]);
		const uint32_t irq_flags = fdt32_to_cpu(irq_data[2]);

		typedef Irq_session::Trigger Trigger;
		typedef Irq_session::Polarity Polarity;

		Trigger irq_trigger = Trigger::TRIGGER_UNCHANGED;
		Polarity irq_polarity = Polarity::POLARITY_UNCHANGED;

		switch (irq_flags) {
		case EDGE_LO_HI:
			irq_trigger = Trigger::TRIGGER_EDGE;
			irq_polarity = Polarity::POLARITY_HIGH;
			break;
		case EDGE_HI_LO:
			irq_trigger = Trigger::TRIGGER_EDGE;
			irq_polarity = Polarity::POLARITY_LOW;
			break;
		case LEVEL_HI:
			irq_trigger = Trigger::TRIGGER_LEVEL;
			irq_polarity = Polarity::POLARITY_HIGH;
			break;
		case LEVEL_LO:
			irq_trigger = Trigger::TRIGGER_LEVEL;
			irq_polarity = Polarity::POLARITY_LOW;
			break;
		default:
			warning("Unhandled FDT IRQ flags!");
			continue;
		}

		const fdt32_t* reg_data = static_cast<const fdt32_t *>(
			fdt_getprop(dtb, current_node, "reg", &len));
		if (!reg_data) {
			warning("fdt_get_property() failed: ", fdt_strerror(len));
			continue;
		}

		if ((len / sizeof(fdt32_t)) != (address_cells + size_cells)) {
			warning("Invalid \"reg\" property size!");
			continue;
		}

		addr_t base = fdt32_to_cpu(reg_data[1]);
		size_t size = fdt32_to_cpu(reg_data[3]);
		Device_type type = Device_type::INVALID;

		try {
			Device_mmio dev(env, base, size);
			type = static_cast<Device_type>(dev.read<Device_mmio::DeviceID>());
		} catch (...) {
			warning("Failed to probe VirtIO device @ ", Hex(base));
			continue;
		}

		if (type != Device_type::INVALID) {
			log("Found VirtIO ", type, " device, address: ",
			    Hex(base), ", interrupt: ", irq_num);
			auto *elm = new (md_alloc) Device_description(
				base, size, irq_num, type, irq_trigger, irq_polarity);
			out_list.insert(elm);
			device_count++;
		}
	}

	return device_count;
}
