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

#include <floor/compute/host/host_image.hpp>

#if !defined(FLOOR_NO_HOST_COMPUTE)

#include <floor/core/logger.hpp>
#include <floor/compute/host/host_queue.hpp>
#include <floor/compute/host/host_device.hpp>
#include <floor/compute/host/host_compute.hpp>

#if defined(FLOOR_DEBUG)
static constexpr const size_t protection_size { 1024u };
static constexpr const uint8_t protection_byte { 0xA5 };
#else
static constexpr const size_t protection_size { 0u };
#endif

host_image::host_image(const host_device* device,
					   const uint4 image_dim_,
					   const COMPUTE_IMAGE_TYPE image_type_,
					   void* host_ptr_,
					   const COMPUTE_MEMORY_FLAG flags_,
					   const uint32_t opengl_type_,
					   const uint32_t external_gl_object_,
					   const opengl_image_info* gl_image_info) :
compute_image(device, image_dim_, image_type_, host_ptr_, flags_,
			  opengl_type_, external_gl_object_, gl_image_info) {
	// actually create the image
	if(!create_internal(true, ((host_compute*)device->context)->get_main_queue())) {
		return; // can't do much else
	}
}

bool host_image::create_internal(const bool copy_host_data, shared_ptr<compute_queue> cqueue) {
	image = new uint8_t[image_data_size_mip_maps + protection_size] alignas(1024);
	program_info.buffer = image;
	program_info.runtime_image_type = image_type;
	
	const auto dim_count = image_dim_count(image_type);
	uint4 mip_image_dim {
		image_dim.x,
		dim_count >= 2 ? image_dim.y : 0,
		dim_count >= 3 ? image_dim.z : 0,
		0
	};
	uint32_t level_offset = 0;
	for(size_t level = 0; level < host_limits::max_mip_levels; ++level, mip_image_dim >>= 1) {
		const auto slice_data_size = image_slice_data_size_from_types(mip_image_dim, image_type, 1);
		const auto level_data_size = slice_data_size * layer_count;
		program_info.level_info[level].offset = level_offset;
		level_offset += level_data_size;
		
		program_info.level_info[level].dim = mip_image_dim;
		program_info.level_info[level].clamp_dim_int = {
			mip_image_dim.x > 0 ? int(mip_image_dim.x - 1) : 0,
			mip_image_dim.y > 0 ? int(mip_image_dim.y - 1) : 0,
			mip_image_dim.z > 0 ? int(mip_image_dim.z - 1) : 0,
			0
		};
		program_info.level_info[level].clamp_dim_float = {
			mip_image_dim.x > 0 ? float(mip_image_dim.x) : 0.0f,
			mip_image_dim.y > 0 ? float(mip_image_dim.y) : 0.0f,
			mip_image_dim.z > 0 ? float(mip_image_dim.z) : 0.0f,
			0.0f
		};
	}
	
#if defined(FLOOR_DEBUG)
	// set protection bytes
	memset(image + image_data_size_mip_maps, protection_byte, protection_size);
#endif
	
	// -> normal host image
	if(!has_flag<COMPUTE_MEMORY_FLAG::OPENGL_SHARING>(flags)) {
		// copy host memory to "device" if it is non-null and NO_INITIAL_COPY is not specified
		if(copy_host_data &&
		   host_ptr != nullptr &&
		   !has_flag<COMPUTE_MEMORY_FLAG::NO_INITIAL_COPY>(flags)) {
			memcpy(image, host_ptr,
				   // if mip-maps have to be created on the libfloor side (i.e. not provided by the user),
				   // only copy the data that is actually provided by the user
				   generate_mip_maps ? image_data_size : image_data_size_mip_maps);
			
			// manually create mip-map chain
			if(generate_mip_maps) {
				generate_mip_map_chain(cqueue);
			}
		}
	}
	// -> shared host/opengl image
	else {
#if !defined(FLOOR_IOS)
		if(!create_gl_image(copy_host_data)) return false;
#endif
		
		// acquire for use with the host
		acquire_opengl_object(cqueue);
	}
	
	return true;
}

host_image::~host_image() {
	// first, release and kill the opengl image
	if(gl_object != 0) {
		if(gl_object_state) {
			log_warn("image still registered for opengl use - acquire before destructing a compute image!");
		}
		if(!gl_object_state) release_opengl_object(nullptr); // -> release to opengl
#if !defined(FLOOR_IOS)
		delete_gl_image();
#endif
	}
	// then, also kill the host image
	if(image != nullptr) {
		delete [] image;
	}
}

