/*
 * \brief  Platform driver for qemu arm virt machine.
 * \author Piotr Tworek <tworaz@tworaz.net>
 * \date   2019-09-18
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/attached_io_mem_dataspace.h>
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/heap.h>
#include <base/log.h>
#include <base/session_label.h>
#include <rom_session/rom_session.h>
#include <root/component.h>

#include <dtb_flash_defs.h>

namespace Platform {
	using namespace Genode;
	using namespace Qemu_virt;
	class Root;
	class Dtb_rom_session_component;
	class Flash;
	struct Main;
}


namespace {
	using namespace Genode;

	inline uint8_t extract_byte(uint32_t x, size_t n)
	{
		return ((uint8_t *)&x)[n];
	}

	inline uint32_t fdt32_to_cpu(uint32_t x)
	{
		return ((extract_byte(x, 0) << 24) |
		        (extract_byte(x, 1) << 16) |
		        (extract_byte(x, 2) << 8)  |
		         extract_byte(x, 3));
	}
}


class Platform::Flash : private Attached_io_mem_dataspace
{
	private:

		struct Dtb_magic_invalid : public Exception { };

		/*
		 * Noncopyable
		 */
		Flash(Flash const &);
		Flash &operator = (Flash const &);

		struct Header
		{
			enum { MAGIC = 0xD00DFEED };
			uint32_t magic;
			uint32_t totalsize;
		};

	public:

		const char* dtb_data;
		const size_t dtb_size;

		Flash(Env &env)
		: Attached_io_mem_dataspace(env, DTB_FLASH_BASE, DTB_FLASH_SIZE)
		, dtb_data(local_addr<char>())
		, dtb_size(fdt32_to_cpu(local_addr<Header>()->totalsize))
		{
			if (fdt32_to_cpu(local_addr<Header>()->magic) != Header::MAGIC) {
				error("Invalid DTB magic number!");
				throw Dtb_magic_invalid();
			}
		}
};


class Platform::Dtb_rom_session_component : public Rpc_object<Rom_session>
{
	private:

		/*
		 * Noncopyable
		 */
		Dtb_rom_session_component(Dtb_rom_session_component const &);
		Dtb_rom_session_component &operator = (Dtb_rom_session_component const &);

		const Dataspace_capability &_dtb_ds;

	public:

		Dtb_rom_session_component(const Dataspace_capability &ds) : _dtb_ds(ds)
		{ }

		Rom_dataspace_capability dataspace() override
		{
			return static_cap_cast<Rom_dataspace>(_dtb_ds);
		}

		void sigh(Signal_context_capability) override { }
};


class Platform::Root : public Root_component<Dtb_rom_session_component>
{
	private:

		/*
		 * Noncopyable
		 */
		Root(Root const &);
		Root &operator = (Root const &);

		Env                      &_env;
		Ram_dataspace_capability _dtb_ds;

		Dtb_rom_session_component *_create_session(const char *args) override
		{
			Session_label const label = label_from_args(args);
			Session_label const module_name = label.last_element();

			if (module_name != "platform.dtb")
				throw Service_denied();

			return new (md_alloc()) Dtb_rom_session_component(_dtb_ds);
		}

		static Ram_dataspace_capability _init_dtb_ds(Ram_allocator &ram,
		                                             Region_map &rm,
		                                             Flash const &flash)
		{
			Ram_dataspace_capability dtb_ds;
			try {
				dtb_ds = ram.alloc(flash.dtb_size);
			} catch (...) {
				error("couldn't allocate memory for dtb binary, empty result");
				return dtb_ds;
			}

			Attached_dataspace ds(rm, dtb_ds);
			memcpy(ds.local_addr<char>(), flash.dtb_data, flash.dtb_size);

			return dtb_ds;
		}

	public:

		Root(Env &env, Allocator &md_alloc)
		: Root_component<Dtb_rom_session_component>(env.ep(), md_alloc),
		  _env(env), _dtb_ds(_init_dtb_ds(_env.ram(), _env.rm(), Flash(_env)))
		{
			if (!_dtb_ds.valid())
				throw Out_of_ram();
		}

		~Root() { _env.ram().free(_dtb_ds); }
};


struct Platform::Main
{
	Env         &_env;
	Sliced_heap  _heap { _env.ram(), _env.rm() };
	Root         _root { _env, _heap };

	Main(Env &env) : _env(env)
	{
		log("--- Qemu virt platform driver ---");
		env.parent().announce(env.ep().manage(_root));
	}
};


void Component::construct(Genode::Env &env)
{
	static Platform::Main platform(env);
}
