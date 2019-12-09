/*
 * \brief  Linux platform device API
 * \author Piotr Tworek
 * \date   2019-12-07
 *
 * Based on the prototypes found in the Linux kernel's 'include/'.
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _LX_EMUL__PLATFORM_DEVICE_
#define _LX_EMUL__PLATFORM_DEVICE_

#include <lx_emul/device.h>

#define PLATFORM_NAME_SIZE 20

struct platform_device
{
	const char *name;

	struct device dev;

	const char *driver_override;

	void *lx_private_data;
};

struct platform_driver
{
	int (*probe)(struct platform_device *);
	struct device_driver driver;
};

int platform_bus_init(void);
int platform_device_register(struct platform_device *);
int platform_driver_register(struct platform_driver *);

static inline void platform_set_drvdata(struct platform_device *pdev, void *data) {
	dev_set_drvdata(&pdev->dev, data); }

#endif /* _LX_EMUL__PLATFORM_DEVICE_ */
