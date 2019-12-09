/*
 * \brief  Linux kernel API
 * \author Piotr Tworek
 * \date   2020-01-01
 *
 * Based on the prototypes found in the Linux kernel's 'include/'.
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

void *vmap(struct page **, unsigned int, unsigned long, pgprot_t);
void vunmap(const void *);
