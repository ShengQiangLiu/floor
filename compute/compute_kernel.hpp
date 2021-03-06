/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2017 Florian Ziesche
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

#ifndef __FLOOR_COMPUTE_KERNEL_HPP__
#define __FLOOR_COMPUTE_KERNEL_HPP__

#include <string>
#include <vector>
#include <floor/math/vector_lib.hpp>
#include <floor/compute/compute_common.hpp>
#include <floor/compute/compute_buffer.hpp>
#include <floor/compute/compute_image.hpp>
#include <floor/compute/llvm_toolchain.hpp>
#include <floor/core/flat_map.hpp>
#include <floor/threading/atomic_spin_lock.hpp>

FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(weak-vtables)

class compute_kernel {
public:
	virtual ~compute_kernel() = 0;
	
	struct kernel_entry {
		const llvm_toolchain::function_info* info { nullptr };
		uint32_t max_total_local_size { 0u };
		uint3 max_local_size;
	};
	//! returns the internal kernel entry for the specified device
	virtual const kernel_entry* get_kernel_entry(shared_ptr<compute_device> dev) const = 0;
	
	//! don't call this directly, call the execute function in a compute_queue object instead!
	template <typename... Args, class work_size_type_global, class work_size_type_local>
	void execute(compute_queue* queue_ptr,
				 work_size_type_global&& global_work_size,
				 work_size_type_local&& local_work_size,
				 Args&&... args);
	
protected:
	//! same as the one in compute_context, but this way we don't need access to that object
	virtual COMPUTE_TYPE get_compute_type() const = 0;
	
	atomic_spin_lock warn_map_lock;
	//! used to prevent console/log spam by remembering if a warning/error has already been printed for a kernel
	flat_map<const kernel_entry*, bool> warn_map GUARDED_BY(warn_map_lock);
	
	//! checks the specified local work size against the max local work size in the kernel_entry,
	//! and will compute a proper local work size if the specified one is invalid
	//! NOTE: will only warn/error once per kernel per device
	uint3 check_local_work_size(const kernel_entry& entry,
								const uint3& local_work_size) REQUIRES(!warn_map_lock);
	
};

#include <floor/compute/cuda/cuda_kernel.hpp>
#include <floor/compute/host/host_kernel.hpp>
#include <floor/compute/metal/metal_kernel.hpp>
#include <floor/compute/opencl/opencl_kernel.hpp>
#include <floor/compute/vulkan/vulkan_kernel.hpp>

#if (!defined(FLOOR_CUDA_KERNEL_IMPL) && \
	 !defined(FLOOR_HOST_KERNEL_IMPL) && \
	 !defined(FLOOR_OPENCL_KERNEL_IMPL) && \
	 !defined(FLOOR_METAL_KERNEL_IMPL) && \
	 !defined(FLOOR_VULKAN_KERNEL_IMPL))
// forwarder to the actual kernel classes (disabled when included by them)
template <typename... Args, class work_size_type_global, class work_size_type_local>
void compute_kernel::execute(compute_queue* queue_ptr,
							 work_size_type_global&& global_work_size,
							 work_size_type_local&& local_work_size,
							 Args&&... args) {
	// get around the nightmare of the non-existence of virtual (variadic) template member functions ...
	switch(get_compute_type()) {
		case COMPUTE_TYPE::CUDA:
#if !defined(FLOOR_NO_CUDA)
			static_cast<cuda_kernel*>(this)->execute(queue_ptr,
													 decay_t<work_size_type_global>::dim(),
													 uint3 { global_work_size },
													 uint3 { local_work_size },
													 forward<Args>(args)...);
#endif // else: nop
			break;
		case COMPUTE_TYPE::HOST:
#if !defined(FLOOR_NO_HOST_COMPUTE)
			static_cast<host_kernel*>(this)->execute(queue_ptr,
													 decay_t<work_size_type_global>::dim(),
													 uint3 { global_work_size },
													 uint3 { local_work_size },
													 forward<Args>(args)...);
#endif // else: nop
			break;
		case COMPUTE_TYPE::METAL:
#if !defined(FLOOR_NO_METAL)
			static_cast<metal_kernel*>(this)->execute(queue_ptr,
													  decay_t<work_size_type_global>::dim(),
													  uint3 { global_work_size },
													  uint3 { local_work_size },
													  forward<Args>(args)...);
#endif // else: nop
			break;
		case COMPUTE_TYPE::OPENCL:
#if !defined(FLOOR_NO_OPENCL)
			static_cast<opencl_kernel*>(this)->execute(queue_ptr,
													   decay_t<work_size_type_global>::dim(),
													   uint3 { global_work_size },
													   uint3 { local_work_size },
													   forward<Args>(args)...);
#endif // else: nop
			break;
		case COMPUTE_TYPE::VULKAN:
#if !defined(FLOOR_NO_VULKAN)
			static_cast<vulkan_kernel*>(this)->execute(queue_ptr,
													   decay_t<work_size_type_global>::dim(),
													   uint3 { global_work_size },
													   uint3 { local_work_size },
													   forward<Args>(args)...);
#endif // else: nop
			break;
		default: break;
	}
}
#endif

FLOOR_POP_WARNINGS()

#endif
