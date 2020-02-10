/**
 * \brief  VirtIO based input driver
 * \author Piotr Tworek
 * \date   2020-02-01
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode */
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/env.h>
#include <base/heap.h>
#include <base/log.h>
#include <input/component.h>
#include <input/keycodes.h>
#include <input/root.h>
#include <irq_session/client.h>
#include <root/component.h>
#include <util/register.h>
#include <virtio_device/client.h>
#include <virtio_device/virt_queue.h>
#include <virtio_session/connection.h>


namespace Virtio_input {
	using namespace Genode;
	struct Main;
	class Driver;
}


class Virtio_input::Driver
{
	public:

		struct Device_not_found   : Genode::Exception { };
		struct Device_init_failed : Genode::Exception { };
		struct Queue_init_failed  : Genode::Exception { };

	private:

		enum { VENDOR_QEMU = 0x0627 };

		enum Product {
			Any      = 0x0,
			Keyboard = 0x1,
			Mouse    = 0x2,
			Tablet   = 0x3,
		};

		enum Config : uint8_t {
			SelectID    = 0x00,
			SelectSubID = 0x01,
			Data_size   = 0x02,
			Data        = 0x08,
		};

		enum Config_id : uint8_t {
			Name          = 0x01,
			Serial        = 0x02,
			Device_id     = 0x03,
			Prop_bits     = 0x10,
			Event_bits    = 0x11,
			Abs_info      = 0x12,
		};

		struct Device_id {
			uint16_t bus_type = 0;
			uint16_t vendor = 0;
			uint16_t product = 0;
			uint16_t version = 0;
		};

		struct Features : Genode::Register<64> { struct VERSION_1 : Bitfield<32, 1> { }; };

		enum Queue_id : Genode::uint16_t { EVENTS_VQ = 0, STATUS_VQ = 1 };

		struct Abs_config {
			struct {
				uint32_t min;
				uint32_t max;
			} x, y;
			uint32_t width;
			uint32_t height;
		};

		struct Event {
			enum Type {
				Syn = 0x00,
				Key = 0x01,
				Rel = 0x02,
				Abs = 0x03,
				Rep = 0x14,
			};
			enum Code {
				Rel_x       = 0x00,
				Rel_y       = 0x01,
				Rel_wheel   = 0x08,
				Abs_x       = 0x00,
				Abs_y       = 0x01,
			};
			Genode::uint16_t type;
			Genode::uint16_t code;
			Genode::uint32_t value;

			bool operator == (Event const &other) const {
				return other.type == this->type &&
				       other.code == this->code &&
					   other.value == this->value;
			}
		};

		struct Events_queue_traits {
			static const bool device_write_only = true;
			static const bool has_data_payload = false;
		};

		struct Status_queue_traits {
			static const bool device_write_only = false;
			static const bool has_data_payload = false;
		};

		enum { QUEUE_SIZE = 64, QUEUE_ELM_SIZE = sizeof(Event) };

		typedef Virtio::Queue<Event, Events_queue_traits> Events_virtqueue;
		typedef Virtio::Queue<Event, Status_queue_traits> Status_virtqueue;

		Driver(Driver const &);
		Driver &operator = (Driver const &);

		Genode::Env                    &_env;
		Virtio::Connection              _bus;
		Input::Session_component       &_session;
		Virtio::Device_client           _device;
		Event                           _last_sent_key_event { 0, 0, 0 };
		Input::Relative_motion			_rel_motion { 0, 0 };
		Input::Absolute_motion			_abs_motion { -1, -1 };
		Abs_config                      _abs_config;
		Genode::Irq_session_client      _irq { _device.irq() };
		Genode::Signal_handler<Driver>  _irq_handler;
		Events_virtqueue                _events_vq { _env.ram(), _env.rm(), QUEUE_SIZE, QUEUE_ELM_SIZE };
		Status_virtqueue                _status_vq { _env.ram(), _env.rm(), QUEUE_SIZE, QUEUE_ELM_SIZE };


