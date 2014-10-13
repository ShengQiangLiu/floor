/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2014 Florian Ziesche
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

#include <floor/compute/cuda/cuda_compute.hpp>

#if !defined(FLOOR_NO_CUDA)

#include <floor/core/gl_support.hpp>

#if defined(__APPLE__)
#include <CUDA/cuda.h>
#include <CUDA/cudaGL.h>
#else
#include <cuda.h>
#include <cudaGL.h>
#endif

void cuda_compute::init(const bool use_platform_devices floor_unused,
						const size_t platform_index floor_unused,
						const bool gl_sharing floor_unused,
						const set<string> device_restriction floor_unused) {
	// TODO: !
}

void cuda_compute::finish() {
	// TODO: !
}

void cuda_compute::flush() {
	// TODO: !
}

void cuda_compute::activate_context() {
	// TODO: !
}

void cuda_compute::deactivate_context() {
	// TODO: !
}

weak_ptr<compute_kernel> cuda_compute::add_kernel_file(const string& file_name floor_unused,
													   const string additional_options floor_unused) {
	// TODO: !
	return {};
}

weak_ptr<compute_kernel> cuda_compute::add_kernel_source(const string& source_code floor_unused,
														 const string additional_options floor_unused) {
	// TODO: !
	return {};
}

void cuda_compute::delete_kernel(weak_ptr<compute_kernel> kernel floor_unused) {
	// TODO: !
}

void cuda_compute::execute_kernel(weak_ptr<compute_kernel> kernel floor_unused) {
	// TODO: !
}

#endif