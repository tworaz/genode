/*
 * \brief  Emulation of Linux kernel interfaces
 * \author Piotr Tworek
 * \author Norman Feske
 * \author Stefan Kalkowski
 * \date   2019-12-19
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <util/bit_allocator.h>
#include <base/log.h>
#include <base/env.h>
#include <util/string.h>

/* local includes */
#include <component.h>

#include <lx_emul.h>
#include <lx_emul_c.h>
#include <lx_emul/extern_c_begin.h>
#include <drm/drm_modes.h>
#include <drm/drm_drv.h>
#include <drm/drm_device.h>
#include <drm/drm_file.h>
#include <drm/drm_print.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_encoder.h>
#include <drm_internal.h>
#include <linux/pagevec.h>
#include <lx_emul/extern_c_end.h>

#if 0
#define TRACE \
        do { \
		Genode::log(__func__, ": not implemented " \
		            "(called from ", __builtin_return_address(0), ")"); \
        } while (0)
#else
#define TRACE do { ; } while (0)
#endif

#define TRACE_AND_STOP \
        do { \
		Genode::log(__func__, ": not implemented " \
		            "(called from ", __builtin_return_address(0), ")"); \
                BUG(); \
        } while (0)

#include <lx_emul/impl/kernel.h>
#include <lx_emul/impl/completion.h>
#include <lx_emul/impl/slab.h>
#include <lx_emul/impl/gfp.h>
#include <lx_emul/impl/mutex.h>
#include <lx_emul/impl/sched.h>
#include <lx_emul/impl/timer.h>
#include <lx_emul/impl/work.h>
#include <lx_emul/impl/wait.h>
#include <lx_emul/impl/device.h>
#include <lx_emul/impl/platform_device.h>
#include <lx_emul/impl/kobject.h>
#include <lx_emul/impl/bitops.h>
#include <lx_emul/impl/vmalloc.h>


namespace {
	struct Mutex_guard
	{
		struct mutex &_mutex;
		Mutex_guard(struct mutex &m) : _mutex(m) { mutex_lock(&_mutex); }
		~Mutex_guard() { mutex_unlock(&_mutex); }
	};

	struct Drm_guard
	{
		drm_device * _dev;

		Drm_guard(drm_device *dev) : _dev(dev)
		{
			if (dev) {
				mutex_lock(&dev->mode_config.mutex);
				mutex_lock(&dev->mode_config.blob_lock);
				drm_modeset_lock_all(dev);
			}
		}

		~Drm_guard()
		{
			if (_dev) {
				drm_modeset_unlock_all(_dev);
				mutex_unlock(&_dev->mode_config.mutex);
				mutex_unlock(&_dev->mode_config.blob_lock);
			}
		}
	};
}