		void _handle_event(const Event &evt)
		{
			switch (evt.type) {

				case Event::Type::Syn:
				{
					if (_rel_motion.x != 0 || _rel_motion.y != 0) {
						_session.submit(_rel_motion);
						_rel_motion = Input::Relative_motion{0, 0};
					}

					if (_abs_motion.x >= 0 || _abs_motion.y >= 0) {
						_session.submit(_abs_motion);
						_abs_motion = Input::Absolute_motion{-1, -1};
					}

					break;
				}

				case Event::Type::Rel:
				{
					switch (evt.code) {
						case Event::Code::Rel_x: _rel_motion.x = evt.value; break;
						case Event::Code::Rel_y: _rel_motion.y = evt.value; break;
						case Event::Code::Rel_wheel:
							_session.submit(Input::Wheel{0, (int)evt.value});
							break;
						default:
							warning("Unhandled relative event code: ", Hex(evt.code));
							break;
					}
					break;
				}

				case Event::Type::Key:
				{
					// Filter out auto-repeat keypress events.
					if (_last_sent_key_event == evt)
						break;

					// It looks like Genode keyboard event codes mirror linux evdev ones.
					Input::Keycode keycode = static_cast<Input::Keycode>(evt.code);

					// Some key events apparently don't send both press and release values.
					// Fake both press and release to make nitpicker happy.
					if ((keycode == Input::BTN_GEAR_UP ||
					     keycode == Input::BTN_GEAR_DOWN) && !evt.value)
						_session.submit(Input::Press{keycode});

					switch (evt.value) {
						case 0: _session.submit(Input::Release{keycode}); break;
						case 1: _session.submit(Input::Press{keycode}); break;
						default:
							warning("Unhandled key event value: ", evt.value);
							break;
					}

					_last_sent_key_event = evt;
					break;
				}

				case Event::Type::Abs:
				{
					switch (evt.code) {
						case Event::Code::Abs_x:
							_abs_motion.x = (_abs_config.width * evt.value / _abs_config.x.max);
							_abs_motion.y = Genode::max(0, _abs_motion.y);
							break;
						case Event::Code::Abs_y:
							_abs_motion.x = Genode::max(0, _abs_motion.x);
							_abs_motion.y = (_abs_config.height * evt.value / _abs_config.y.max);
							break;
						default:
							warning("Unhandled absolute event code: ", Hex(evt.code));
							break;
					}
					break;
				}

				default:
					warning("Unhandled event type: ", Hex(evt.type));
					break;
			}
		}


		static Product _match_product(Genode::Xml_node const &config)
		{
			auto product_string = config.attribute_value("match_product", String<10>("any"));

			if (product_string  == "keyboard") {
				return Product::Keyboard;
			} else if (product_string == "mouse") {
				return Product::Mouse;
			} else if (product_string == "tablet") {
				return Product::Tablet;
			} else if (product_string == "any") {
				return Product::Any;
			}

			error("Invalid product name: ", product_string);

			throw Device_init_failed();
		}


		static size_t _cfg_select(Virtio::Device_client &device, Config_id sel, uint8_t subsel)
		{
			device.write_config(Config::SelectID, Virtio::Device::ACCESS_8BIT, sel);
			device.write_config(Config::SelectSubID, Virtio::Device::ACCESS_8BIT, subsel);
			return device.read_config(Config::Data_size, Virtio::Device::ACCESS_8BIT);
		}


		static Abs_config _read_abs_config(Virtio::Device_client  &device,
		                                   Genode::Xml_node const &config)
		{
			Abs_config cfg {0, ~0U, 0, ~0U, 0, 0};

			auto size = _cfg_select(device, Config_id::Abs_info, Event::Code::Abs_x);
			if (size >= sizeof(cfg.x)) {
				cfg.x.min = device.read_config(Config::Data, Virtio::Device::ACCESS_32BIT);
				cfg.x.max = device.read_config(Config::Data + 4, Virtio::Device::ACCESS_32BIT);
			}

			size = _cfg_select(device, Config_id::Abs_info, Event::Code::Abs_y);
			if (size >= sizeof(cfg.y)) {
				cfg.y.min = device.read_config(Config::Data, Virtio::Device::ACCESS_32BIT);
				cfg.y.max = device.read_config(Config::Data + 4, Virtio::Device::ACCESS_32BIT);
			}

			cfg.width = config.attribute_value("width", cfg.x.max);
			cfg.height = config.attribute_value("height", cfg.y.max);

			return cfg;
		}


		static struct Device_id _read_device_id(Virtio::Device_client &device)
		{
			struct Device_id device_id;
			auto size = _cfg_select(device, Config_id::Device_id, 0);

			if (size != sizeof(device_id)) {
				error("Invalid VirtIO input device ID size!");
				throw Device_init_failed();
			}

			device_id.bus_type = device.read_config(Config::Data + 0, Virtio::Device::ACCESS_16BIT);
			device_id.vendor   = device.read_config(Config::Data + 2, Virtio::Device::ACCESS_16BIT);
			device_id.product  = device.read_config(Config::Data + 4, Virtio::Device::ACCESS_16BIT);
			device_id.version  = device.read_config(Config::Data + 6, Virtio::Device::ACCESS_16BIT);

			return device_id;
		}


		template <Genode::size_t SZ>
		static String<SZ> _read_device_name(Virtio::Device_client &device)
		{
			auto size = _cfg_select(device, Config_id::Name, 0);
			size = Genode::min(size, SZ);
			
			char buf[SZ];
			memset(buf, 0, sizeof(buf));

			for (unsigned i = 0; i < size; ++i)
				buf[i] = device.read_config(Config::Data + i, Virtio::Device::ACCESS_8BIT);

			return String<SZ>(buf);
		}


