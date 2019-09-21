/*
 * \brief  Flash address and size values for Qemu Arm Virt DTB ROM.
 * \author Piotr Tworek
 * \date   2019-09-18
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#pragma once

namespace Qemu_virt {
        enum {
                DTB_FLASH_BASE = 0x00000000,
                DTB_FLASH_SIZE = 0x04000000,
        };
};