extern "C" {

/**********************
 ** Global variables **
 **********************/

struct task_struct *current = NULL;

struct ww_class reservation_ww_class;

/***************************
 ** linux/pgtable_types.h **
 ***************************/

pgprot_t pgprot_writecombine(pgprot_t prot)
{
	TRACE;
	return prot;
}

/******************
 ** linux/kref.h **
 ******************/

void kref_init(struct kref *kref) {
	kref->refcount.counter = 1; }

void kref_get(struct kref *kref)
{
	if (!kref->refcount.counter)
		Genode::error(__func__, " kref already zero");

	kref->refcount.counter++;
}

int kref_put(struct kref *kref, void (*release) (struct kref *kref))
{
	if (!kref->refcount.counter) {
		Genode::error(__func__, " kref already zero");
		return 1;
	}

	kref->refcount.counter--;
	if (kref->refcount.counter == 0) {
		release(kref);
		return 1;
	}
	return 0;
}

int kref_put_mutex(struct kref *kref, void (*release)(struct kref *kref), struct mutex *lock)
{
	if (kref_put(kref, release)) {
		mutex_lock(lock);
		return 1;
	}
	return 0;
}

unsigned int kref_read(const struct kref *kref)
{
	TRACE;
	return atomic_read(&kref->refcount);
}

int kref_get_unless_zero(struct kref *kref)
{
	if (!kref->refcount.counter)
		return 0;

	kref_get(kref);
	return 1;
}

/******************
 ** linux/slab.h **
 ******************/

void *kmalloc_array(size_t n, size_t size, gfp_t flags)
{
	if (size != 0 && n > SIZE_MAX / size) return NULL;
	return kmalloc(n * size, flags);
}

void *kmem_cache_zalloc(struct kmem_cache *cache, gfp_t flags)
{
	void * const ret = kmem_cache_alloc(cache, flags);
	if (ret)
		memset(ret, 0, cache->size());
	return ret;
}

size_t ksize(const void* objp)
{
	return _ksize((void *)objp);
}

/******************
 ** linux/file.h **
 ******************/

void fput(struct file *file)
{
	BUG_ON(!file);

	if (file->f_mapping) {
		if (file->f_mapping->my_pages) {
			free_pages((unsigned long)file->f_mapping->my_pages->addr,
                                   file->f_mapping->order);
			file->f_mapping->my_pages = nullptr;
		}
		kfree(file->f_mapping);
	}
	kfree(file);
}

/***********************
 ** linux/workqueue.h **
 ***********************/

struct workqueue_struct *system_wq = nullptr;
struct workqueue_struct *system_unbound_wq = nullptr;

struct workqueue_struct *alloc_workqueue(const char *fmt, unsigned int flags,
                                         int max_active, ...)
{
	workqueue_struct *wq = (workqueue_struct *)kzalloc(sizeof(workqueue_struct), 0);
	Lx::Work *work = Lx::Work::alloc_work_queue(&Lx::Malloc::mem(), fmt);
	wq->task       = (void *)work;

	return wq;
}

bool flush_work(struct work_struct *work)
{
	TRACE_AND_STOP;
	cancel_work_sync(work);
	return 0;
}

bool mod_delayed_work(struct workqueue_struct *wq, struct delayed_work *dwork,
                      unsigned long delay)
{
	TRACE;
	return queue_delayed_work(wq, dwork, delay);
}

void flush_workqueue(struct workqueue_struct *wq)
{
	Lx::Task *current = Lx::scheduler().current();
	if (!current) {
		Genode::error("BUG: flush_workqueue executed without task");
		Genode::sleep_forever();
	}

	Lx::Work *lx_work = (wq && wq->task) ? (Lx::Work*) wq->task
	                                     : &Lx::Work::work_queue();

	lx_work->flush(*current);
	Lx::scheduler().current()->block_and_schedule();
}

struct workqueue_struct *create_singlethread_workqueue(char const *name)
{
	workqueue_struct *wq = (workqueue_struct *)kzalloc(sizeof(workqueue_struct), 0);
	Lx::Work *work = Lx::Work::alloc_work_queue(&Lx::Malloc::mem(), name);
	wq->task       = (void *)work;
	return wq;
}

/************************
 ** asm/memory_model.h **
 ************************/

dma_addr_t page_to_pfn(struct page *page)
{
	return page->paddr / PAGE_SIZE;
}

/************************
 ** asm-generic/page.h **
 ************************/

dma_addr_t page_to_phys(struct page *page)
{
	BUG_ON(page == NULL);
	return page->paddr;
}

/*********************
 ** linux/uaccess.h **
 *********************/

size_t copy_to_user(void *dst, void const *src, size_t len)
{
	if (dst && src && len)
		memcpy(dst, src, len);
	return 0;
}

size_t copy_from_user(void *dst, void const *src, size_t len)
{
	if (dst && src && len)
		memcpy(dst, src, len);
	return 0;
}

/********************
 ** linux/string.h **
 ********************/

char *strcpy(char *to, const char *from)
{
	char *save = to;
	for (; (*to = *from); ++from, ++to);
	return(save);
}

char *strncpy(char *dst, const char* src, size_t n) {
	return Genode::strncpy(dst, src, n); }

size_t strlen(const char *s) {
	return Genode::strlen(s); }

int strcmp(const char *cs, const char *ct) {
	return Genode::strcmp(cs, ct); }

int strncmp(const char *cs, const char *ct, size_t count) {
	return Genode::strcmp(cs, ct, count); }

char *strchr(const char *p, int ch)
{
	char c;
	c = ch;
	for (;; ++p) {
		if (*p == c)
			return ((char *)p);
		if (*p == '\0')
			break;
	}

	return 0;
}

int memcmp(const void *cs, const void *ct, size_t count)
{
	/* original implementation from lib/string.c */
	const unsigned char *su1, *su2;
	int res = 0;

	for (su1 = (unsigned char*)cs, su2 = (unsigned char*)ct;
	     0 < count; ++su1, ++su2, count--)
		if ((res = *su1 - *su2) != 0)
			break;
	return res;
}

void *memchr_inv(const void *s, int cc, size_t n)
{
	if (!s)
		return NULL;

	uint8_t const c = cc;
	uint8_t const * start = (uint8_t const *)s;

	for (uint8_t const *i = start; i >= start && i < start + n; i++)
		if (*i != c)
			return (void *)i;

	return NULL;
}

void *memdup_user_nul(const void *src, size_t len)
{
	char *p = (char *)kmalloc(len + 1, GFP_KERNEL);
	if (!p)
		return ERR_PTR(-ENOMEM);

	if (Genode::strncpy(p, (const char *)src, len)) {
		kfree(p);
		return ERR_PTR(-EFAULT);
	}
	p[len] = '\0';

	return p;
}

char *kstrdup(const char *s, gfp_t gfp)
{
	if (!s)
		return NULL;

	size_t const len = strlen(s);
	char * ptr = (char *)kmalloc(len + 1, gfp);
	if (ptr)
		memcpy(ptr, s, len + 1);
	return ptr;
}

/***************(****
 ** linux/kernel.h **
 ********************/

int vsnprintf(char *str, size_t size, const char *format, va_list args)
{
	Genode::String_console sc(str, size);
	sc.vprintf(format, args);

	return sc.len();
}

char *kvasprintf(gfp_t gfp, const char *fmt, va_list ap)
{
	size_t const bad_guess = strlen(fmt) + 10;
	char * const p = (char *)kmalloc(bad_guess, gfp);
	if (!p)
		return NULL;

	vsnprintf(p, bad_guess, fmt, ap);

	return p;
}

long simple_strtol(const char *cp, char **endp, unsigned int base)
{
	unsigned long result = 0;
	size_t ret = Genode::ascii_to_unsigned(cp, result, base);
	if (endp) *endp = (char*)cp + ret;
	return result;
}

/*******************
 ** linux/sched.h **
 *******************/

char *get_task_comm(char *to, size_t len, struct task_struct *tsk)
{
	TRACE_AND_STOP;
	return nullptr;
}

static void _completion_timeout(struct timer_list *list)
{
	struct process_timer *timeout = from_timer(timeout, list, timer);
	timeout->task.unblock();
}

long __wait_completion(struct completion *work, unsigned long timeout)
{
	Lx::timer_update_jiffies();
	unsigned long j = timeout ? jiffies + timeout : 0;

	Lx::Task & cur_task = *Lx::scheduler().current();
	struct process_timer timer { cur_task };

	if (timeout) {
		timer_setup(&timer.timer, _completion_timeout, 0);
		mod_timer(&timer.timer, j);
	}

	while (!work->done) {

		if (j && j <= jiffies) {
			lx_log(1, "timeout jiffies %lu", jiffies);
			return 0;
		}

		Lx::Task *task = Lx::scheduler().current();
		work->task = (void *)task;
		task->block_and_schedule();
	}

	if (timeout)
		del_timer(&timer.timer);

	return (j  || j == jiffies) ? 1 : j - jiffies;
}

void set_current_state(int state)
{
	switch (state) {
	case TASK_INTERRUPTIBLE:
		printk("%s TASK_INTERRUPTIBLE\n", __func__);
		break;
	case TASK_RUNNING:
		printk("%s TASK_RUNNING\n", __func__);
		break;
	default:
		printk("%s unknown %d\n", __func__, state);
	}
}

void __set_current_state(int state)
{
	set_current_state(state);
}

/*************************
 ** linux/dma-mapping.h **
 *************************/

dma_addr_t dma_map_page(struct device *dev, struct page *page,
                        unsigned long offset, size_t size,
                        enum dma_data_direction direction)
{
	return page_to_phys(page) + offset;
}

void dma_unmap_page(struct device *dev, dma_addr_t dma_address, size_t size,
                    enum dma_data_direction direction)
{
	TRACE_AND_STOP;
}

int dma_mapping_error(struct device *dev, dma_addr_t dma_addr)
{
	TRACE;
	return 0;
}


/*************************
 ** linux/page-flags.h **
 *************************/

int page_count(struct page *page)
{
	if (page->flags == 0)
		return atomic_read(&(page->_count));

	while (!(page->flags & PAGE_ZONE_FIRST_PAGE))
		--page;
	return atomic_read(&(page->_count));
}

/*******************
 ** linux/ktime.h **
 *******************/

struct timespec64 ns_to_timespec64(const s64 nsec)
{
	struct timespec64 ret = { 0, 0 };

	TRACE;

	if (!nsec)
		return ret;

	s32 rest = 0;
	ret.tv_sec = div_s64_rem(nsec, NSEC_PER_SEC, &rest);
	if (rest < 0) {
		ret.tv_sec--;
		rest += NSEC_PER_SEC;
	}
	ret.tv_nsec = rest;

	return ret;
}

s64 ktime_ms_delta(const ktime_t later, const ktime_t earlier)
{
	return ktime_to_ms(ktime_sub(later, earlier));
}

/*****************************
 ** linux/atomic-fallback.h **
 *****************************/

long long atomic64_add_return(long long i, atomic64_t *p)
{
	TRACE;
	p->counter += i;
	return p->counter;
}

/****************
 ** linux/fb.h **
 ****************/

int fb_get_options(const char *name, char **option)
{
	TRACE;
	return 0;
}

/**********************
 ** asm-generic/io.h **
 **********************/

phys_addr_t virt_to_phys(void *vaddr)
{
	const unsigned long page_vaddr = PAGE_MASK & (unsigned long)vaddr;
	phys_addr_t paddr = 0;

	const struct page *p = Lx::Addr_to_page_mapping::find_page(page_vaddr);
	if (p) {
		paddr = p->paddr;
	}

	if (paddr == 0 && Lx::Malloc::mem().inside((Genode::addr_t)page_vaddr)) {
		paddr = Lx::Malloc::mem().phys_addr(vaddr);
	}

	BUG_ON(paddr == 0);

	return paddr;
}

/*****************
 ** linux/gfp.h **
 *****************/

unsigned long __get_free_pages(gfp_t gfp_mask, unsigned int order)
{
	struct page * pages = alloc_pages(gfp_mask, order);
	if (!pages)
		return 0;

	return (unsigned long)pages->addr;
}

void __free_pages(struct page *page, unsigned int order)
{
	free_pages((unsigned long)page->addr, order);
}

void *alloc_pages_exact(size_t size, gfp_t gfp_mask)
{
	return (void*)__get_free_pages(gfp_mask, get_order(size));
}

void free_pages_exact(void *virt, size_t size)
{
	TRACE_AND_STOP;
}

/*********************
 ** linux/pagevec.h **
 *********************/

void __pagevec_release(struct pagevec *pvec)
{
	pagevec_reinit(pvec);
	pvec->percpu_pvec_drained = true;
}

/**********************
 ** linux/refcount.h **
 **********************/

bool refcount_dec_and_test(atomic_t *a)
{
	if ((unsigned)a->counter == UINT_MAX)
		return false;

	if (a->counter == 0)
		pr_warn("Underflow of atomic variable ...\n");

	return atomic_dec_and_test(a);
}

/**********************
 ** linux/rcupdate.h **
 **********************/

void call_rcu(struct rcu_head *head, void (*func)(struct rcu_head *))
{
	TRACE;
	func(head);
}

/****************
 ** linux/fs.h **
 ****************/

struct inode *file_inode(struct file *f)
{
	TRACE;
	return f->f_inode;
}

/**********************
 ** linux/shmem_fs.h **
 **********************/

struct page *shmem_read_mapping_page_gfp(struct address_space *mapping,
                                         pgoff_t index, gfp_t gfp_mask)
{
	BUG_ON(index >= (1UL << mapping->order));
	return &mapping->my_pages[index];
}

struct file *shmem_file_setup(const char *name, loff_t const size,
                              unsigned long flags)
{
	struct file *file = (struct file *)kzalloc(sizeof(*file), GFP_KERNEL);
	file->f_mapping = (struct address_space *)kzalloc(sizeof(*file->f_mapping), GFP_KERNEL);

	size_t const npages = (size + PAGE_SIZE - 1) >> PAGE_SHIFT;
	size_t sz_log2 = Genode::log2(npages);
	sz_log2 += ((npages > (1UL << sz_log2)) ? 1 : 0);

	struct page *pages = alloc_pages(GFP_DMA, sz_log2);
	if (!pages)
		return 0;

	file->f_mapping->my_pages = pages;
	file->f_mapping->order = sz_log2;

	return file;
}

/************************
 ** DRM implementation **
 ************************/

static struct inode drm_fs_anon_inode = { nullptr, nullptr };
static struct drm_device *lx_drm_device = nullptr;
static struct drm_file   *lx_drm_file = nullptr;

/*
 * Defined in drm_crtc_internal.h.
 */
extern int drm_modeset_register_all(struct drm_device *dev);

int drm_dev_init(struct drm_device *dev,
                 struct drm_driver *driver,
                 struct device *parent)
{
	int ret = 0;

	kref_init(&dev->ref);
	dev->dev = parent;
	dev->driver = driver;

	/* no per-device feature limits by default */
	dev->driver_features = ~0u;

	INIT_LIST_HEAD(&dev->filelist);
	INIT_LIST_HEAD(&dev->filelist_internal);
	INIT_LIST_HEAD(&dev->clientlist);
	INIT_LIST_HEAD(&dev->vblank_event_list);

	spin_lock_init(&dev->event_lock);
	mutex_init(&dev->struct_mutex);
	mutex_init(&dev->filelist_mutex);
	mutex_init(&dev->clientlist_mutex);
	mutex_init(&dev->master_mutex);

	dev->anon_inode = &drm_fs_anon_inode;

	if (drm_core_check_feature(dev, DRIVER_GEM)) {
		ret = drm_gem_init(dev);
		if (ret) {
			DRM_ERROR("Cannot initialize graphics execution manager (GEM)\n");
			goto err_free;
		}
	}

	ret = drm_dev_set_unique(dev, dev_name(parent));
	if (ret)
		goto err_setunique;

	return 0;

err_setunique:
	if (drm_core_check_feature(dev, DRIVER_GEM))
		drm_gem_destroy(dev);
err_free:
	put_device(dev->dev);
	mutex_destroy(&dev->master_mutex);
	mutex_destroy(&dev->clientlist_mutex);
	mutex_destroy(&dev->filelist_mutex);
	mutex_destroy(&dev->struct_mutex);
	return ret;
}


struct drm_device *drm_dev_alloc(struct drm_driver *driver, struct device *parent)
{
	struct drm_device *dev;
	int ret;

	dev = (struct drm_device *)kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return (struct drm_device *)ERR_PTR(-ENOMEM);

	ret = drm_dev_init(dev, driver, parent);
	if (ret) {
		kfree(dev);
		return (struct drm_device *)ERR_PTR(ret);
	}

	return dev;
}

int drm_dev_set_unique(struct drm_device *dev, const char *name)
{
	kfree(dev->unique);
	dev->unique = kstrdup(name, GFP_KERNEL);
	return dev->unique ? 0 : -ENOMEM;
}

int drm_dev_register(struct drm_device *dev, unsigned long flags)
{
	int ret = 0;

	BUG_ON(lx_drm_device);
	lx_drm_device = dev;
	lx_drm_file = lx_drm_file_alloc(dev);

	dev->registered = true;

	if (dev->driver->load) {
		DRM_DEBUG("Call DRM driver load function\n");
		ret = dev->driver->load(dev, flags);
		if (ret)
			return ret;
	}

	if (drm_core_check_feature(dev, DRIVER_MODESET)) {
		DRM_DEBUG("Initialize modesetting\n");
		drm_modeset_register_all(dev);
	}

	DRM_INFO("Initialized %s %d.%d.%d %s\n",
		 dev->driver->name, dev->driver->major, dev->driver->minor,
		 dev->driver->patchlevel, dev->driver->date);

	return 0;
}

void drm_send_event_locked(struct drm_device *dev, struct drm_pending_event *e)
{
	if (e->completion) {
		complete_all(e->completion);
		e->completion_release(e->completion);
		e->completion = NULL;
	}

	if (e->fence) {
		TRACE_AND_STOP;
	}
}

void drm_dev_unregister(struct drm_device *dev)
{
	TRACE_AND_STOP;
}

void drm_dev_put(struct drm_device *dev)
{
	TRACE_AND_STOP;
}

void drm_put_dev(struct drm_device *dev)
{
	TRACE_AND_STOP;
}

void drm_sysfs_hotplug_event(struct drm_device *dev)
{
	if (!lx_drm_device)
		return;

	Framebuffer::Driver *driver = (Framebuffer::Driver *)
		lx_c_get_driver(lx_drm_device);

	if (driver) {
		DRM_DEBUG("generating hotplug event\n");
		driver->generate_report();
		driver->trigger_reconfiguration();
	}
}

int drm_sysfs_connector_add(struct drm_connector *connector)
{
	TRACE;
	connector->kdev = (struct device*)
		kmalloc(sizeof(struct device), GFP_KERNEL);
	DRM_DEBUG("adding \"%s\" to sysfs\n", connector->name);
	drm_sysfs_hotplug_event(connector->dev);
	return 0;
}

void drm_sysfs_connector_remove(struct drm_connector *connector)
{
	kfree(connector->kdev);
	connector->kdev = nullptr;
	DRM_DEBUG("removing \"%s\" from sysfs\n", connector->name);
	drm_sysfs_hotplug_event(connector->dev);
}

struct sg_table *drm_prime_pages_to_sg(struct page **pages, unsigned int nr_pages)
{
	struct sg_table *sg = NULL;
	int ret;

	sg = (struct sg_table *)kmalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (!sg) {
		ret = -ENOMEM;
		goto out;
	}

	ret = sg_alloc_table_from_pages(sg, pages, nr_pages, 0,
				        nr_pages << PAGE_SHIFT, GFP_KERNEL);
	if (ret)
		goto out;

	return sg;
out:
	kfree(sg);
	return (struct sg_table *)ERR_PTR(ret);
}

} /* extern "C" */

