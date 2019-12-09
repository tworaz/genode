/*
 * \brief  Emulation of the Linux kernel API used by DRM
 * \author Piotr Tworek
 * \author Norman Feske
 * \date   2019-12-07
 *
 * The content of this file, in particular data structures, is partially
 * derived from Linux-internal headers.
 */

#ifndef _LX_EMUL_H_
#define _LX_EMUL_H_

#include <stdarg.h>
#include <base/fixed_stdint.h>

#include <lx_emul/extern_c_begin.h>

/*****************
 ** asm/param.h **
 *****************/

enum { HZ = 100UL };

#define DEBUG_LINUX_PRINTK 0

#include <lx_emul/compiler.h>
#include <lx_emul/printf.h>
#include <lx_emul/bug.h>
#include <lx_emul/barrier.h>
#include <lx_emul/types.h>
#include <lx_emul/atomic.h>
#include <lx_emul/errno.h>
#include <lx_emul/string.h>
#include <lx_emul/pm.h>
#include <lx_emul/spinlock.h>
#include <lx_emul/mutex.h>
#include <lx_emul/ww_mutex.h>
#include <lx_emul/kobject.h>
#include <lx_emul/bitops.h>
#include <lx_emul/kernel.h>
#include <lx_emul/irq.h>
#include <lx_emul/completion.h>
#include <lx_emul/module.h>
#include <lx_emul/gfp.h>
#include <lx_emul/jiffies.h>

#include <asm-generic/div64.h>
#include <linux/math64.h>
#include <linux/stringify.h>
#include <linux/lockdep.h>
#include <linux/overflow.h>

/* Normally included by kernel.h */
#include <linux/typecheck.h>


LX_MUTEX_INIT_DECLARE(bridge_lock);

#define bridge_lock                LX_MUTEX(bridge_lock)

/***************************
 ** asm-generic/barrier.h **
 ***************************/

#define virt_mb()  smp_mb()
#define virt_rmb() smp_rmb()
#define virt_wmb() smp_wmb()
#define virt_store_mb(var, value) smp_store_mb(var, value)

#define smp_load_acquire(p)     *(p)

/****************
 ** asm/page.h **
 ****************/

/*
 * For now, hardcoded
 */
#define PAGE_SIZE 4096UL
#define PAGE_MASK (~(PAGE_SIZE-1))
#define PAGE_SHIFT 12

struct page
{
	atomic_t   _count;
	void      *addr;
	dma_addr_t paddr;
	unsigned flags;
} __attribute((packed));

struct page *virt_to_page(const void *addr);

dma_addr_t page_to_phys(struct page *page);

#include <asm-generic/getorder.h>
#include <asm-generic/ioctl.h>
#include <lx_emul/byteorder.h>

/*********************
 ** linux/pagemap.h **
 *********************/

struct address_space {
	gfp_t          gfp_mask;
	unsigned long  flags;
	struct page   *my_pages;
	unsigned long  order;
};

#define mapping_set_unevictable(mapping) do { } while (0)
#define mapping_clear_unevictable(mapping) do { } while (0)

static inline void mapping_set_gfp_mask(struct address_space *m, gfp_t mask)
{
	m->gfp_mask = mask;
}

static inline gfp_t mapping_gfp_mask(struct address_space * mapping)
{
	return mapping->gfp_mask;
}

static inline gfp_t mapping_gfp_constraint(struct address_space *mapping,
		gfp_t gfp_mask)
{
	return mapping_gfp_mask(mapping) & gfp_mask;
}

/***************************
 ** linux/pgtable_types.h **
 ***************************/

typedef unsigned long   pgprotval_t;
struct pgprot { pgprotval_t pgprot; };
typedef struct pgprot pgprot_t;

#define PAGE_KERNEL    ((pgprot_t) {0}) /* XXX */
#define PAGE_KERNEL_IO ((pgprot_t) {1}) /* XXX */

extern pgprot_t pgprot_writecombine(pgprot_t prot);
extern pgprot_t pgprot_decrypted(pgprot_t prot);

