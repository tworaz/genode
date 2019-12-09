/*
 * \brief  Implementation of linux/gfp.h
 * \author Norman Feske
 * \date   2015-09-09
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Linux kit includes */
#include <lx_kit/addr_to_page_mapping.h>
#include <lx_kit/backend_alloc.h>
#include <lx_kit/env.h>
#include <lx_kit/internal/list.h>
#include <lx_kit/malloc.h>

namespace Lx { class Malloc_dummy_page_mapping; }


class Lx::Malloc_dummy_page_mapping : public Lx_kit::List<Malloc_dummy_page_mapping>::Element
{
	private:

		struct page _page;

		static Genode::List<Malloc_dummy_page_mapping> *_list()
		{
			static Genode::List<Malloc_dummy_page_mapping> _l;
			return &_l;
		}

		Malloc_dummy_page_mapping(unsigned long addr)
		{
			atomic_inc(&_page._count);
			_page.addr = (void *)addr;
			_page.paddr = Lx::Malloc::mem().phys_addr((void *)addr);
			_page.flags = FAKE_MALLOC_PAGE;
			_list()->insert(this);
		}

	public:

		static struct page *get(unsigned long addr)
		{
			for (auto *m = _list()->first(); m; m = m->next())
				if ((unsigned long)m->_page.addr == addr)
					return &m->_page;
			auto *entry = new (Lx::Malloc::mem()) Malloc_dummy_page_mapping(addr);
			return &(entry->_page);
		}

		static void put(struct page *page)
		{
			for (auto *m = _list()->first(); m; m = m->next()) {
				if (m->_page.addr == page->addr) {
					_list()->remove(m);
					return;
				}
			}
			BUG();
		}

};


struct page *virt_to_page(const void *vaddr)
{
	const unsigned long page_vaddr = PAGE_MASK & (unsigned long)vaddr;
	auto *page = Lx::Addr_to_page_mapping::find_page(page_vaddr);

	if (!page && Lx::Malloc::mem().inside((Genode::addr_t)vaddr)) {
		page = Lx::Malloc_dummy_page_mapping::get(page_vaddr);
	}

	BUG_ON(page == NULL);
	return page;
}


struct page *alloc_pages(gfp_t const gfp_mask, unsigned int order)
{
	using Genode::Cache_attribute;

	const size_t size = PAGE_SIZE << order;
	const size_t page_count = 1 << order;

	struct page *pages = (struct page *)kzalloc(sizeof(struct page) * page_count, 0);

	gfp_t const dma_mask = (GFP_DMA | GFP_LX_DMA | GFP_DMA32);
	Cache_attribute const cached = (gfp_mask & dma_mask) ? Genode::UNCACHED
	                                                     : Genode::CACHED;
	Genode::Ram_dataspace_capability ds_cap = Lx::backend_alloc(size, cached);
	atomic_inc(&pages[0]._count);
	pages[0].addr = Lx_kit::env().rm().attach(ds_cap);
	pages[0].paddr = Genode::Dataspace_client(ds_cap).phys_addr();
	pages[0].flags = order > 0 ? PAGE_ZONE_FIRST_PAGE : 0;

	if (!pages[0].addr) {
		Genode::error("alloc_pages: ", size, " failed");
		kfree(pages);
		return 0;
	}

	Lx::Addr_to_page_mapping::insert(&pages[0], ds_cap);

	for (unsigned i = 1; i < page_count; ++i) {
		pages[i].addr = (void*)((Genode::addr_t)pages[0].addr + i * PAGE_SIZE);
		pages[i].paddr = pages[0].paddr + i * PAGE_SIZE;
		pages[i].flags = PAGE_ZONE_PAGE;
	}

	return pages;
}


void free_pages(unsigned long addr, unsigned int order)
{
	struct page *pages = Lx::Addr_to_page_mapping::find_page(addr);
	if (!pages) return;

	BUG_ON(pages->flags & PAGE_ZONE_PAGE);
	BUG_ON(pages->flags & PAGE_ZONE_FIRST_PAGE && order == 0);
	BUG_ON((pages->flags & FAKE_MALLOC_PAGE) && order != 0);

	if (pages->flags & FAKE_MALLOC_PAGE) {
		Lx::Malloc_dummy_page_mapping::put(pages);
		return;
	}

	Genode::Ram_dataspace_capability cap =
		Lx::Addr_to_page_mapping::remove(pages);
	if (cap.valid())
		Lx::backend_free(cap);
	kfree(pages);
}


void get_page(struct page *page)
{
	atomic_inc(&page->_count);
}


void put_page(struct page *page)
{
	TRACE;
}
