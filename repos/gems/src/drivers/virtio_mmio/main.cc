/*
 * \brief  VirtIO MMIO driver
 * \author Piotr Tworek
 * \date   2019-09-27
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <bus.h>
#include <device_mmio.h>
#include <virtio_device/xml_node.h>

#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/heap.h>
#include <base/log.h>
#include <base/quota_guard.h>
#include <base/tslab.h>
#include <irq_session/connection.h>
#include <os/session_policy.h>
#include <root/component.h>
#include <util/list.h>
#include <util/string.h>
#include <virtio_device/virtio_device.h>
#include <virtio_session/virtio_session.h>

namespace Virtio_mmio
{
	using namespace Genode;
	using namespace Virtio;

	class Device_component;
	class Root;
	class Session_component;
	struct Main;
};


class Virtio_mmio::Device_component : public Genode::Rpc_object<Virtio::Device>,
                                      private Virtio_mmio::Device_mmio,
                                      private Genode::List<Device_component>::Element
{
	private:

		friend class Genode::List<Device_component>;

		/*
		 * Noncopyable
		 */
		Device_component(Device_component const &);
		Device_component &operator = (Device_component const &);

		Genode::Env             &_env;
		Genode::Cap_quota_guard &_cap_guard;
		Device_description      &_device_description;
		Genode::Irq_connection   _irq_connection;


		/***********************************
		 ** Virtio::Device implementation **
		 ***********************************/

		uint32_t vendor_id() override { return read<VendorID>(); }
		uint32_t device_id() override { return read<DeviceID>(); }

		uint8_t get_status() override {
			return read<Device_mmio::Status>() & 0xff; }

		bool set_status(uint8_t status) override
		{
			write<Device_mmio::Status>(status);
			return read<Device_mmio::Status>() == status;
		}

		uint32_t get_features(uint32_t selection) override
		{
			write<DeviceFeaturesSel>(selection);
			return read<DeviceFeatures>();
		}

		void set_features(uint32_t selection, uint32_t features) override
		{
			write<DriverFeaturesSel>(selection);
			write<DriverFeatures>(features);
		}

		uint32_t read_config(uint8_t offset, Access_size size) override
		{
			switch (size) {
			case Device::ACCESS_8BIT:  return read<Config_8>(offset);
			case Device::ACCESS_16BIT: return read<Config_16>(offset >> 1);
			case Device::ACCESS_32BIT: return read<Config_32>(offset >> 2);
			}
			return 0;
		}

		void write_config(uint8_t offset, Access_size size, uint32_t value) override
		{
			switch (size) {
			case Device::ACCESS_8BIT:  write<Config_8>(value,  offset);  break;
			case Device::ACCESS_16BIT: write<Config_16>(value, (offset >> 1)); break;
			case Device::ACCESS_32BIT: write<Config_32>(value, (offset >> 2)); break;
			}
		}

		uint8_t get_config_generation() override {
			return read<ConfigGeneration>() & 0xff; }

		uint16_t get_max_queue_size(uint16_t queue_index) override
		{
			write<QueueSel>(queue_index);
			if (read<QueueReady>() != 0)
				return 0;
			return read<QueueNumMax>();
		}

		bool configure_queue(uint16_t queue_index, Virtio::Queue_description desc) override
		{
			write<QueueSel>(queue_index);

			if (read<QueueReady>() != 0)
				return false;

			write<QueueNum>(desc.size);

			uint64_t addr = desc.desc;
			write<QueueDescLow>((uint32_t)addr);
			write<QueueDescHigh>((uint32_t)(addr >> 32));

			addr = desc.avail;
			write<QueueAvailLow>((uint32_t)addr);
			write<QueueAvailHigh>((uint32_t)(addr >> 32));

			addr = desc.used;
			write<QueueUsedLow>((uint32_t)addr);
			write<QueueUsedHigh>((uint32_t)(addr >> 32));

			write<QueueReady>(1);
			return read<QueueReady>() != 0;
		}

		Genode::Irq_session_capability irq() override {
			return _irq_connection.cap(); }

		uint32_t read_isr() override
		{
			uint32_t isr = read<InterruptStatus>();
			write<InterruptAck>(isr);
			return isr;
		}

		void notify_buffers_available(uint16_t queue_index) override {
			write<QueueNotify>(queue_index); }

	public:

		const Device_description &description() const {
			return _device_description; }

		Device_component(Genode::Env             &env,
		                 Genode::Cap_quota_guard &cap_guard,
		                 Device_description      &description)
		: Device_mmio(env, description.base, description.size),
		  _env(env),
		  _cap_guard(cap_guard),
		  _device_description(description),
		  _irq_connection(_env,
		                  description.irq,
		                  description.irq_trigger,
		                  description.irq_polarity)
		{
			/* RPC ep + mmio cap, irq cap */
			_cap_guard.withdraw(Genode::Cap_quota{3});
			_device_description.claimed = true;
		}

		~Device_component()
		{
			_cap_guard.replenish(Genode::Cap_quota{3});
			_device_description.claimed = false;
		}
};


class Virtio_mmio::Session_component : public Rpc_object<Virtio::Session>
{
	private:

		/*
		 * Noncopyable
		 */
		Session_component(Session_component const &);
		Session_component &operator = (Session_component const &);

		enum {
			SLAB_BLOCK_SIZE = (sizeof(Device_component) + 32) * DEVICE_SLOT_COUNT,
		};