/***************************************
 ** Framebuffer driver implementation **
 ***************************************/

template <typename FUNCTOR>
static inline void lx_for_each_connector(drm_device * dev, FUNCTOR f)
{
	struct drm_connector *connector;
	list_for_each_entry(connector, &dev->mode_config.connector_list, head)
		f(connector);
}


void Framebuffer::Driver::finish_initialization()
{
	BUG_ON(!lx_drm_device);

	lx_c_set_driver(lx_drm_device, (void*)this);

	generate_report();
	_session.config_changed();

	_task.construct(_run, reinterpret_cast<void*>(this),
	                "fb_updater", Lx::Task::PRIORITY_0, Lx::scheduler());
}


struct drm_display_mode *
Framebuffer::Driver::_preferred_mode(struct drm_connector *connector)
{
	using namespace Genode;
	using Genode::size_t;

	/* try to read configuration for connector */
	try {
		Xml_node config = _session.config();
		Xml_node xn = config.sub_node();
		for (unsigned i = 0; i < config.num_sub_nodes(); xn = xn.next()) {
			if (!xn.has_type("connector"))
				continue;

			typedef String<64> Name;
			Name const con_policy = xn.attribute_value("name", Name());
			if (con_policy != connector->name)
				continue;

			bool enabled = xn.attribute_value("enabled", true);
			if (!enabled)
				return nullptr;

			unsigned long const width  = xn.attribute_value("width",  0UL);
			unsigned long const height = xn.attribute_value("height", 0UL);
			long          const hz     = xn.attribute_value("hz",     0L);

			struct drm_display_mode *mode;
			list_for_each_entry(mode, &connector->modes, head) {
			if (mode->hdisplay == (int) width &&
				mode->vdisplay == (int) height)
				if (!hz || hz == mode->vrefresh)
					return mode;
		};
		}
	} catch (...) {
		/**
		 * If no config is given, we take the most wide mode of a
		 * connector as long as it is connected at all
		 */
		if (connector->status != connector_status_connected)
			return nullptr;

		struct drm_display_mode *mode = nullptr, *tmp;
		list_for_each_entry(tmp, &connector->modes, head) {
			if (!mode || tmp->hdisplay > mode->hdisplay) mode = tmp;
		};
		return mode;
	}
	return nullptr;
}