void host_image::zero(shared_ptr<compute_queue> cqueue) {
	if(image == nullptr) return;
	
	cqueue->finish();
	memset(image, 0, image_data_size_mip_maps);
}

void* __attribute__((aligned(128))) host_image::map(shared_ptr<compute_queue> cqueue,
													const COMPUTE_MEMORY_MAP_FLAG flags_) {
	if(image == nullptr) return nullptr;
	
	const bool blocking_map = has_flag<COMPUTE_MEMORY_MAP_FLAG::BLOCK>(flags_);
	if(blocking_map) {
		cqueue->finish();
	}
	return image;
}

void host_image::unmap(shared_ptr<compute_queue> cqueue, void* __attribute__((aligned(128))) mapped_ptr) {
	if(image == nullptr) return;
	if(mapped_ptr == nullptr) return;
	
	// manually create mip-map chain
	if(generate_mip_maps) {
		generate_mip_map_chain(cqueue);
	}
}

bool host_image::acquire_opengl_object(shared_ptr<compute_queue> cqueue floor_unused) {
#if !defined(FLOOR_IOS)
	if(gl_object == 0) return false;
	if(!gl_object_state) {
#if defined(FLOOR_DEBUG) && 0
		log_warn("opengl image has already been acquired for use with the host!");
#endif
		return true;
	}
	
	// copy gl image data to host memory (if read access is set)
	const auto dim_count = image_dim_count(image_type);
	const auto is_cube = has_flag<COMPUTE_IMAGE_TYPE::FLAG_CUBE>(image_type);
	const auto is_array = has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type);
	int4 mip_image_dim {
		(int)image_dim.x,
		dim_count >= 2 ? (int)image_dim.y : 0,
		dim_count >= 3 ? (int)image_dim.z : 0,
		0
	};
	if(has_flag<COMPUTE_MEMORY_FLAG::READ>(flags)) {
		glBindTexture(opengl_type, gl_object);
		const uint8_t* level_data = image;
		for(size_t level = 0; level < mip_level_count; ++level, mip_image_dim >>= 1) {
			const auto slice_data_size = image_slice_data_size_from_types(mip_image_dim, image_type, 1);
			const auto level_data_size = slice_data_size * layer_count;
			
			if(!is_cube ||
			   // contrary to GL_TEXTURE_CUBE_MAP, GL_TEXTURE_CUBE_MAP_ARRAY can be copied directly
			   (is_cube && is_array)) {
				glGetTexImage(opengl_type, (int)level, gl_format, gl_type, (void*)level_data);
			}
			else {
				// must copy cube faces individually
				for(uint32_t i = 0; i < 6; ++i) {
					glGetTexImage(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, (int)level, gl_format, gl_type, (void*)level_data);
					level_data += slice_data_size;
				}
			}
			
			level_data += level_data_size;
		}
		glBindTexture(opengl_type, 0);
		
#if defined(FLOOR_DEBUG)
		// check memory protection strip
		for(size_t i = 0; i < protection_size; ++i) {
			if(*(image + image_data_size_mip_maps + i) != protection_byte) {
				log_error("DO PANIC: opengl wrote too many bytes: image: %X, gl object: %u, @ protection byte #%u, expected data size %u",
						  this, gl_object, i, image_data_size_mip_maps);
				logger::flush();
				break;
			}
		}
#endif
	}
	
	gl_object_state = false;
	return true;
#else
	log_error("this is not supported in iOS!");
	return false;
#endif
}

bool host_image::release_opengl_object(shared_ptr<compute_queue> cqueue floor_unused) {
#if !defined(FLOOR_IOS)
	if(gl_object == 0) return false;
	if(image == nullptr) return false;
	if(gl_object_state) {
#if defined(FLOOR_DEBUG) && 0
		log_warn("opengl image has already been released for opengl use!");
#endif
		return true;
	}
	
	// copy the host data back to the gl buffer (if write access is set)
	if(has_flag<COMPUTE_MEMORY_FLAG::WRITE>(flags)) {
		glBindTexture(opengl_type, gl_object);
		update_gl_image_data(image);
		glBindTexture(opengl_type, 0);
	}
	
	gl_object_state = true;
	return true;
#else
	log_error("this is not supported in iOS!");
	return false;
#endif
}

#endif
