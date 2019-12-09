/*
 * \brief  C-declarations needed for device driver environment
 * \author Stefan Kalkowski
 * \author Norman Feske
 * \date   2016-06-17
 */

#ifndef _LX_EMUL_C_H_
#define _LX_EMUL_C_H_

#include <lx_emul/extern_c_begin.h>

struct drm_device;
struct drm_file;
struct drm_framebuffer;
struct drm_connector;
struct drm_display_mode;
struct drm_clip_rect;

struct lx_drm_fb {
	struct drm_device       *dev;
	struct drm_file         *file;
	struct drm_framebuffer  *drm_fb;
	unsigned                 handle;

	int                      height;
	int                      width;
	unsigned                 bpp;
	unsigned                 pitch;
	void                    *addr;
};

struct drm_file *lx_drm_file_alloc(struct drm_device *dev);

int lx_drm_framebuffer_alloc(struct drm_device *dev,
                             struct drm_file   *file,
                             struct lx_drm_fb  *lx_fb);

void lx_drm_framebuffer_free(struct lx_drm_fb  *lx_fb);

void lx_drm_set_mode(struct lx_drm_fb        *lx_fb,
		     struct drm_connector    *connector,
		     struct drm_display_mode *mode);

void lx_drm_notify_framebuffer_dirty(struct lx_drm_fb     *lx_fb,
                                     struct drm_clip_rect *rect);

void  lx_c_set_driver(struct drm_device *, void *);
void *lx_c_get_driver(struct drm_device *);

#include <lx_emul/extern_c_end.h>

#endif /* _LX_EMUL_C_H_ */
