/*
 * \brief  Genode implementation of linux VirtIO backend driver
 * \author Piotr Tworek
 * \date   2019-12-07
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <base/env.h>
#include <base/log.h>
#include <base/rpc_server.h>
#include <irq_session/client.h>
#include <virtio_device/client.h>
#include <virtio_device/virt_queue.h>

#include <lx_kit/env.h>
#include <lx_kit/malloc.h>

#include <lx_emul.h>
#include <lx_emul/device.h>
#include <lx_emul/extern_c_begin.h>
#include <linux/virtio.h>
#include <linux/virtio_config.h>
#include <linux/virtio_ring.h>
#include <lx_emul/extern_c_end.h>

#define to_virtio_genode_device(_plat_dev) \
	container_of(_plat_dev, struct virtio_genode_device, vdev)

struct virtio_genode_device;

class Irq_handler {

	private:

		Genode::Signal_handler<Irq_handler> _handler;
		Genode::Irq_session_client          _irq;
		struct virtio_genode_device        *_vg_dev;

		void _handle_irq();

	public:

		Irq_handler(Genode::Env &env,
		            Genode::Irq_session_capability &irq_cap,
			    struct virtio_genode_device *vg_dev)
		: _handler(env.ep(), *this, &Irq_handler::_handle_irq)
		, _irq(irq_cap), _vg_dev(vg_dev)
		{
			_irq.sigh(_handler);
			_irq.ack_irq();
		}
};

struct virtio_genode_device
{
	struct virtio_device                         vdev;
	struct platform_device                      *pdev;
	Virtio::Device_client                       *device_client;
	Genode::Constructible<Irq_handler>           irq;

	/* a list of queues so we can dispatch IRQs */
	struct list_head virtqueues;
};

struct virtio_genode_vq_info {
	/* the actual virtqueue */
	struct virtqueue *vq;
	/* the list node for the virtqueues list */
	struct list_head node;
};

enum {
	INT_VRING  = 1 << 0,
	INT_CONFIG = 1 << 1
};

void Irq_handler::_handle_irq() {
	int ret = IRQ_NONE;

	const uint32_t status = _vg_dev->device_client->read_isr();

	if (unlikely(status & INT_CONFIG)) {
		virtio_config_changed(&_vg_dev->vdev);
		ret = IRQ_HANDLED;
	}

	if (likely(status & INT_VRING)) {
		struct virtio_genode_vq_info *info;
		// vring_interript does not use this for anything.
		const int irq = 0;

		list_for_each_entry(info, &_vg_dev->virtqueues, node)
			ret |= vring_interrupt(irq, info->vq);
	}

	if (ret == IRQ_HANDLED) {
		_irq.ack_irq();
	} else {
		Genode::error("Unhandled IRQ");
		BUG();
	}
}

static bool vg_notify(struct virtqueue *vq)
{
	to_virtio_genode_device(vq->vdev)->device_client->notify_buffers_available(vq->index);
	return true;
}

static struct virtqueue *vg_setup_vq(struct virtio_device *vdev, unsigned index,
                                     void (*callback)(struct virtqueue *vq),
                                     const char *name, bool ctx)
{
	auto *vg_dev = to_virtio_genode_device(vdev);
	struct virtio_genode_vq_info *info;

	/* Allocate and fill out our active queue description */
	info = (struct virtio_genode_vq_info *)kmalloc(sizeof(*info), GFP_KERNEL);
	if (!info) {
		return (struct virtqueue *)ERR_PTR(-ENOMEM);
	}

	const uint16_t num = vg_dev->device_client->get_max_queue_size(index);
	auto *vq = vring_create_virtqueue(index, num, PAGE_SIZE, vdev,
				          true, true, ctx, vg_notify, callback, name);
	if (!vq) {
		dev_err(&vdev->dev, "Failed to allocate virtqueue!\n");
		kfree(info);
		return (struct virtqueue *)ERR_PTR(-ENOMEM);
	}

	Virtio::Queue_description dsc;
	dsc.desc  = virtqueue_get_desc_addr(vq);
	dsc.avail = virtqueue_get_avail_addr(vq);
	dsc.used  = virtqueue_get_used_addr(vq);
	dsc.size  = virtqueue_get_vring_size(vq);

	vq->priv = info;
	info->vq = vq;

	list_add(&info->node, &vg_dev->virtqueues);

	vg_dev->device_client->configure_queue(index, dsc);

	return vq;
}

static void vg_get(struct virtio_device *vdev, unsigned offset, void *buf, unsigned len)
{
	using Access_size = Virtio::Device::Access_size;

	auto *vg_dev = to_virtio_genode_device(vdev);

	uint32_t val;
	switch (len) {
		case 4:
			val = vg_dev->device_client->read_config(offset, Access_size::ACCESS_32BIT);
			memcpy(buf, &val, sizeof val);
			break;
		default:
			BUG();
	}
}

static void vg_set(struct virtio_device *vdev, unsigned offset, const void *buf, unsigned len)
{
	using Access_size = Virtio::Device::Access_size;

	auto *vg_dev = to_virtio_genode_device(vdev);

	switch (len) {
		case 4:
			uint32_t val;
			memcpy(&val, buf, len);
			vg_dev->device_client->write_config(offset, Access_size::ACCESS_32BIT, val);
			break;
		default:
			BUG();
	}
}

