/*
 * \brief  Linux sysfs API
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

#ifndef _LX_EMUL__SYSFS_
#define _LX_EMUL__SYSFS_

struct kobject;

struct attribute {
	const char *name;
	umode_t     mode;
};

struct attribute_group {
	struct attribute **attrs;
};

struct sysfs_ops {
	ssize_t (*show)(struct kobject *, struct attribute *, char *);
	ssize_t (*store)(struct kobject *, struct attribute *, const char *, size_t);
};

#define __ATTR_RO(_name) {                                     \
       .attr   = { .name = __stringify(_name), .mode = 0444 }, \
}

#define __ATTRIBUTE_GROUPS(_name)                              \
static const struct attribute_group *_name##_groups[] = {      \
       &_name##_group,                                         \
       NULL,                                                   \
}

#define ATTRIBUTE_GROUPS(_name)                                \
static const struct attribute_group _name##_group = {          \
       .attrs = _name##_attrs,                                 \
};                                                             \
__ATTRIBUTE_GROUPS(_name)

#endif /* _LX_EMUL__SYSFS_ */
