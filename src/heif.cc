/*
 * HEIF codec.
 * Copyright (c) 2017 struktur AG, Dirk Farin <farin@struktur.de>
 *
 * This file is part of libheif.
 *
 * libheif is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * libheif is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with libheif.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "heif.h"
#include "heif_context.h"
#include "error.h"

#include <memory>
#include <utility>
#include <vector>
#include <assert.h>
#include <iostream>
#include <string.h>

using namespace heif;

struct heif_context
{
  std::shared_ptr<heif::HeifContext> context;
};

struct heif_image_handle
{
  std::shared_ptr<heif::HeifContext::Image> image;
};


const char *heif_get_version(void) {
  return (LIBHEIF_VERSION);
}

uint32_t heif_get_version_number(void) {
  return (LIBHEIF_NUMERIC_VERSION);
}

int heif_get_version_number_major(void) {
  return ((LIBHEIF_NUMERIC_VERSION)>>24) & 0xFF;
}

int heif_get_version_number_minor(void) {
  return ((LIBHEIF_NUMERIC_VERSION)>>16) & 0xFF;
}

int heif_get_version_number_maintenance(void) {
  return ((LIBHEIF_NUMERIC_VERSION)>>8) & 0xFF;
}


// Allocate a new context for reading HEIF files.
// Has to be freed again with heif_context_free().
LIBHEIF_API
heif_handle heif_hendle_alloc(void)
{
  struct heif_context* ctx = new heif_context;
  ctx->context = std::make_shared<HeifContext>();

  return ctx;
}

// Free a previously allocated HEIF context. You should not free a context twice.
LIBHEIF_API
void heif_handle_free(heif_handle h)
{
  struct heif_context* ctx = (struct heif_context*)h;
  delete ctx;
}

// Read a HEIF file from a named disk file.
LIBHEIF_API
struct heif_error heif_read_from_file(heif_handle h, const char* filename)
{
  struct heif_context* ctx = (struct heif_context*)h;

  Error err = ctx->context->read_from_file(filename);
  
  return err.error_struct(ctx->context.get());
}

// Read a HEIF file stored completely in memory.
LIBHEIF_API
struct heif_error heif_read_from_memory(heif_handle h, const void* mem, size_t size)
{
  struct heif_context* ctx = (struct heif_context*)h;

  Error err = ctx->context->read_from_memory(mem, size);

  return err.error_struct(ctx->context.get());

}

// Get a handle to the primary image of the HEIF file.
// This is the image that should be displayed primarily when there are several images in the file.
LIBHEIF_API
int heif_get_primary_image_index(heif_handle h)
{
  struct heif_context* ctx = (struct heif_context*)h;

  return ctx->context->image_id_to_index(ctx->context->get_primary_image()->get_id());

}

// Number of top-level image in the HEIF file. This does not include the thumbnails or the
// tile images that are composed to an image grid. You can get access to the thumbnails via
// the main image handle.
LIBHEIF_API
int heif_get_number_of_images(heif_handle h)
{
  struct heif_context* ctx = (struct heif_context*)h;
 
  return (int)ctx->context->get_top_level_images().size();
}


LIBHEIF_API
heif_error heif_get_image_data(heif_handle h, int image_idx, heif_image* out_data)
{
  struct heif_context* ctx = (struct heif_context*)h;


  ctx->context->get_heif_image_data(ctx->context->image_index_to_id(image_idx), out_data);

  return Error::Ok.error_struct(ctx->context.get());
  
}


LIBHEIF_API
heif_image *heif_create_image_buffer(heif_handle h)
{
  struct heif_context* ctx = (struct heif_context*)h;

  return ctx->context->create_heif_image_buffer();
}

LIBHEIF_API
void heif_destory_image_buffer(heif_handle h,heif_image *img)
{
  struct heif_context* ctx = (struct heif_context*)h;

  ctx->context->destory_heif_image_buffer(img);

}
















LIBHEIF_API
void heif_debug_dump_box(heif_handle h)
{
  struct heif_context* ctx = (struct heif_context*)h;

  std::cout << ctx->context->debug_dump_boxes() << std::endl;

  return ;
}




