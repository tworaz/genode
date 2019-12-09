/*
 * \brief  Implementation of linux/vmalloc.h
 * \author Piotr Tworek
 * \date   2020-01-01
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <base/env.h>
#include <pd_session/connection.h>
#include <region_map/client.h>
#include <rm_session/connection.h>
#include <util/retry.h>

#include <lx_kit/env.h>
#include <lx_kit/malloc.h>
#include <lx_kit/internal/list.h>

namespace Lx {
	class Vmap_region;
	class Addr_to_vmap_region_mapping;
}

namespace {
static Genode::Constructible<Genode::Rm_connection> _global_rm;
}


class Lx::Vmap_region
{
	private:

		Genode::Env               &_env;
		Genode::Region_map_client  _region_map;

	public:

		Vmap_region(Genode::Env &env, Genode::Rm_connection& rm, size_t size)
		: _env(env), _region_map(rm.create(size))
		{ }

		Genode::addr_t add_ds(Genode::Ram_dataspace_capability &ds) {
			return _region_map.attach(ds); }

		Genode::addr_t attach() { return _env.rm().attach(_region_map.dataspace()); }
};


class Lx::Addr_to_vmap_region_mapping : public Lx_kit::List<Addr_to_vmap_region_mapping>::Element
{
	private:

		Genode::addr_t   _vaddr { 0 };
		Lx::Vmap_region *_region { nullptr };

		static Genode::List<Addr_to_vmap_region_mapping> *_list()
		{
			static Genode::List<Addr_to_vmap_region_mapping> _l;
			return &_l;
		}

	public:

		static void insert(Genode::addr_t vaddr, Lx::Vmap_region *region)
		{
			auto *m = new (Lx::Malloc::mem()) Addr_to_vmap_region_mapping;

			m->_vaddr = vaddr;
			m->_region = region;

			_list()->insert(m);
		}

		static Lx::Vmap_region *remove(Genode::addr_t vaddr)
		{
			Addr_to_vmap_region_mapping *mp = nullptr;
			for (auto *m = _list()->first(); m; m = m->next())
				if (m->_vaddr == vaddr)
					mp = m;

			Lx::Vmap_region *region = nullptr;
			if (mp) {
				region = mp->_region;
				_list()->remove(mp);
				Lx::Malloc::mem().free(mp);
			}

			return region;
		}
};


void *vmap(struct page **pages, unsigned int count, unsigned long flags, pgprot_t prot)
{
	if (!count || !pages || !pages[0])
		return 0;

	if (count == 1)
		return pages[0]->addr;

	if (!_global_rm.constructed()) {
		_global_rm.construct(Lx_kit::env().env());
	}

	size_t order = Genode::log2(count);
	if (count > (1U << order))
		order++;

	const size_t region_size = PAGE_SIZE << order;

	Lx::Vmap_region *region = nullptr;
	Genode::retry<Genode::Out_of_ram>(
		[&] {
			region = new (Lx::Malloc::mem()) Lx::Vmap_region(
				Lx_kit::env().env(), *_global_rm, region_size);
		},
		[&] () { _global_rm->upgrade_ram(16384); },
		10
	);

	try {

		for (unsigned i = 0; i < count; i++) {
			if (pages[i]->flags & PAGE_ZONE_PAGE)
				continue;

			auto cap = Lx::Addr_to_page_mapping::find_cap_by_page(pages[i]);
			if (!cap.valid()) {
				Genode::error("Failed to find capability for page ", pages[i]);
				return nullptr;
			}

			region->add_ds(cap);
		}

		Genode::addr_t local_region_addr = region->attach();
		Lx::Addr_to_vmap_region_mapping::insert(local_region_addr, region);

		return (void *)local_region_addr;
	} catch (...) {
		Genode::error("Vmap failed!");
		Lx::Malloc::mem().free(region);
	}

	return nullptr;
}


void vunmap(const void *vaddr)
{
	auto *region = Lx::Addr_to_vmap_region_mapping::remove((Genode::addr_t)vaddr);
	BUG_ON(!region);
	Lx::Malloc::mem().free(region);
}
