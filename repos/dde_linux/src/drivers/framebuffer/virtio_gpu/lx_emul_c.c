/*
 * \brief  Linux emulation C helper functions
 * \author Piotr Tworek
 * \date   2019-12-27
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_emul_c.h>

#include <linux/bug.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_connector.h>
#include <drm/drm_drv.h>
#include <drm/drm_encoder.h>
#include <drm/drm_file.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_gem.h>
#include <drm/drm_mode.h>
#include <drm/drm_print.h>

#include "drm_crtc_internal.h"
#include "drm_internal.h"


struct drm_file *lx_drm_file_alloc(struct drm_device *dev)
{
	struct drm_file *file = NULL;

	file = (struct drm_file *)kzalloc(sizeof(*file), GFP_KERNEL);
	if (!file)
		return NULL;

	file->authenticated = true;

	INIT_LIST_HEAD(&file->fbs);
	mutex_init(&file->fbs_lock);

	if (drm_core_check_feature(dev, DRIVER_GEM))
		drm_gem_open(dev, file);

	/* from drm_prime_init_file_private */
	mutex_init(&file->prime.lock);
	file->prime.dmabufs = RB_ROOT;
	file->prime.handles = RB_ROOT;

	return file;
}


int lx_drm_framebuffer_alloc(struct drm_device *dev,
                             struct drm_file   *file,
                             struct lx_drm_fb  *lx_fb)
{
	BUG_ON(!dev->driver->dumb_create);

	lx_fb->dev = dev;
	lx_fb->file = file;

	struct drm_mode_create_dumb create_args;
	memset(&create_args, 0, sizeof(create_args));
	create_args.width = lx_fb->width;
	create_args.height = lx_fb->height;
	create_args.bpp = lx_fb->bpp * 8;
	int ret = dev->driver->dumb_create(file, dev, &create_args);
	if (ret) {
		DRM_ERROR("Failed to create DRM dumb buffer\n");
		return ret;
	}

	struct drm_mode_fb_cmd2 cmd;
	memset(&cmd, 0, sizeof(cmd));
	cmd.width = lx_fb->width;
	cmd.height = lx_fb->height;
	cmd.pixel_format = DRM_FORMAT_XRGB8888;
	cmd.handles[0] = create_args.handle;
	cmd.pitches[0] = create_args.pitch;

	ret = drm_mode_addfb2(dev, &cmd, file);
	if (ret) {
		DRM_ERROR("Failed to allocate DRM framebuffer\n");
		dev->driver->dumb_destroy(file, dev, create_args.handle);
		return ret;
	}

	struct drm_gem_object* fb_obj = drm_gem_object_lookup(file, create_args.handle);
	BUG_ON(!fb_obj);

	struct drm_framebuffer *fb = drm_framebuffer_lookup(dev, file, cmd.fb_id);
	if (!fb) {
		drm_mode_rmfb(dev, cmd.fb_id, file);
		dev->driver->dumb_destroy(file, dev, create_args.handle);
		return false;
	}

	/* drop the reference we picked up in framebuffer lookup */
	drm_framebuffer_put(fb);

	BUG_ON(drm_framebuffer_read_refcount(fb) != 1);

	/* Drop reference we picked up in object lookup */
	drm_gem_object_put_unlocked(fb_obj);

	ret = drm_gem_pin(fb_obj);
	if (ret) {
		DRM_ERROR("Failed to pin DRM framebuffer GEM object!\n");
		drm_mode_rmfb(dev, cmd.fb_id, file);
		dev->driver->dumb_destroy(file, dev, create_args.handle);
		return ret;
	}

	void *vaddr = drm_gem_vmap(fb_obj);
	if (!vaddr) {
		DRM_ERROR("Failed to vmap DRM framebuffer GEM object!\n");
		drm_gem_unpin(fb_obj);
		drm_mode_rmfb(dev, cmd.fb_id, file);
		dev->driver->dumb_destroy(file, dev, create_args.handle);
		return ret;
	}

	lx_fb->drm_fb = fb;
	lx_fb->pitch = fb->pitches[0] / lx_fb->bpp;
	lx_fb->addr = vaddr;
	lx_fb->handle = create_args.handle;

	return 0;
}


