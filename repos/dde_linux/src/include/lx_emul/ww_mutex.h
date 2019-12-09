/*
 * \brief  Linux kernel API
 * \author Piotr Tworek
 * \date   2019-12-20
 *
 * Based on the prototypes found in the Linux kernel's 'include/'.
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/**********************
 ** linux/ww_mutex.h **
 **********************/

struct ww_acquire_ctx
{
	unsigned int acquired;
};

struct ww_class { };

struct ww_mutex
{
	struct mutex base;
	struct ww_acquire_ctx *ctx;
};

#define DEFINE_WD_CLASS(classname) \
	struct ww_class classname;
#define DEFINE_WW_CLASS(classname) \
	struct ww_class classname;
#define DEFINE_WW_MUTEX(mutexname, ww_class) \
	struct ww_mutex mutexname;

static inline void ww_mutex_init(struct ww_mutex *lock, struct ww_class *ww_class)
{
	mutex_init(&lock->base);
	lock->ctx = NULL;
}

static inline void ww_mutex_destroy(struct ww_mutex *lock)
{
	mutex_destroy(&lock->base);
}

static inline int ww_mutex_lock(struct ww_mutex *lock, struct ww_acquire_ctx *ww_ctx)
{
	if (ww_ctx) {
		if (ww_ctx == lock->ctx)
			return -EALREADY;

		BUG_ON(lock->ctx);
		ww_ctx->acquired++;
		lock->ctx = ww_ctx;
	}
	mutex_lock(&lock->base);
	return 0;
}

static inline void ww_mutex_unlock(struct ww_mutex *lock)
{
	if (lock->ctx) {
		if (lock->ctx->acquired > 0)
			lock->ctx->acquired--;
		lock->ctx = NULL;
	}
	mutex_unlock(&lock->base);
}

static inline int ww_mutex_trylock(struct ww_mutex *lock)
{
	return mutex_trylock(&lock->base);
}

static inline bool ww_mutex_is_locked(struct ww_mutex *lock)
{
	return mutex_is_locked(&lock->base);
}

#define ww_mutex_lock_slow ww_mutex_lock
#define ww_mutex_lock_slow_interruptible ww_mutex_lock
#define ww_mutex_lock_interruptible ww_mutex_lock

#define ww_acquire_init(ctx, class)
#define ww_acquire_done(ctx)
#define ww_acquire_fini(ctx)
