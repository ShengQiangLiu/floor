/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2015 Florian Ziesche
 *  
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License only.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __FLOOR_HOST_DEVICE_HPP__
#define __FLOOR_HOST_DEVICE_HPP__

#include <floor/compute/compute_device.hpp>
#include <floor/core/core.hpp>
#include <floor/floor/floor_version.hpp>

FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(weak-vtables)

class host_device final : public compute_device {
public:
	static constexpr const uint32_t host_compute_local_memory_size { 128ull * 1024ull * 1024ull };
	
	host_device();
	~host_device() override {}
	
#if !defined(FLOOR_NO_HOST_COMPUTE)
#endif
	
};

FLOOR_POP_WARNINGS()

#endif
