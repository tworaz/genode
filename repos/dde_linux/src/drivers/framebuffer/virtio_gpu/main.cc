/*
 * \brief  VirtIO GPU framebuffer driver
 * \author Norman Feske
 * \author Piotr Tworek
 * \author Stefan Kalkowski
 * \date   2019-12-08
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <base/component.h>
#include <base/heap.h>
#include <base/log.h>
#include <base/attached_rom_dataspace.h>
#include <virtio_device/client.h>
#include <virtio_session/connection.h>

/* Server related local includes */
#include <component.h>

/* Linux emulation environment includes */
#include <lx_emul.h>
#include <lx_emul/platform_device.h>

/* Linux kit includes */
#include <lx_kit/backend_alloc.h>
#include <lx_kit/env.h>
#include <lx_kit/malloc.h>
#include <lx_kit/scheduler.h>
#include <lx_kit/timer.h>
#include <lx_kit/work.h>

/* Linux module functions */
extern "C" int core_virtio_init(void);
extern "C" int module_virtio_genode_init(void);
extern "C" int module_virtio_gpu_driver_init(void);
extern "C" void radix_tree_init();
extern "C" void drm_connector_ida_init();

static void run_linux(void * m);

unsigned long jiffies;
unsigned int drm_debug;

Genode::Ram_dataspace_capability
Lx::backend_alloc(Genode::addr_t size, Genode::Cache_attribute cached) {
	return Lx_kit::env().env().ram().alloc(size, cached); }

void Lx::backend_free(Genode::Ram_dataspace_capability cap) {
	return Lx_kit::env().env().ram().free(cap); }


struct Main
{

	struct Device_not_found   : Genode::Exception { };

	Genode::Env                   &env;
	Genode::Entrypoint            &ep     { env.ep() };
	Virtio::Connection             bus    { env };
	Virtio::Device_client          device;
	Genode::Heap                   heap   { env.ram(), env.rm() };
	Framebuffer::Root              root   { env, heap, Lx_kit::env().config_rom() };

	/* Linux task that handles the initialization */
	Genode::Constructible<Lx::Task> linux;

	Virtio::Device_client _init_device()
	{
		Virtio::Device_capability device_cap;
		bus.with_upgrade([&] () { device_cap = bus.first_device(Virtio::Device_type::GPU); });
		if (!device_cap.valid()) {
			Genode::warning("No VirtIO GPU device found!");
			throw Device_not_found();
		}
		return Virtio::Device_client(device_cap);
	}

	Main(Genode::Env &env) : env(env), device(_init_device())
	{
		Genode::log("--- VirtIO GPU framebuffer driver ---");

		LX_MUTEX_INIT(bridge_lock);

		/* init singleton Lx::Scheduler */
		Lx::scheduler(&env);

		Lx::malloc_init(env, heap);

		/* init singleton Lx::Timer */
		Lx::timer(&env, &ep, &heap, &jiffies);

		/* init singleton Lx::Work */
		Lx::Work::work_queue(&heap);

		linux.construct(run_linux, reinterpret_cast<void*>(this),
		                "linux", Lx::Task::PRIORITY_0, Lx::scheduler());

		/* give all task a first kick before returning */
		Lx::scheduler().schedule();
	}

	void announce() { env.parent().announce(ep.manage(root)); }

	Lx::Task &linux_task() { return *linux; }
};


struct Policy_agent
{
	Main &main;
	Genode::Signal_handler<Policy_agent> handler;
	bool _pending = false;

	void handle()
	{
		_pending = true;
		main.linux_task().unblock();
		Lx::scheduler().schedule();
	}

	bool pending()
	{
		bool ret = _pending;
		_pending = false;
		return ret;
	}

	Policy_agent(Main &m)
	: main(m), handler(main.ep, *this, &Policy_agent::handle) {}
};


static void run_linux(void *m)
{
	Main *main = reinterpret_cast<Main*>(m);

	system_wq = alloc_workqueue("system_wq", 0, 0);

	current = (struct task_struct *)kzalloc(sizeof(struct task_struct), GFP_KERNEL);
	Genode::strncpy(current->comm, "main", 4);

	drm_debug = Lx_kit::env().config_rom().xml().attribute_value<unsigned>("drm_debug", 0);

	radix_tree_init();
	platform_bus_init();
	core_virtio_init();
	drm_connector_ida_init();

	module_virtio_genode_init();
	module_virtio_gpu_driver_init();

	struct platform_device *pdev =
            (struct platform_device *)kzalloc(sizeof(struct platform_device), 0);
	pdev->name = "virtio_gpu";
	pdev->driver_override = "virtio_genode";
	pdev->lx_private_data = &main->device;
	platform_device_register(pdev);

	main->root.session.driver().finish_initialization();
	main->announce();

	Policy_agent pa(*main);
	main->root.session.driver().config_sigh(pa.handler);
	Lx_kit::env().config_rom().sigh(pa.handler);

	while (1) {
		Lx::scheduler().current()->block_and_schedule();
		while (pa.pending())
			main->root.session.config_changed();
	}
}


void Component::construct(Genode::Env &env)
{
	/* XXX execute constructors of global statics */
	env.exec_static_constructors();

	Lx_kit::construct_env(env);

	static Main m(env);
}
