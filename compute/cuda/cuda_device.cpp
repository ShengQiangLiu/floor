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

#include <floor/compute/cuda/cuda_device.hpp>

cuda_device::cuda_device() : compute_device() {
	// init statically known info
	type = compute_device::TYPE::GPU;
	
	vendor = COMPUTE_VENDOR::NVIDIA;
	platform_vendor = COMPUTE_VENDOR::NVIDIA;
	vendor_name = "NVIDIA";
	
	simd_width = 32;
	simd_range = { simd_width, simd_width };
	local_mem_dedicated = true;
	image_support = true;
	double_support = true; // true for all gpus since fermi/sm_20
	basic_64_bit_atomics_support = true; // always true since fermi/sm_20
	sub_group_support = true;
	
#if defined(PLATFORM_X32)
	bitness = 32;
#elif defined(PLATFORM_X64)
	bitness = 64;
#endif
}
