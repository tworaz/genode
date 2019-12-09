/*
 * \brief  Linux device driver API
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

#ifndef _LX_EMUL__DEVICE_
#define _LX_EMUL__DEVICE_

#include <lx_emul/sysfs.h>
#include <lx_emul/printf.h>
#include <lx_emul/kobject.h>

struct device;
struct device_driver;

struct kobj_uevent_env;

struct bus_type
{
	const char *name;

	const struct attribute_group **dev_groups;

	int (*match)(struct device *dev, struct device_driver *drv);
	int (*uevent)(struct device *dev, struct kobj_uevent_env *env);
	int (*probe)(struct device *dev);
	int (*remove)(struct device *dev);
	void (*shutdown)(struct device *dev);
};

struct device_type
{
	const char *name;
	const struct attribute_group **groups;
	int (*uevent)(struct device *dev, struct kobj_uevent_env *env);
	void (*release)(struct device *dev);
};

struct device {
	struct device            *parent;
	const char               *init_name;
	struct kobject            kobj;
	struct device_driver     *driver;
	void                     *drvdata;  /* not in Linux */
	const struct device_type *type;
	void                     *platform_data;
	void                     *driver_data;
	struct bus_type          *bus;

	void	(*release)(struct device *dev);
};

struct device_attribute {
	struct attribute attr;
};

struct device_driver
{
	char const      *name;
	struct bus_type *bus;

	struct module   *owner;

	int (*probe) (struct device *dev);
};

static inline int bus_register(struct bus_type *bus) { return 0; }
static inline void bus_unregister(struct bus_type *bus) { };

void put_device(struct device *dev);

int device_register(struct device *dev);
int device_unregister(struct device *dev);
int device_add(struct device *dev);
static inline void device_initialize(struct device *dev) { };

int driver_register(struct device_driver *drv);

int dev_set_name(struct device *dev, const char *name, ...);

static inline const char *dev_name(const struct device *dev)
{
	/* Use the init name until the kobject becomes available */
	if (dev->init_name)
		return dev->init_name;
	return kobject_name(&dev->kobj);
}

static inline void *dev_get_drvdata(const struct device *dev) {
	return dev->driver_data; }

static inline void dev_set_drvdata(struct device *dev, void *data) {
	dev->driver_data = data; }

#define DEVICE_ATTR_RO(_name) \
	struct device_attribute dev_attr_##_name = __ATTR_RO(_name)

#define dev_info(  dev, format, arg...) lx_printf("[%s] info: "   format , dev_name(dev), ## arg)
#define dev_warn(  dev, format, arg...) lx_printf("[%s] warn: "   format , dev_name(dev), ## arg)
#define dev_WARN(  dev, format, arg...) lx_printf("[%s] WARN: "   format , dev_name(dev), ## arg)
#define dev_err(   dev, format, arg...) lx_printf("[%s] error: "  format , dev_name(dev), ## arg)
#define dev_notice(dev, format, arg...) lx_printf("[%s] notice: " format , dev_name(dev), ## arg)
#define dev_crit(  dev, format, arg...) lx_printf("[%s] crit: "   format , dev_name(dev), ## arg)

#define dev_printk(level, dev, format, arg...) \
	lx_printf("[%s]: " format , dev_name(dev), ## arg)
#define dev_dbg(dev, format, arg...) \
	lx_printf("[%s] dbg: " format, dev_name(dev), ## arg)

#endif /* _LX_EMUL__DEVICE_ */