/**********************
 ** linux/mm_types.h **
 **********************/

typedef __bitwise unsigned int vm_fault_t;

enum vm_fault_reason {
	VM_FAULT_OOM            = (__force vm_fault_t)0x000001,
	VM_FAULT_SIGBUS         = (__force vm_fault_t)0x000002,
	VM_FAULT_MAJOR          = (__force vm_fault_t)0x000004,
	VM_FAULT_WRITE          = (__force vm_fault_t)0x000008,
	VM_FAULT_HWPOISON       = (__force vm_fault_t)0x000010,
	VM_FAULT_HWPOISON_LARGE = (__force vm_fault_t)0x000020,
	VM_FAULT_SIGSEGV        = (__force vm_fault_t)0x000040,
	VM_FAULT_NOPAGE         = (__force vm_fault_t)0x000100,
	VM_FAULT_LOCKED         = (__force vm_fault_t)0x000200,
	VM_FAULT_RETRY          = (__force vm_fault_t)0x000400,
	VM_FAULT_FALLBACK       = (__force vm_fault_t)0x000800,
	VM_FAULT_DONE_COW       = (__force vm_fault_t)0x001000,
	VM_FAULT_NEEDDSYNC      = (__force vm_fault_t)0x002000,
	VM_FAULT_HINDEX_MASK    = (__force vm_fault_t)0x0f0000,
};

#define VM_FAULT_ERROR (VM_FAULT_OOM | VM_FAULT_SIGBUS |	\
			VM_FAULT_SIGSEGV | VM_FAULT_HWPOISON |	\
			VM_FAULT_HWPOISON_LARGE | VM_FAULT_FALLBACK)

struct vm_operations_struct;

struct vm_area_struct {
	unsigned long                      vm_start;
	unsigned long                      vm_end;
	pgprot_t                           vm_page_prot;
	unsigned long                      vm_flags;
	const struct vm_operations_struct *vm_ops;
	unsigned long                      vm_pgoff;
	void                               *vm_private_data;
};

/****************
 ** linux/mm.h **
 ****************/

#define VM_READ       0x00000001
#define VM_WRITE      0x00000002
#define VM_MAYREAD    0x00000010
#define VM_MAYWRITE   0x00000020
#define VM_PFNMAP     0x00000400
#define VM_IO         0x00004000
#define VM_DONTEXPAND 0x00040000
#define VM_NORESERVE  0x00200000
#define VM_DONTDUMP   0x04000000
#define VM_MIXEDMAP   0x10000000

#define FAULT_FLAG_ALLOW_RETRY  0x04
#define FAULT_FLAG_RETRY_NOWAIT	0x08

#define PAGE_ALIGN(addr) ALIGN(addr, PAGE_SIZE)
#define PAGE_ALIGNED(addr) IS_ALIGNED((unsigned long)(addr), PAGE_SIZE)

struct vm_fault {
	struct vm_area_struct *vma;
	unsigned int flags;
	pgoff_t pgoff;
	unsigned long address;
};

struct vm_operations_struct {
	void (*open)(struct vm_area_struct * area);
	void (*close)(struct vm_area_struct * area);
	vm_fault_t (*fault)(struct vm_fault *vmf);
};

#define offset_in_page(p) ((unsigned long)(p) & ~PAGE_MASK)

static inline void *page_address(struct page *page) { return page ? page->addr : 0; };

extern struct page * nth_page(struct page * page, int n);

void unmap_mapping_range(struct address_space *, loff_t const, loff_t const, int);

pgprot_t vm_get_page_prot(unsigned long vm_flags);

static inline bool is_vmalloc_addr(const void *x) { return 0; }

struct page *vmalloc_to_page(const void *addr);

/******************
 ** linux/slab.h **
 ******************/

enum {
	SLAB_HWCACHE_ALIGN   = 0x00002000ul,
	SLAB_RECLAIM_ACCOUNT = 0x00020000ul,
	SLAB_PANIC           = 0x00040000ul,
	SLAB_TYPESAFE_BY_RCU = 0x00080000ul,
};

