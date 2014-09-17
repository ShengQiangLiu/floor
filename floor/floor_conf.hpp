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

#ifndef __FLOOR_CONF_HPP__
#define __FLOOR_CONF_HPP__

// if defined, this disables cuda support
//#define FLOOR_NO_CUDA_CL 1

// if defined, this disable opencl support and possibly enabled cuda support
//#define FLOOR_NO_OPENCL 1

// if defined, this disabled openal support
//#define FLOOR_NO_OPENAL 1

// if defined, this will use extern templates for specific template classes (vector*, matrix, etc.)
// and instantiate them for various basic types (float, int, ...)
#define FLOOR_EXPORT 1

#endif