		static bool _probe_device(Virtio::Device_client &device, Product match_product)
		{
			using Status = Virtio::Device::Status;

			if (!device.set_status(Status::RESET)) {
				warning("Failed to reset the device!");
				return false;
			}

			if (!device.set_status(Status::ACKNOWLEDGE)) {
				warning("Failed to acknowledge the device!");
				return false;
			}

			const auto dev_id = _read_device_id(device);
			if (dev_id.vendor != VENDOR_QEMU) {
				warning("Unsupported VirtIO input device vendor: ", Hex(dev_id.vendor));
			}

			if (match_product != Product::Any && match_product != dev_id.product)
				return false;

			return true;
		}


		static bool _init_features(Virtio::Device_client &device)
		{
			using Status = Virtio::Device::Status;

			const uint32_t low = device.get_features(0);
			const uint32_t high = device.get_features(1);
			const Features::access_t device_features = ((uint64_t)high << 32) | low;

			Features::access_t driver_features = 0;

			if (!Features::VERSION_1::get(device_features)) {
				warning("Unsupprted VirtIO device version!");
				return false;
			}
			Features::VERSION_1::set(driver_features);

			device.set_features(0, (uint32_t)driver_features);
			device.set_features(1, (uint32_t)(driver_features >> 32));

			if (!device.set_status(Status::FEATURES_OK)) {
				device.set_status(Status::FAILED);
				error("Device feature negotiation failed!");
				return false;
			}

			return true;
		}


		Virtio::Device_client _init_device(Genode::Xml_node const &config)
		{
			using Status = Virtio::Device::Status;

			const auto product = _match_product(config);

			Virtio::Device_capability device_cap;

			_bus.with_upgrade([&] () {
				device_cap = _bus.first_device(Virtio::Device_type::INPUT);
			});

			while (device_cap.valid()) {
				{
					Virtio::Device_client device(device_cap);

					if (_probe_device(device, product) && _init_features(device)) {

						if (!device.set_status(Status::DRIVER)) {
							device.set_status(Status::FAILED);
							warning("Device initialization failed!");
							throw Device_init_failed();
						}

						return device;
					} else if (!device.set_status(Status::RESET)) {
						warning("Failed to reset the device!");
					}
				}

				auto prev_device_cap = device_cap;
				_bus.with_upgrade([&] () { device_cap = _bus.next_device(device_cap); });
				_bus.release_device(prev_device_cap);
			}

			warning("No suitable VirtIO input device found!");
			throw Device_not_found();
		}


		void _setup_virtio_queues()
		{
			if (!_device.configure_queue(EVENTS_VQ, _events_vq.description())) {
				error("Failed to initialize events VirtIO queue!");
				throw Queue_init_failed();
			}

			if (!_device.configure_queue(STATUS_VQ, _status_vq.description())) {
				error("Failed to initialize status VirtIO queue!");
				throw Queue_init_failed();
			}

			using Status = Virtio::Device::Status;
			if (!_device.set_status(Status::DRIVER_OK)) {
				_device.set_status(Status::FAILED);
				error("Failed to initialize VirtIO queues!");
				throw Queue_init_failed();
			}
		}


		void _handle_irq()
		{
			enum { IRQ_USED_RING_UPDATE = 1, IRQ_CONFIG_CHANGE = 2 };

			const uint32_t reasons = _device.read_isr();

			if (reasons & IRQ_USED_RING_UPDATE) {
				while (_events_vq.has_used_buffers())
					_handle_event(_events_vq.read_data());

				/* Reclaim all buffers processed by the device. */
				if (_status_vq.has_used_buffers())
					_status_vq.ack_all_transfers();
			}

			_irq.ack_irq();
		}


	public:

		Driver(Genode::Env              &env,
		       Input::Session_component &session,
		       const Genode::Xml_node   &config)
		: _env(env), _bus(env), _session(session),
		  _device(_init_device(config)),
		  _abs_config(_read_abs_config(_device, config)),
		  _irq_handler(env.ep(), *this, &Driver::_handle_irq)
		{
			_setup_virtio_queues();
			_irq.sigh(_irq_handler);
			_irq.ack_irq();
			log("Using \"", _read_device_name<32>(_device), "\" device.");
		}

};


struct Virtio_input::Main
{
	Genode::Env              &env;
	Genode::Heap              heap { env.ram(), env.rm() };
	Input::Session_component  session { env, env.ram() };
	Input::Root_component     root { env.ep().rpc_ep(), session };
	Attached_rom_dataspace    config { env, "config" };
	Virtio_input::Driver      driver { env, session, config.xml() };

	Main(Genode::Env &env)
	try : env(env)
	{
		env.parent().announce(env.ep().manage(root));
	}
	catch (...)
	{
		env.parent().exit(-1);
	}
};


void Component::construct(Genode::Env &env)
{
	static Virtio_input::Main server(env);
}
