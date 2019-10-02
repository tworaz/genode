/*
 * \brief  VirtIO MMIO device mmio
 * \author Piotr Tworek
 * \date   2019-09-27
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#pragma once

#include <os/attached_mmio.h>

namespace Virtio_mmio
{
	using namespace Genode;
	struct Device_mmio;
};


struct Virtio_mmio::Device_mmio : public Attached_mmio
{
	struct Probe_failed : Exception { };

	enum { VIRTIO_MMIO_MAGIC = 0x74726976 };

	/**
	 * Some of the registers are actually 8 bits wide , but according to section 4.2.2.2 of
	 * VIRTIO 1.0 spec "The driver MUST use only 32 bit wide and aligned reads and writes".
	 */
	struct Magic             : Register<0x000, 32> { };
	struct Version           : Register<0x004, 32> { };
	struct DeviceID          : Register<0x008, 32> { };
	struct VendorID          : Register<0x00C, 32> { };
	struct DeviceFeatures    : Register<0x010, 32> { };
	struct DeviceFeaturesSel : Register<0x014, 32> { };
	struct DriverFeatures    : Register<0x020, 32> { };
	struct DriverFeaturesSel : Register<0x024, 32> { };
	struct QueueSel          : Register<0x030, 32> { };
	struct QueueNumMax       : Register<0x034, 32> { };
	struct QueueNum          : Register<0x038, 32> { };
	struct QueueReady        : Register<0x044, 32> { };
	struct QueueNotify       : Register<0x050, 32> { };
	struct InterruptStatus   : Register<0x060, 32> { };
	struct InterruptAck      : Register<0x064, 32> { };
	struct Status            : Register<0x070, 32> { };
	struct QueueDescLow      : Register<0x080, 32> { };
	struct QueueDescHigh     : Register<0x084, 32> { };
	struct QueueAvailLow     : Register<0x090, 32> { };
	struct QueueAvailHigh    : Register<0x094, 32> { };
	struct QueueUsedLow      : Register<0x0A0, 32> { };
	struct QueueUsedHigh     : Register<0x0A4, 32> { };
	struct ConfigGeneration  : Register<0x0FC, 32> { };

	/**
	 * Different views on device configuration space. According to the VIRTIO 1.0 spec
	 * 64 bit wide registers are supposed to be read as two 32 bit values.
	 */
	struct Config_8          : Register_array<0x100,  8, 256, 8>  { };
	struct Config_16         : Register_array<0x100, 16, 128, 16> { };
	struct Config_32         : Register_array<0x100, 32,  64, 32> { };

	Device_mmio(Env &env, addr_t base, size_t size)
	: Attached_mmio(env, base, size, false)
	{
		if (read<Magic>() != VIRTIO_MMIO_MAGIC)
			throw Probe_failed();
	}
};
