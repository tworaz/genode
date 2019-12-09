/*
 * \brief  Implementation of linux/device.h
 * \author Piotr Tworek
 * \author Stefan Kalkowski
 * \date   2019-12-07
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

namespace Lx {

	class Driver : public Genode::List<Driver>::Element
	{

	private:

		struct device_driver *_drv; /* Linux driver */

	public:

		Driver(struct device_driver *drv) : _drv(drv)
		{
			list()->insert(this);
		}

		/**
		 * List of all currently registered drivers
		 */
		static Genode::List<Driver> *list()
		{
			static Genode::List<Driver> _list;
			return &_list;
		}

		/**
		 * Match device and drivers
		 */
		bool match(struct device *dev)
		{
			/*
			 *  Don't try if buses don't match, since drivers often use 'container_of'
			 *  which might cast the device to non-matching type
			 */
			if (_drv->bus != dev->bus)
				return false;

			return _drv->bus->match ? _drv->bus->match(dev, _drv) : true;
		}

		/**
		 * Probe device with driver
		 */
		int probe(struct device *dev)
		{
			dev->driver = _drv;

			if (dev->bus->probe) return dev->bus->probe(dev);
			else if (_drv->probe)
				return _drv->probe(dev);

			return 0;
		}
	};
}


int driver_register(struct device_driver *drv)
{
	pr_debug("Driver register: %s\n", drv->name);
	if (drv) new (Lx::Malloc::mem()) Lx::Driver(drv);
	return 0;
}


void put_device(struct device *dev)
{
	TRACE_AND_STOP;
}


int device_register(struct device *dev)
{
	pr_debug("Device register: %s\n", dev_name(dev));
	return device_add(dev);
}


int device_unregister(struct device *dev)
{
	TRACE_AND_STOP;
	return -1;
}


int device_add(struct device *dev)
{
	pr_debug("Device add: %s\n", dev_name(dev));

	if (dev->driver) {
		pr_debug("No driver assigned to device!\n");
		return 0;
	}

	if (dev->init_name) {
		dev_set_name(dev, "%s", dev->init_name);
		dev->init_name = NULL;
	}

	/* foreach driver match and probe device */
	for (Lx::Driver *driver = Lx::Driver::list()->first(); driver; driver = driver->next())
		if (driver->match(dev)) {
			int ret = driver->probe(dev);

			if (!ret) return 0;
		}

	return 0;
}


int dev_set_name(struct device *dev, const char *fmt, ...)
{
	va_list args;
	int err;

	va_start(args, fmt);
	err = kobject_set_name_vargs(&dev->kobj, fmt, args);
	va_end(args);
	return err;
}