bool Framebuffer::Driver::update_mode()
{
	using namespace Genode;

	Lx_fb old = _lx_fb;
	_lx_fb = Lx_fb();

	/* VirtIO GPU only supports 32 bit color depth */
	_lx_fb.bpp = 4;

	lx_for_each_connector(lx_drm_device, [&] (drm_connector *c) {
		drm_display_mode *mode = _preferred_mode(c);
		if (!mode) {
			_lx_fb = old;
			return;
		}
		if (mode->hdisplay > _lx_fb.width)  _lx_fb.width  = mode->hdisplay;
		if (mode->vdisplay > _lx_fb.height) _lx_fb.height = mode->vdisplay;
	});

	if (_lx_fb.width == old.width &&
            _lx_fb.height == old.height &&
	    _lx_fb.bpp == old.bpp) {
		_lx_fb = old;
		return false;
	}

	if (lx_drm_framebuffer_alloc(lx_drm_device, lx_drm_file, &_lx_fb)) {
		Genode::error("Failed to crete Lx DRM framebuffer!");
		_lx_fb = old;
		return false;
	}

	{
		Drm_guard guard(lx_drm_device);
		lx_for_each_connector(lx_drm_device, [&] (drm_connector *c) {
			lx_drm_set_mode(&_lx_fb, c, _preferred_mode(c));
		});
	}

	/* force virtual framebuffer size if requested */
	if (int w = _session.force_width_from_config())
		_lx_fb.width = min(_lx_fb.width, w);
	if (int h = _session.force_height_from_config())
		_lx_fb.height = min(_lx_fb.height, h);

	if (old.drm_fb) {
		lx_drm_framebuffer_free(&old);
	}

	return true;
}