void *kzalloc(size_t size, gfp_t flags);
void *kvzalloc(size_t size, gfp_t flags);
void kfree(const void *);
void *kmalloc(size_t size, gfp_t flags);
void *krealloc(void *, size_t, gfp_t);
void *kcalloc(size_t n, size_t size, gfp_t flags);
void *kmalloc_array(size_t n, size_t size, gfp_t flags);

#define kvmalloc(size, flags) kmalloc(size, flags)
#define kvfree(p) kfree(p)

static inline void *kvmalloc_array(size_t n, size_t size, gfp_t flags)
{
	size_t bytes;

	if (unlikely(check_mul_overflow(n, size, &bytes)))
		return NULL;

	return kvmalloc(bytes, flags);
}

struct kmem_cache;
struct kmem_cache *kmem_cache_create(const char *, size_t, size_t, unsigned long, void (*)(void *));
void kmem_cache_destroy(struct kmem_cache *);
void *kmem_cache_zalloc(struct kmem_cache *k, gfp_t flags);
void *kmem_cache_alloc(struct kmem_cache *, gfp_t);
void  kmem_cache_free(struct kmem_cache *, void *);

/*********************
 ** linux/kobject.h **
 *********************/

struct sysfs_ops;

struct kobj_type {
	void (*release)(struct kobject *kobj);
	const struct sysfs_ops *sysfs_ops;
	struct attribute **default_attrs;
};


/**********************
 ** linux/kmemleak.h **
 **********************/

#ifndef CONFIG_DEBUG_KMEMLEAK
static inline void kmemleak_update_trace(const void *ptr) { }
static inline void kmemleak_alloc(const void *ptr, size_t size, int min_count,
                                  gfp_t gfp) { }
static inline void kmemleak_free(const void *ptr) { }
#endif

/********************
 ** linux/bitops.h **
 ********************/

#define __const_hweight8(w)               \
	( (!!((w) & (1ULL << 0))) +       \
	  (!!((w) & (1ULL << 1))) +       \
	  (!!((w) & (1ULL << 2))) +       \
	  (!!((w) & (1ULL << 3))) +       \
	  (!!((w) & (1ULL << 4))) +       \
	  (!!((w) & (1ULL << 5))) +       \
	  (!!((w) & (1ULL << 6))) +       \
	  (!!((w) & (1ULL << 7))) )

#define hweight16(w) (__const_hweight8(w)  + __const_hweight8((w)  >> 8 ))
#define hweight32(w) (hweight16(w) + hweight16((w) >> 16))
#define hweight64(w) (hweight32(w) + hweight32((w) >> 32))

#define clear_bit_unlock(nr, addr) clear_bit(nr, addr)

/*********************
 ** linux/cpumask.h **
 *********************/

struct cpumask { };

/*********************
 ** linux/console.h **
 *********************/

static inline bool vgacon_text_force(void) { return false; }

/******************
 ** linux/time.h **
 ******************/

#include <lx_emul/time.h>

/********************
 ** linux/time64.h **
 ********************/

typedef __s64 time64_t;
struct timespec64 {
	time64_t tv_sec;  /* seconds */
	long     tv_nsec; /* nanoseconds */
};

/*******************
 ** linux/ktime.h **
 *******************/

#define ktime_to_ns(kt) kt

extern struct timespec64 ns_to_timespec64(const s64 nsec);

#define ktime_to_timespec64(kt)	ns_to_timespec64((kt))

#define ktime_sub(lhs, rhs) (lhs - rhs)

static inline s64 ktime_to_ms(const ktime_t kt) {
	return kt / NSEC_PER_MSEC; }

/******************
 ** linux/stat.h **
 ******************/

#define S_IRUGO 00444
#define S_IWUSR 00200

/*********************
 ** linux/debugfs.h **
 *********************/

struct debugfs_reg32 {
	char *name;
	unsigned long offset;
};

struct debugfs_regset32 {
	const struct debugfs_reg32 *regs;
	int nregs;
	void __iomem *base;
};

