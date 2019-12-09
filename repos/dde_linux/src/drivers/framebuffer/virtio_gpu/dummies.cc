/*
 * \brief  Stub linux functions for VirtIO GPU driver port.
 * \author Piotr Tworek
 * \date   2019-12-07
 *
 * The content of this file, in particular data structures, is partially
 * derived from Linux-internal headers.
 */

/* Genode includes */
#include <base/log.h>
#include <base/sleep.h>

extern "C" {
	typedef long DUMMY;

enum {
	SHOW_DUMMY = 1,
	SHOW_SKIP  = 0,
	SHOW_RET   = 1,
};

#define DUMMY(retval, name) \
	DUMMY name(void) { \
		if (SHOW_DUMMY) \
			Genode::log(__func__, ": " #name " called " \
			            "(from ", __builtin_return_address(0), ") " \
			            "not implemented"); \
		return retval; \
	}

#define DUMMY_SKIP(retval, name) \
	DUMMY name(void) { \
		if (SHOW_SKIP) \
			Genode::log(__func__, ": " #name " called " \
			            "(from ", __builtin_return_address(0), ") " \
			            "skipped"); \
		return retval; \
	}

#define DUMMY_RET(retval, name) \
	DUMMY name(void) { \
		if (SHOW_RET) \
			Genode::log(__func__, ": " #name " called " \
			            "(from ", __builtin_return_address(0), ") " \
			            "return ", retval); \
		return retval; \
	}

#define DUMMY_STOP(retval, name) \
	DUMMY name(void) { \
		do { \
			Genode::warning(__func__, ": " #name " called " \
			               "(from ", __builtin_return_address(0), ") " \
			               "stopped"); \
			Genode::sleep_forever(); \
		} while (0); \
		return retval; \
	}

DUMMY_SKIP(0, spin_lock_init)
DUMMY_SKIP(0, spin_lock)
DUMMY_SKIP(0, spin_unlock)
DUMMY_SKIP(0, spin_lock_irqsave)
DUMMY_SKIP(0, spin_unlock_irqrestore)
DUMMY_SKIP(0, spin_lock_irq)
DUMMY_SKIP(0, spin_unlock_irq)
DUMMY_STOP(0, assert_spin_locked)

DUMMY_SKIP(-1, check_move_unevictable_pages)

DUMMY_STOP(-1, drm_open)
DUMMY_STOP(-1, drm_poll)
DUMMY_STOP(-1, drm_read)
DUMMY_STOP(-1, drm_ioctl)
DUMMY_STOP(-1, drm_release)
DUMMY_STOP(-1, drm_dev_enter)
DUMMY_STOP(-1, drm_dev_exit)
DUMMY_STOP(-1, drm_prime_gem_destroy)
DUMMY_STOP(-1, noop_llseek)
DUMMY_STOP(-1, _drm_lease_held)

DUMMY_STOP(-1, fd_install)
DUMMY_STOP(-1, put_unused_fd)
DUMMY_STOP(-1, get_unused_fd_flags)

DUMMY_STOP(0, vmemdup_user)

DUMMY_STOP(-1, signal_pending)

DUMMY_STOP(-1, sync_file_create)
DUMMY_STOP(-1, sync_file_get_fence)

DUMMY_STOP(0, wake_up_state)

DUMMY_STOP(0, print_hex_dump)

DUMMY_STOP(0, atomic64_read)
DUMMY_STOP(0, atomic64_set)
DUMMY_STOP(0, atomic64_add)

DUMMY_STOP(0, strscpy)

DUMMY_STOP(0, dma_alloc_coherent)
DUMMY_STOP(0, dma_free_coherent)
DUMMY_STOP(0, dma_sync_sg_for_device)
DUMMY_STOP(0, dma_map_sg)
DUMMY_STOP(0, dma_unmap_sg)
DUMMY_STOP(0, dma_map_single)
DUMMY_STOP(0, dma_unmap_single)

DUMMY_STOP(-1, dma_buf_put)
DUMMY_STOP(-1, dma_buf_vmap)
DUMMY_STOP(-1, dma_buf_vunmap)

DUMMY_STOP(-1, drm_gem_prime_handle_to_fd)
DUMMY_STOP(-1, drm_gem_prime_fd_to_handle)
DUMMY_STOP(-1, drm_gem_prime_mmap)
DUMMY_SKIP(-1, drm_prime_remove_buf_handle_locked)

DUMMY_STOP(0, set_page_dirty)
DUMMY_STOP(0, mark_page_accessed)
DUMMY_STOP(0, vm_get_page_prot)
DUMMY_STOP(0, vma_pages)
DUMMY_STOP(1, vmf_insert_page)

DUMMY_STOP(0, irq_work_queue)

DUMMY_STOP(0, add_uevent_var)

DUMMY_STOP(0, pgprot_decrypted)

} /* extern "C" */