void Framebuffer::Driver::generate_report()
{
	/* detect mode information per connector */
	{
		Mutex_guard mutex(lx_drm_device->mode_config.mutex);

		struct drm_connector *c;
		list_for_each_entry(c, &lx_drm_device->mode_config.connector_list,
		                    head)
		{
			/*
			 * All states unequal to disconnected are handled as connected,
			 * since some displays stay in unknown state if not fill_modes()
			 * is called at least one time.
			 */
			bool connected = c->status != connector_status_disconnected;
			if ((connected && list_empty(&c->modes)) ||
			    (!connected && !list_empty(&c->modes))) {
				c->funcs->fill_modes(c, 0, 0);
			}
		}
	}

	/* check for report configuration option */
	try {
		_reporter.enabled(_session.config().sub_node("report")
			.attribute_value(_reporter.name().string(), false));
	} catch (...) {
		_reporter.enabled(false);
	}
	if (!_reporter.enabled()) return;

	/* write new report */
	try {
		Genode::Reporter::Xml_generator xml(_reporter, [&] ()
		{
			Drm_guard guard(lx_drm_device);
			struct drm_connector *c;
			list_for_each_entry(c, &lx_drm_device->mode_config.connector_list, head) {
				xml.node("connector", [&] ()
				{
					bool connected = c->status == connector_status_connected;
					xml.attribute("name", c->name);
					xml.attribute("connected", connected);

					if (!connected) return;

					struct drm_display_mode *mode;
					list_for_each_entry(mode, &c->modes, head) {
						xml.node("mode", [&] ()
						{
							xml.attribute("width",  mode->hdisplay);
							xml.attribute("height", mode->vdisplay);
							xml.attribute("hz",     mode->vrefresh);
						});
					}
				});
			}
		});
	} catch (...) {
		Genode::warning("Failed to generate report");
	}
}

void Framebuffer::Driver::update_fb()
{
	struct drm_clip_rect r = { _dirty_rect.x1, _dirty_rect.y1,
				   _dirty_rect.x2, _dirty_rect.y2 };
	lx_drm_notify_framebuffer_dirty(&_lx_fb, &r);
	_fb_dirty = false;
}