/**********************
 ** linux/seq_file.h **
 **********************/

struct seq_file {};

/*************************************
 ** asm/processor.h (arch specific) **
 *************************************/

static inline void cpu_relax(void) {}

/******************
 ** linux/file.h **
 ******************/

struct file;

void fput(struct file *);

/*******************
 ** linux/timer.h **
 *******************/

#include <lx_emul/timer.h>
#include <lx_emul/work.h>

#define from_timer(var, callback_timer, timer_fieldname) \
	container_of(callback_timer, typeof(*var), timer_fieldname)

/*******************
 ** linux/types.h **
 *******************/

struct rcu_head { };

typedef void (*swap_func_t)(void *a, void *b, int size);

typedef int (*cmp_r_func_t)(const void *a, const void *b, const void *priv);
typedef int (*cmp_func_t)(const void *a, const void *b);

/*********************
 ** linux/vmalloc.h **
 *********************/

#include <lx_emul/vmalloc.h>

#define VM_MAP 0x00000004

/*********************
 ** linux/uaccess.h **
 *********************/

size_t copy_to_user(void *dst, void const *src, size_t len);
size_t copy_from_user(void *dst, void const *src, size_t len);

/*************************
 ** linux/percpu-defs.h **
 *************************/

#define DEFINE_PER_CPU(type, name) \
	typeof(type) name

#define this_cpu_ptr(ptr) ptr
#define per_cpu(var, cpu) var

#define cpuhp_setup_state_nocalls(a, b, c, d) 0

/**********************
 ** linux/highmem.h  **
 **********************/

static inline void *kmap(struct page *page) { return page_address(page); }
static inline void *kmap_atomic(struct page *page) { return kmap(page); }
static inline void  kunmap(struct page *page) { return; }
static inline void  kunmap_atomic(void *addr) { return; }

/******************
 ** linux/gfp.h  **
 ******************/

#define __GFP_BITS_SHIFT (25 + IS_ENABLED(CONFIG_LOCKDEP))
#define __GFP_BITS_MASK ((__force gfp_t)((1 << __GFP_BITS_SHIFT) - 1))

static inline bool gfpflags_allow_blocking(const gfp_t gfp_flags)
{
	return !!(gfp_flags & __GFP_DIRECT_RECLAIM);
}

unsigned long __get_free_pages(gfp_t gfp_mask, unsigned int order);

#define __get_free_page(gfp_mask) \
		__get_free_pages((gfp_mask), 0)

void *alloc_pages_exact(size_t size, gfp_t gfp_mask);

struct page *alloc_pages(gfp_t gfp_mask, unsigned int order);
#define alloc_page(gfp_mask) alloc_pages(gfp_mask, 0)

void free_pages(unsigned long addr, unsigned int order);
#define free_page(addr) free_pages((addr), 0)

void __free_pages(struct page *page, unsigned int order);
#define __free_page(page) __free_pages((page), 0)

/**********************
 ** linux/compiler.h **
 **********************/

static inline void __read_once_size(const volatile void *p, void *res, int size)
{
	switch (size) {
		case 1: *(__u8  *)res = *(volatile __u8  *)p; break;
		case 2: *(__u16 *)res = *(volatile __u16 *)p; break;
		case 4: *(__u32 *)res = *(volatile __u32 *)p; break;
		case 8: *(__u64 *)res = *(volatile __u64 *)p; break;
		default:
			barrier();
			__builtin_memcpy((void *)res, (const void *)p, size);
			barrier();
	}
}

#ifdef __cplusplus
#define READ_ONCE(x) \
({                                               \
	barrier(); \
	x; \
})

#else
#define READ_ONCE(x) \
({                                               \
	union { typeof(x) __val; char __c[1]; } __u; \
	__read_once_size(&(x), __u.__c, sizeof(x));  \
	__u.__val;                                   \
})
#endif

#define oops_in_progress 0

int sprintf(char *buf, const char *fmt, ...);
int snprintf(char *buf, size_t size, const char *fmt, ...);

enum { SPRINTF_STR_LEN = 64 };