void lx_drm_framebuffer_free(struct lx_drm_fb  *lx_fb)
{
	struct drm_gem_object* fb_obj = drm_gem_object_lookup(lx_fb->file, lx_fb->handle);
	BUG_ON(!fb_obj);

	drm_gem_vunmap(fb_obj, lx_fb->addr);
	drm_gem_unpin(fb_obj);

	int ret = drm_mode_rmfb(lx_fb->dev, lx_fb->drm_fb->base.id, lx_fb->file);
	if (ret) {
		DRM_ERROR("Failed to free DRM framebuffer!\n");
		return;
	}

	// No specialized dumb_destroy function in kernel 5.5
	BUG_ON(lx_fb->dev->driver->dumb_destroy);

	ret = drm_gem_dumb_destroy(lx_fb->file, lx_fb->dev, lx_fb->handle);
	if (ret) {
		DRM_ERROR("Failed to destroy dumb framebuffer!\n");
	}
}


void lx_drm_set_mode(struct lx_drm_fb        *lx_fb,
		     struct drm_connector    *connector,
		     struct drm_display_mode *mode)
{
	struct drm_crtc        *crtc    = NULL;
	struct drm_encoder     *encoder = connector->encoder;

	if (!encoder) {
		struct drm_encoder *enc;
		list_for_each_entry(enc, &lx_fb->dev->mode_config.encoder_list, head) {
			if (!drm_connector_has_possible_encoder(connector, enc))
				continue;

			bool used = false;
			struct drm_connector *c;
			list_for_each_entry(c, &lx_fb->dev->mode_config.connector_list, head) {
				if (c->encoder == enc) used = true;
			}
			if (used) continue;
			encoder = enc;
			break;
		}
	}

	if (!encoder) {
		DRM_ERROR("Found no encoder for the connector %s\n", connector->name);
		return;
	}

	unsigned used_crtc = 0;

	crtc = encoder->crtc;
	if (!crtc) {
		unsigned i = 0;
		struct drm_crtc *c;
		list_for_each_entry(c, &lx_fb->dev->mode_config.crtc_list, head) {
			if (!(encoder->possible_crtcs & (1 << i))) continue;
			if (c->state->enable) {
				used_crtc ++;
				continue;
			}
			crtc = c;
			break;
		}
	}

	if (!crtc) {
		if (mode)
			DRM_ERROR("Found no crtc for the connector %s used/max %u+1/%u\n",
			          connector->name, used_crtc, lx_fb->dev->mode_config.num_crtc);
		return;
	}

	DRM_DEBUG("%s%s for connector %s\n", mode ? "set mode " : "no mode",
	          mode ? mode->name : "", connector->name);

	struct drm_mode_set set;
	set.crtc = crtc;
	set.x = 0;
	set.y = 0;
	set.mode = mode;
	set.connectors = &connector;
	set.num_connectors = mode ? 1 : 0;
	set.fb = mode ? lx_fb->drm_fb : 0;

	uint32_t const ref_cnt_before = drm_framebuffer_read_refcount(lx_fb->drm_fb);
	int ret = drm_atomic_helper_set_config(&set, lx_fb->dev->mode_config.acquire_ctx);
	if (ret)
		DRM_ERROR("Error: set config failed ret=%d refcnt before=%u after=%u\n",
		          ret, ref_cnt_before, drm_framebuffer_read_refcount(lx_fb->drm_fb));
}


void lx_drm_notify_framebuffer_dirty(struct lx_drm_fb     *lx_fb,
                                     struct drm_clip_rect *rect)
{
	struct drm_mode_fb_dirty_cmd cmd;
	memset(&cmd, 0, sizeof(cmd));
	cmd.fb_id = lx_fb->drm_fb->base.id;
	cmd.num_clips = 1;
	cmd.clips_ptr = (unsigned long)rect;

	if (drm_mode_dirtyfb_ioctl(lx_fb->dev, &cmd, lx_fb->file)) {
		DRM_ERROR("drm_mode_dirtyfb_ioctl failed!\n");
	}
}


void lx_c_set_driver(struct drm_device *dev, void *driver)
{
	BUG_ON(dev->fb_helper);
	dev->fb_helper = (struct drm_fb_helper *) driver;
}


void* lx_c_get_driver(struct drm_device *dev)
{
	return (void*) dev->fb_helper;
}
