/*
 * \brief  Implementation of linux/platform_device.h
 * \author Piotr Tworek
 * \date   2019-12-07
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#define to_platform_driver(drv) (container_of((drv), struct platform_driver, \
                                 driver))
#define to_platform_device(x) container_of((x), struct platform_device, dev)

static int platform_match(struct device *dev, struct device_driver *drv)
{
	struct platform_device *pdev = to_platform_device(dev);

	pr_debug("Platform match %s using %s\n", pdev->name, drv->name);

	/* When driver_override is set, only bind to the matching driver */
	if (pdev->driver_override)
		return !strcmp(pdev->driver_override, drv->name);

	return (strcmp(pdev->name, drv->name) == 0);
}

static int platform_drv_probe(struct device *_dev)
{
	pr_debug("Platform bus probe: %s\n", dev_name(_dev));
	struct platform_driver *drv = to_platform_driver(_dev->driver);
	struct platform_device *dev = to_platform_device(_dev);
	return drv->probe(dev);
}

struct device platform_bus = {
	.init_name = "platform",
};

struct bus_type platform_bus_type = {
	.name = "platform",
	.match = platform_match,
	.probe = platform_drv_probe,
};

int platform_bus_init(void)
{
	pr_debug("Platform bus init\n");

	int error = device_register(&platform_bus);
	if (error) {
		put_device(&platform_bus);
		return error;
	}

	error = bus_register(&platform_bus_type);
	if (error) {
		device_unregister(&platform_bus);
	}

	return error;
}

int platform_device_register(struct platform_device *pdev)
{
	dev_set_name(&pdev->dev, pdev->name);

	if (!pdev->dev.parent)
		pdev->dev.parent = &platform_bus;

	pdev->dev.bus = &platform_bus_type;

	pr_debug("Platform device register: %s\n", pdev->name);

	return device_add(&pdev->dev);
}

int platform_driver_register(struct platform_driver *drv)
{
	drv->driver.bus = &platform_bus_type;
	if (drv->probe)
		drv->driver.probe = platform_drv_probe;

	pr_debug("Platform driver register: %s\n", drv->driver.name);

	return driver_register(&drv->driver);
}