#define kasprintf(gfp, fmt, ...) ({ \
		void *buf = kmalloc(SPRINTF_STR_LEN, 0); \
		sprintf(buf, fmt, __VA_ARGS__); \
		buf; \
		})

#include <lx_emul/list.h>

/**********************
 ** linux/rcupdate.h **
 **********************/

#define kfree_rcu(ptr, offset) kfree(ptr)

#define rcu_access_pointer(p) p
#define rcu_assign_pointer(p, v) p = v
#define rcu_dereference(p) p
#define rcu_dereference_protected(p, c) p
#define rcu_dereference_raw(p) p
#define rcu_dereference_check(p, c) p
#define rcu_pointer_handoff(p) (p)

static inline void rcu_read_lock(void) { };
static inline void rcu_read_unlock(void) { };

void call_rcu(struct rcu_head *, void (*)(struct rcu_head *));

#define RCU_INIT_POINTER(p, v) do { p = (typeof(*v) *)v; } while (0)

/***********************
 ** linux/interrupt.h **
 ***********************/

struct tasklet_struct
{
	unsigned long state;
	void (*func)(unsigned long);
	unsigned long data;
};

/********************
 ** linux/device.h **
 ********************/

#include <lx_emul/device.h>

#define KBUILD_MODNAME "virtio_gpu"

/*****************************
 ** linux/platform_device.h **
 *****************************/

#include <lx_emul/platform_device.h>

/***************************
 ** linux/dma-direction.h **
 ***************************/

enum dma_data_direction
{
	DMA_BIDIRECTIONAL = 0,
	DMA_TO_DEVICE     = 1,
	DMA_FROM_DEVICE   = 2
};

/*************************
 ** linux/dma-mapping.h **
 *************************/

void *dma_alloc_coherent(struct device *dev, size_t size, dma_addr_t *dma_handle, gfp_t gfp);

/**********************
 ** linux/shmem_fs.h **
 **********************/

extern struct page *shmem_read_mapping_page_gfp(struct address_space *mapping,
                                                pgoff_t index, gfp_t gfp_mask);
struct file *shmem_file_setup(const char *, loff_t, unsigned long);

static inline struct page *shmem_read_mapping_page(struct address_space *mapping, pgoff_t index)
{
	return shmem_read_mapping_page_gfp(mapping, index, mapping_gfp_mask(mapping));
}

/*****************************
 ** linux/mod_devicetable.h **
 *****************************/

struct virtio_device_id {
	__u32 device;
	__u32 vendor;
};

#define VIRTIO_DEV_ANY_ID 0xffffffff

/*******************
 ** linux/pfn_t.h **
 *******************/

#define PFN_DEV (1ULL << (BITS_PER_LONG_LONG - 3))

/********************
 ** linux/rwlock.h **
 ********************/

typedef unsigned long rwlock_t;

static inline void rwlock_init(rwlock_t *lock) { };
static inline void read_lock(rwlock_t *lock) { };
static inline void read_unlock(rwlock_t *lock) { };
static inline void write_lock(rwlock_t *lock) { };
static inline void write_unlock(rwlock_t *lock) { };

/************************
 ** linux/tracepoint.h **
 ************************/

#define EXPORT_TRACEPOINT_SYMBOL(name)

/*******************
 ** linux/sched.h **
 *******************/

enum { TASK_COMM_LEN = 16 };

enum {
	TASK_RUNNING         = 0x0,
	TASK_INTERRUPTIBLE   = 0x1,
	TASK_UNINTERRUPTIBLE = 0x2,
	TASK_NORMAL          = TASK_INTERRUPTIBLE | TASK_UNINTERRUPTIBLE,
};

#define	MAX_SCHEDULE_TIMEOUT LONG_MAX

struct mm_struct;
struct task_struct {
	struct mm_struct *mm;
	char comm[16]; /* only for debug output */
	unsigned pid;
	int      prio;
	volatile long state;
};

extern struct task_struct *current;

signed long schedule_timeout(signed long timeout);
void __set_current_state(int state);
int signal_pending(struct task_struct *p);