		Genode::Env                                     &_env;
		Genode::Attached_rom_dataspace                  &_config;
		Bus::Device_list                                &_device_description_list;
		Genode::Cap_quota_guard                          _cap_guard;
		Genode::Session_label                      const _label;
		Genode::Session_policy                     const _policy { _label, _config.xml() };
		Genode::uint8_t                                  _slab_data[SLAB_BLOCK_SIZE];
		Genode::Tslab<Device_component, SLAB_BLOCK_SIZE> _slab;
		Genode::List<Device_component>                   _device_list { };


		/**
		 * Check according session policy device usage
		 */
		bool _permit_device(Device_type type)
		{
			try {
				_policy.for_each_sub_node("device", [&] (Xml_node node) {

					if (!node.has_attribute("type"))
						return;

					Device_type type_from_policy =
						node.attribute_value("type", Device_type::INVALID);

					if (type == type_from_policy)
						throw true;
				});
			} catch (bool result) { return result; }

			return false;
		}

		Device_capability _find_device(Device_component* prev_device, Device_type type)
		{
			Device_description *desc = nullptr;

			if (!_permit_device(type))
				return Device_capability();

			if (prev_device) {
				desc = prev_device->description().next();
			} else {
				desc = _device_description_list.first();
			}

			while (desc) {
				if (!desc->claimed && desc->type == type)
					break;
				desc = desc->next();
			}

			if (!desc)
				return Device_capability();

			try {
				auto *dev = new (_slab) Device_component(_env, _cap_guard, *desc);
				_device_list.insert(dev);
				return _env.ep().rpc_ep().manage(dev);
			} catch (Device_mmio::Probe_failed) {
				warning("'", _label ,"' - probe failed during Device_component construction!");
				return Device_capability();
			} catch (Genode::Out_of_ram) {
				warning("'", _label, "' - too many claimed devices!");
				throw Out_of_device_slots();
			} catch (Genode::Out_of_caps) {
				warning("'", _label, "' - Out_of_caps during Device_component construction!");
				throw;
			}
		}

	public:

		Session_component(Genode::Env                      &env,
		                  Genode::Attached_rom_dataspace   &config,
		                  Genode::List<Device_description> &device_config_list,
		                  char                       const *args)
		: _env(env),
		  _config(config),
		  _device_description_list(device_config_list),
		  _cap_guard(Genode::cap_quota_from_args(args)),
		  _label(Genode::label_from_args(args)),
		  _slab(nullptr, _slab_data) { }

		~Session_component()
		{
			while (_device_list.first())
				release_device(static_cast<Rpc_object<Virtio::Device> *>(
					_device_list.first())->cap());
		}

		void upgrade_resources(Genode::Session::Resources resources) {
			_cap_guard.upgrade(resources.cap_quota); }

		Device_capability first_device(Device_type type) override
		{
			auto lambda = [&] (Device_component *prev) {
				return _find_device(prev, type); };
			return _env.ep().rpc_ep().apply(Device_capability(), lambda);
		}

		Device_capability next_device(Device_capability prev_device) override
		{
			auto lambda = [&] (Device_component *prev) {
				if (!prev)
					return Device_capability();
				return _find_device(prev, prev->description().type);
			};
			return _env.ep().rpc_ep().apply(prev_device, lambda);
		}

		void release_device(Device_capability device_cap) override
		{
			Device_component *device;
			auto lambda = [&] (Device_component *d) {
				device = d;
				if (!device)
					return;

				_device_list.remove(device);
				_env.ep().rpc_ep().dissolve(device);
			};

			_env.ep().rpc_ep().apply(device_cap, lambda);

			if (device)
				destroy(_slab, device);
		}
};


class Virtio_mmio::Root : public Root_component<Session_component>
{
	private:

		/*
		 * Noncopyable
		 */
		Root(Root const &);
		Root &operator = (Root const &);

		Genode::Env                    &_env;
		Genode::Attached_rom_dataspace  _config_rom { _env, "config" };
		Genode::Heap                    _local_heap { _env.ram(), _env.rm() };
		Bus::Device_list                _device_description_list {};

		Session_component *_create_session(const char *args) override
		{
			return new (md_alloc()) Session_component(
				_env, _config_rom, _device_description_list, args);
		}

		void _upgrade_session(Session_component *s, const char *args) override {
			s->upgrade_resources(Genode::session_resources_from_args(args)); }

	public:

		Root(Env &env, Allocator &md_alloc)
		: Root_component<Session_component>(env.ep(), md_alloc), _env(env)
		{
			size_t const device_number = Virtio_mmio::Bus::scan(
				_env, _local_heap, _device_description_list);
			if (!_device_description_list.first()) {
				warning("No VirtIO devices found!");
				env.parent().exit(-1);
			}
			log("Probe finished, found ", device_number, " VirtIO device(s).");
		}
};


struct Virtio_mmio::Main
{
	Genode::Env         &_env;
	Genode::Sliced_heap  _heap { _env.ram(), _env.rm() };
	Root                 _root { _env, _heap };

	Main(Genode::Env &env) : _env(env)
	{
		log("--- VirtIO MMIO driver started ---");
		env.parent().announce(env.ep().manage(_root));
	}
};


void Component::construct(Genode::Env &env)
{
	static Virtio_mmio::Main main(env);
}
