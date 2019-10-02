/*
 * \brief  VirtIO MMIO bus
 * \author Piotr Tworek
 * \date   2019-09-27
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#pragma once

#include <base/allocator.h>
#include <irq_session/connection.h>
#include <util/list.h>
#include <virtio_session/virtio_session.h>

namespace Virtio_mmio
{
	struct Device_description : Genode::List<Device_description>::Element
	{
		Device_description(Genode::addr_t                base_,
		                   Genode::size_t                size_,
		                   Genode::uint32_t              irq_number,
		                   Virtio::Device_type           type_,
		                   Genode::Irq_session::Trigger  trigger,
		                   Genode::Irq_session::Polarity polarity)
		    : base(base_), size(size_), irq(irq_number), type(type_),
		      irq_trigger(trigger), irq_polarity(polarity)
		{ }

		Genode::addr_t                const base;
		Genode::size_t                const size;
		Genode::uint32_t              const irq;
		Virtio::Device_type           const type;
		Genode::Irq_session::Trigger  const irq_trigger;
		Genode::Irq_session::Polarity const irq_polarity;
		bool                                claimed = false;

		void print(Genode::Output &output) const;
	};

	namespace Bus
	{
		typedef Genode::List<Device_description> Device_list;

		Genode::size_t scan(Genode::Env       &env,
		                    Genode::Allocator &md_alloc,
		                    Device_list       &out_list);
	};
}