static u32 vg_generation(struct virtio_device *vdev)
{
	auto *vg_dev = to_virtio_genode_device(vdev);
	return vg_dev->device_client->get_config_generation();
}

static u8 vg_get_status(struct virtio_device *vdev) {
	return to_virtio_genode_device(vdev)->device_client->get_status(); }

static void vg_set_status(struct virtio_device *vdev, u8 status)
{
	/* We should never be setting status to 0. */
	BUG_ON(status == 0);

	to_virtio_genode_device(vdev)->device_client->set_status(status);
}

static void vg_reset(struct virtio_device *vdev)
{
	to_virtio_genode_device(vdev)->device_client->set_status(
		Virtio::Device::Status::RESET);
}

static void vg_del_vqs(struct virtio_device *vdev)
{
	BUG();
}

static int vg_find_vqs(struct virtio_device *vdev, unsigned nvqs,
		       struct virtqueue *vqs[],
		       vq_callback_t *callbacks[],
		       const char * const names[],
		       const bool *ctx,
		       struct irq_affinity *desc)
{
	auto *vg_dev = to_virtio_genode_device(vdev);
	int queue_idx = 0;

	vg_dev->irq.construct(Lx_kit::env().env(), vg_dev->device_client->irq(), vg_dev);

	for (unsigned i = 0; i < nvqs; ++i) {
		if (!names[i]) {
			vqs[i] = NULL;
			continue;
		}

		vqs[i] = vg_setup_vq(vdev, queue_idx++, callbacks[i], names[i],
                                     ctx ? ctx[i] : false);
		if (IS_ERR(vqs[i])) {
			vg_del_vqs(vdev);
			return PTR_ERR(vqs[i]);
		}
	}

	return 0;
}

static u64 vg_get_features(struct virtio_device *vdev)
{
	auto *vg_dev = to_virtio_genode_device(vdev);
	u64 features;

	features = vg_dev->device_client->get_features(1);
	features <<= 32;
	features |= vg_dev->device_client->get_features(0);

	return features;
}

static int vg_finalize_features(struct virtio_device *vdev)
{
	auto *vg_dev = to_virtio_genode_device(vdev);

	/* Give virtio_ring a chance to accept features. */
	vring_transport_features(vdev);

	/* Make sure there is are no mixed devices */
	if (!__virtio_test_bit(vdev, VIRTIO_F_VERSION_1)) {
		dev_err(&vdev->dev, "Genode VirtIO requires VIRTIO_F_VERSION_1 feature!\n");
		return -EINVAL;
	}

	vg_dev->device_client->set_features(1, (uint32_t)(vdev->features >> 32));
	vg_dev->device_client->set_features(0, (uint32_t)vdev->features);

	return 0;
}

static const char *vg_bus_name(struct virtio_device *vdev)
{
	auto *vg_dev = to_virtio_genode_device(vdev);
	return vg_dev->pdev->name;
}

static const struct virtio_config_ops virtio_genode_config_ops = {
	.get               = vg_get,
	.set               = vg_set,
	.generation	   = vg_generation,
	.get_status	   = vg_get_status,
	.set_status	   = vg_set_status,
	.reset		   = vg_reset,
	.find_vqs	   = vg_find_vqs,
	.del_vqs	   = vg_del_vqs,
	.get_features	   = vg_get_features,
	.finalize_features = vg_finalize_features,
	.bus_name	   = vg_bus_name,
};

static void virtio_genode_release_dev(struct device *dev)
{
	BUG();
}

static int virtio_genode_probe(struct platform_device *pdev)
{
	struct virtio_genode_device *vg_dev;

	BUG_ON(!pdev->lx_private_data);

	vg_dev = (struct virtio_genode_device *)kzalloc(sizeof(*vg_dev), 0);
	if (!vg_dev)
		return -ENOMEM;

	vg_dev->vdev.dev.parent  = &pdev->dev;
	vg_dev->vdev.dev.release = virtio_genode_release_dev;
	vg_dev->vdev.config      = &virtio_genode_config_ops;
	vg_dev->pdev             = pdev;

	INIT_LIST_HEAD(&vg_dev->virtqueues);

	vg_dev->device_client    = static_cast<Virtio::Device_client *>(pdev->lx_private_data);

	vg_dev->vdev.id.device   = vg_dev->device_client->device_id();
	vg_dev->vdev.id.vendor   = vg_dev->device_client->vendor_id();

	int rc = register_virtio_device(&vg_dev->vdev);
	if (rc) {
		dev_err(&pdev->dev, "Failed to register VirtIO device!\n");
		put_device(&vg_dev->vdev.dev);
	}

	return rc;
}

static struct platform_driver virtio_genode_driver = {
	.probe   = virtio_genode_probe,
	.driver  = {
		.name = "virtio_genode",
	},
};

extern "C" {

int virtio_genode_init(void)
{
	return platform_driver_register(&virtio_genode_driver);
}

module_init(virtio_genode_init);

}