static inline int cond_resched(void) { return 0; }

/********************
 ** linux/kernel.h **
 ********************/

#define IS_ALIGNED(x, a) (((x) & ((typeof(x))(a) - 1)) == 0)

#define might_sleep() do { } while (0)

#define u64_to_user_ptr(x) (		\
{                                       \
	typecheck(u64, (x));            \
	(void __user *)(uintptr_t)(x);	\
}                                       \
)

#define DIV_ROUND_CLOSEST_ULL(x, divisor)(		\
{							\
	typeof(divisor) __d = divisor;			\
	unsigned long long _tmp = (x) + (__d) / 2;	\
	do_div(_tmp, __d);				\
	_tmp;						\
}							\
)

#define DIV_ROUND_DOWN_ULL(ll, d) \
	({ unsigned long long _tmp = (ll); do_div(_tmp, d); _tmp; })

#define DIV_ROUND_UP_ULL(ll, d) \
	DIV_ROUND_DOWN_ULL((unsigned long long)(ll) + (d) - 1, (d))

char *kvasprintf(gfp_t, const char *, va_list);

long simple_strtol(const char *,char **,unsigned int);

/****************
 ** linux/io.h **
 ****************/

#define readl(addr)         (*(volatile uint32_t *)(addr))

/************************
 ** linux/io-mapping.h **
 ************************/

#define pgprot_noncached(prot) prot

/*********************
 ** linux/dma-buf.h **
 *********************/

struct dma_buf;
void dma_buf_put(struct dma_buf *);
void *dma_buf_vmap(struct dma_buf *);

struct dma_buf {
	size_t size;
	void *priv;
	struct reservation_object *resv;
};

struct dma_buf_attachment {
	struct dma_buf *dmabuf;
};

/******************
 ** linux/kref.h **
 ******************/

struct kref;
unsigned int kref_read(const struct kref*);

/**************************
 ** linux/preempt_mask.h **
 **************************/

#define in_interrupt() 1

bool in_atomic();

/*********************
 ** linux/preempt.h **
 *********************/

#define preempt_enable()
#define preempt_disable()

/******************
 ** linux/kgdb.h **
 ******************/

#define in_dbg_master() (0)

/**********************
 ** linux/irqflags.h **
 **********************/

bool irqs_disabled();

/**********************
 ** linux/irq_work.h **
 **********************/

struct irq_work { };

bool irq_work_queue(struct irq_work *work);

/****************
 ** linux/fs.h **
 ****************/

struct inode;

struct inode_operations {
	void (*truncate) (struct inode *);
};

struct inode {
	const struct inode_operations *i_op;
	struct address_space *i_mapping;
};

struct file
{
	atomic_long_t         f_count;
	struct inode         *f_inode;
	struct address_space *f_mapping;
	void                 *private_data;
};

struct poll_table_struct;
typedef struct poll_table_struct poll_table;

struct file_operations {
	struct module *owner;
	int (*open) (struct inode *, struct file *);
	int (*mmap) (struct file *, struct vm_area_struct *);
	unsigned int (*poll) (struct file *, struct poll_table_struct *);
	ssize_t (*read) (struct file *, char __user *, size_t, loff_t *);
	long (*unlocked_ioctl) (struct file *, unsigned int, unsigned long);
	int (*release) (struct inode *, struct file *);
	long (*compat_ioctl) (struct file *, unsigned int, unsigned long);
	loff_t (*llseek) (struct file *, loff_t, int);
};

loff_t noop_llseek(struct file *file, loff_t offset, int whence);

struct inode *file_inode(struct file *f);

/***********************
 ** linux/sync_file.h **
 ***********************/

struct sync_file {
	struct file *file;
};

struct dma_fence;
struct dma_fence *sync_file_get_fence(int);
struct sync_file *sync_file_create(struct dma_fence *);

/************************
 ** linux/completion.h **
 ************************/

struct completion {
	unsigned done;
	void * task;
};

long __wait_completion(struct completion *work, unsigned long);

/****************
 ** linux/fb.h **
 ****************/

