/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2016 Florian Ziesche
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

#include <floor/core/essentials.hpp>

#if !defined(FLOOR_NO_VULKAN)
#include <floor/compute/vulkan/vulkan_program.hpp>
#include <floor/compute/vulkan/vulkan_kernel.hpp>
#include <floor/compute/vulkan/vulkan_device.hpp>

vulkan_program::vulkan_program(program_map_type&& programs_) : programs(move(programs_)) {
	if(programs.empty()) return;
	retrieve_unique_kernel_names(programs);
	
	// create all kernels of all device programs
	// note that this essentially reshuffles the program "device -> kernels" data to "kernels -> devices"
	kernels.reserve(kernel_names.size());
	for(const auto& kernel_name : kernel_names) {
		vulkan_kernel::kernel_map_type kernel_map;
		kernel_map.reserve(kernel_names.size());
		
		for(const auto& prog : programs) {
			if(!prog.second.valid) continue;
			
			for(const auto& info : prog.second.kernels_info) {
				if(info.name == kernel_name) {
					vulkan_kernel::vulkan_kernel_entry entry;
					entry.info = &info;
					entry.max_work_group_item_sizes = prog.first->max_work_group_item_sizes;
					
					// retrieve max possible work-group size for this device for this kernel
					// TODO: retrieve this from the binary
					entry.max_local_work_size = prog.first->max_work_group_size;
					
					// TODO: make sure that _all_ of this is synchronized
					
					// create kernel + device specific descriptor set layout
					vector<VkDescriptorSetLayoutBinding> bindings(info.args.size());
					uint32_t buffer_desc = 0, read_image_desc = 0, write_image_desc = 0;
					bool valid_desc = true;
					for(uint32_t i = 0, binding_idx = 0; i < (uint32_t)info.args.size(); ++i, ++binding_idx) {
						bindings[binding_idx].binding = binding_idx;
						bindings[binding_idx].descriptorCount = 1;
						bindings[binding_idx].stageFlags = VK_SHADER_STAGE_ALL;
						bindings[binding_idx].pImmutableSamplers = nullptr; // TODO: use this?
						
						switch(info.args[i].address_space) {
							// buffer
							case llvm_compute::kernel_info::ARG_ADDRESS_SPACE::GLOBAL:
								bindings[binding_idx].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
								++buffer_desc;
								break;
							// image
							case llvm_compute::kernel_info::ARG_ADDRESS_SPACE::IMAGE:
								switch(info.args[i].image_access) {
									case llvm_compute::kernel_info::ARG_IMAGE_ACCESS::READ:
										bindings[binding_idx].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
										++read_image_desc;
										break;
									case llvm_compute::kernel_info::ARG_IMAGE_ACCESS::WRITE:
										bindings[binding_idx].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
										++write_image_desc;
										break;
									case llvm_compute::kernel_info::ARG_IMAGE_ACCESS::READ_WRITE: {
										// need to add both a sampled one and a storage one
										bindings[binding_idx].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
										++binding_idx;
										bindings.emplace(next(begin(bindings), binding_idx),
														 VkDescriptorSetLayoutBinding {
															 .binding = binding_idx,
															 .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
															 .descriptorCount = 1,
															 .stageFlags = VK_SHADER_STAGE_ALL,
															 .pImmutableSamplers = nullptr,
														 });
										++read_image_desc;
										++write_image_desc;
										break;
									}
									case llvm_compute::kernel_info::ARG_IMAGE_ACCESS::NONE:
										log_error("unknown image access type");
										valid_desc = false;
										break;
								}
								break;
							// param
							case llvm_compute::kernel_info::ARG_ADDRESS_SPACE::CONSTANT:
								// TODO/NOTE: for now, this is always a buffer, later on it might make sense to fit as much as possible
								//            into push constants (will require compiler support of course + device specific binary)
								// NOTE: min push constants size is at least 128 bytes
								// alternatively: put it into a ubo
								bindings[binding_idx].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
								++buffer_desc;
								break;
							case llvm_compute::kernel_info::ARG_ADDRESS_SPACE::LOCAL:
								log_error("arg with a local address space is not supported");
								valid_desc = false;
								break;
							case llvm_compute::kernel_info::ARG_ADDRESS_SPACE::UNKNOWN:
								log_error("arg with an unknown address space");
								valid_desc = false;
								break;
						}
						if(!valid_desc) break;
					}
					if(!valid_desc) {
						log_error("invalid descriptor bindings for function \"%s\" for device \"%s\"!", kernel_name, prog.first->name);
						continue;
					}
					
					const VkDescriptorSetLayoutCreateInfo desc_set_layout_info {
						.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
						.pNext = nullptr,
						.flags = 0,
						.bindingCount = (uint32_t)bindings.size(),
						.pBindings = bindings.data(),
					};
					VK_CALL_CONT(vkCreateDescriptorSetLayout(prog.first->device, &desc_set_layout_info, nullptr, &entry.desc_set_layout),
								 "failed to create descriptor set layout");
					// TODO: vkDestroyDescriptorSetLayout cleanup
					
					// create the pipeline layout
					const VkPipelineLayoutCreateInfo pipeline_layout_info {
						.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
						.pNext = nullptr,
						.flags = 0,
						.setLayoutCount = 1,
						.pSetLayouts = &entry.desc_set_layout,
						.pushConstantRangeCount = 0,
						.pPushConstantRanges = nullptr,
					};
					VK_CALL_CONT(vkCreatePipelineLayout(prog.first->device, &pipeline_layout_info, nullptr, &entry.pipeline_layout),
								 "failed to create pipeline layout");
					
					// create the compute pipeline for this kernel + device
					const VkPipelineShaderStageCreateInfo pipeline_stage_info {
						.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
						.pNext = nullptr,
						.flags = 0,
						.stage = VK_SHADER_STAGE_COMPUTE_BIT,
						.module = prog.second.program,
						.pName = kernel_name.c_str(),
						// TODO: use this later on to set dynamic local / work-group sizes
						.pSpecializationInfo = nullptr,
					};
					const VkComputePipelineCreateInfo pipeline_info {
						.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
						.pNext = nullptr,
						.flags = 0,
						.stage = pipeline_stage_info,
						.layout = entry.pipeline_layout,
						.basePipelineHandle = VK_NULL_HANDLE,
						.basePipelineIndex = 0,
					};
					VK_CALL_CONT(vkCreateComputePipelines(prog.first->device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &entry.pipeline),
								 "failed to create compute pipeline");
					
					// create descriptor pool + descriptors
					// TODO: think about how this can be properly handled (creating a pool per kernel per device is probably not a good idea)
					const uint32_t pool_count = ((buffer_desc > 0 ? 1 : 0) +
												 (read_image_desc > 0 ? 1 : 0) +
												 (write_image_desc > 0 ? 1 : 0));
					vector<VkDescriptorPoolSize> pool_sizes(pool_count > 0 ? pool_count : 1); // need at least 1
					uint32_t pool_index = 0;
					if(buffer_desc > 0 || pool_count == 0) {
						pool_sizes[pool_index].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
						pool_sizes[pool_index].descriptorCount = (buffer_desc > 0 ? buffer_desc : 1);
						++pool_index;
					}
					if(read_image_desc > 0) {
						pool_sizes[pool_index].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
						pool_sizes[pool_index].descriptorCount = read_image_desc;
						++pool_index;
					}
					if(write_image_desc > 0) {
						pool_sizes[pool_index].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
						pool_sizes[pool_index].descriptorCount = write_image_desc;
						++pool_index;
					}
					const VkDescriptorPoolCreateInfo desc_pool_info {
						.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
						.pNext = nullptr,
						.flags = 0,
						// we only need one set for now
						.maxSets = 1,
						.poolSizeCount = pool_count,
						.pPoolSizes = pool_sizes.data(),
					};
					VK_CALL_CONT(vkCreateDescriptorPool(prog.first->device, &desc_pool_info, nullptr, &entry.desc_pool),
								 "failed to create descriptor pool");
					
					// allocate descriptor set
					const VkDescriptorSetAllocateInfo desc_set_alloc_info {
						.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
						.pNext = nullptr,
						.descriptorPool = entry.desc_pool,
						.descriptorSetCount = 1,
						.pSetLayouts = &entry.desc_set_layout,
					};
					VK_CALL_CONT(vkAllocateDescriptorSets(prog.first->device, &desc_set_alloc_info, &entry.desc_set), "failed to allocate descriptor set");
					
					// success, insert into map
					kernel_map.insert_or_assign(prog.first, entry);
					break;
				}
			}
		}
		
		kernels.emplace_back(make_shared<vulkan_kernel>(move(kernel_map)));
	}
}

#endif