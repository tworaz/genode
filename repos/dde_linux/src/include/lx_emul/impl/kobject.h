/*
 * \brief  Implementation of linux/kobject.h
 * \author Piotr Tworek
 * \date   2019-12-07
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <base/snprintf.h>

int kobject_set_name_vargs(struct kobject *kobj, const char *fmt, va_list vargs)
{
	Genode::String_console sc((char *)kobj->name, 32);
	sc.vprintf(fmt, vargs);
	return 0;
}