struct aperture;
struct apertures_struct;

/********************
 ** linux/vgaarb.h **
 ********************/

struct pci_dev;

static inline int vga_remove_vgacon(struct pci_dev *pdev) { return 0; };

/*****************
 ** linux/pci.h **
 *****************/

struct pci_dev {
	struct device dev;
	unsigned int  class;
};

#define to_pci_dev(n) NULL

/********************
 ** linux/printk.h **
 ********************/

enum { DUMP_PREFIX_NONE, };

void print_hex_dump(const char *level, const char *prefix_str,
		int prefix_type, int rowsize, int groupsize,
		const void *buf, size_t len, bool ascii);

#define pr_debug(fmt, ...)      printk(KERN_INFO fmt,   ##__VA_ARGS__)
#define pr_info(fmt, ...)       printk(KERN_INFO fmt,   ##__VA_ARGS__)
#define pr_err(fmt, ...)        printk(KERN_ERR fmt,    ##__VA_ARGS__)
#define pr_warn(fmt, ...)       printk(KERN_ERR fmt,    ##__VA_ARGS__)
#define pr_notice(fmt, ...)     printk(KERN_NOTICE fmt, ##__VA_ARGS__)

/*********************
 ** linux/pci_ids.h **
 *********************/

enum { PCI_CLASS_DISPLAY_VGA = 0x0300 };

/********************
 ** linux/string.h **
 ********************/

ssize_t strscpy(char *, const char *, size_t);

extern void *vmemdup_user(const void __user *, size_t);
void *memchr_inv(const void *s, int c, size_t n);
void *memdup_user_nul(const void __user *, size_t);

/*****************
 ** linux/i2c.h **
 *****************/

struct i2c_adapter {
	char name[48];
};

#define I2C_M_RD 0x0001

struct i2c_msg {
	__u16 addr;
	__u16 flags;
	__u16 len;
	__u8 *buf;
};

/**********************
 ** linux/refcount.h **
 **********************/

bool refcount_dec_and_test(atomic_t *);

/****************
 ** linux/of.h **
 ****************/

struct device_node { };

/*******************
 ** drm/drm_drv.h **
 *******************/

enum { O_CLOEXEC = 0xbadaffe };

void put_unused_fd(unsigned int);
void fd_install(unsigned int, struct file *);
int get_unused_fd_flags(unsigned);

/************************
 ** linux/capability.h **
 ************************/

#define CAP_SYS_ADMIN 21

#define capable(int) 0

/**********************
 ** linux/spinlock.h **
 **********************/

#define assert_spin_locked(lock)

/***************
 ** xen/xen.h **
 ***************/

#define xen_domain() 0

/********************
 ** linux/module.h **
 ********************/

#define module_driver(__driver, __register, __unregister, ...)       \
	static int __init __driver##_init(void)                      \
	{                                                            \
		return __register(&(__driver) , ##__VA_ARGS__);      \
	}                                                            \
	module_init(__driver##_init);                                \
	static void __exit __driver##_exit(void)                     \
	{                                                            \
		__unregister(&(__driver) , ##__VA_ARGS__);           \
	}                                                            \
	module_exit(__driver##_exit);

/*******************
 ** Configuration **
 *******************/

#define CONFIG_BASE_SMALL 0

/**************************
 ** Dummy trace funtions **
 **************************/

#define trace_drm_vblank_event_delivered(...)

#define trace_virtio_gpu_cmd_queue(...)
#define trace_virtio_gpu_cmd_response(...)

#define trace_dma_fence_destroy(...)       while (0) { }
#define trace_dma_fence_emit(...)          while (0) { }
#define trace_dma_fence_enable_signal(...) while (0) { }
#define trace_dma_fence_init(...)          while (0) { }
#define trace_dma_fence_signaled(...)      while (0) { }
#define trace_dma_fence_wait_end(...)      while (0) { }
#define trace_dma_fence_wait_start(...)    while (0) { }

#include <lx_emul/extern_c_end.h>

#endif /* _LX_EMUL_H_ */
