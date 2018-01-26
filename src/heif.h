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

#ifndef LIBHEIF_HEIF_H
#define LIBHEIF_HEIF_H


#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#include "heif-version.h"

#if defined(_MSC_VER) && !defined(LIBHEIF_STATIC_BUILD)
  #ifdef LIBHEIF_EXPORTS
  #define LIBHEIF_API __declspec(dllexport)
  #else
  #define LIBHEIF_API __declspec(dllimport)
  #endif
#elif HAVE_VISIBILITY
  #ifdef LIBHEIF_EXPORTS
  #define LIBHEIF_API __attribute__((__visibility__("default")))
  #else
  #define LIBHEIF_API
  #endif
#else
  #define LIBHEIF_API
#endif

/* === version numbers === */

// Version string of linked libde265 library.
LIBHEIF_API const char *heif_get_version(void);
// Numeric version of linked libde265 library, encoded as 0xHHMMLL00 = HH.MM.LL.
LIBHEIF_API uint32_t heif_get_version_number(void);

// Numeric part "HH" from above.
LIBHEIF_API int heif_get_version_number_major(void);
// Numeric part "MM" from above.
LIBHEIF_API int heif_get_version_number_minor(void);
// Numeric part "LL" from above.
LIBHEIF_API int heif_get_version_number_maintenance(void);

// Helper macros to check for given versions of libde265 at compile time.
#define LIBHEIF_ENCODED_VERSION(h, m, l) ((h) << 24 | (m) << 16 | (l) << 8)
#define LIBHEIF_CHECK_VERSION(h, m, l) (LIBHEIF_NUMERIC_VERSION >= LIBHEIF_ENCODED_VERSION(h, m, l))



enum heif_error_code {
  // Everything ok, no error occurred.
  heif_error_Ok = 0,

  // Input file does not exist.
  heif_error_Input_does_not_exist = 1,

  // Error in input file. Corrupted or invalid content.
  heif_error_Invalid_input = 2,

  // Input file type is not supported.
  heif_error_Unsupported_filetype = 3,

  // Image requires an unsupported decoder feature.
  heif_error_Unsupported_feature = 4,

  // Library API has been used in an invalid way.
  heif_error_Usage_error = 5,

  // Could not allocate enough memory.
  heif_error_Memory_allocation_error = 6,

  // The decoder plugin generated an error
  heif_error_Decoder_plugin_error = 7
};


enum heif_suberror_code {
  // no further information available
  heif_suberror_Unspecified = 0,

  // --- Invalid_input ---

  // End of data reached unexpectedly.
  heif_suberror_End_of_data = 100,

  // Size of box (defined in header) is wrong
  heif_suberror_Invalid_box_size = 101,

  // Mandatory 'ftyp' box is missing
  heif_suberror_No_ftyp_box = 102,

  heif_suberror_No_idat_box = 103,

  heif_suberror_No_meta_box = 104,

  heif_suberror_No_hdlr_box = 105,

  heif_suberror_No_hvcC_box = 106,

  heif_suberror_No_pitm_box = 107,

  heif_suberror_No_ipco_box = 108,

  heif_suberror_No_ipma_box = 109,

  heif_suberror_No_iloc_box = 110,

  heif_suberror_No_iinf_box = 111,

  heif_suberror_No_iprp_box = 112,

  heif_suberror_No_iref_box = 113,

  heif_suberror_No_pict_handler = 114,

  // An item property referenced in the 'ipma' box is not existing in the 'ipco' container.
  heif_suberror_Ipma_box_references_nonexisting_property = 115,

  // No properties have been assigned to an item.
  heif_suberror_No_properties_assigned_to_item = 116,

  // Image has no (compressed) data
  heif_suberror_No_item_data = 117,

  // Invalid specification of image grid (tiled image)
  heif_suberror_Invalid_grid_data = 118,

  // Tile-images in a grid image are missing
  heif_suberror_Missing_grid_images = 119,

  heif_suberror_Invalid_clean_aperture = 120,

  // Invalid specification of overlay image
  heif_suberror_Invalid_overlay_data = 121,

  // Overlay image completely outside of visible canvas area
  heif_suberror_Overlay_image_outside_of_canvas = 122,

  heif_suberror_Auxiliary_image_type_unspecified = 123,



  // --- Memory_allocation_error ---

  // A security limit preventing unreasonable memory allocations was exceeded by the input file.
  // Please check whether the file is valid. If it is, contact us so that we could increase the
  // security limits further.
  heif_suberror_Security_limit_exceeded = 200,


  // --- Usage_error ---

  // An image ID was used that is not present in the file.
  heif_suberror_Nonexisting_image_referenced = 300, // also used for Invalid_input

  // An API argument was given a NULL pointer, which is not allowed for that function.
  heif_suberror_Null_pointer_argument = 301,

  // Image channel referenced that does not exist in the image
  heif_suberror_Nonexisting_image_channel_referenced = 302,


  // --- Unsupported_feature ---

  // Image was coded with an unsupported compression method.
  heif_suberror_Unsupported_codec = 400,

  // Image is specified in an unknown way, e.g. as tiled grid image (which is supported)
  heif_suberror_Unsupported_image_type = 401,

  heif_suberror_Unsupported_data_version = 402
};



struct heif_error
{
  // main error category
  enum heif_error_code code;

  // more detailed error code
  enum heif_suberror_code subcode;

  // textual error message (is always defined, you do not have to check for NULL)
  const char* message;
};




typedef void* heif_handle;
typedef void* image_handle;



typedef struct _yuv_image_data_
{
  int width;
  int height;
  int bit_depth;
  int chroma;
  int colorspace;
  uint8_t *yuv_data;
}yuv_image;

typedef struct heif_image_info_
{
  int width;
  int height;
  int bit_depth;
  int chroma;
  int colorspace;


  int codec_type;

  bool is_primary;
  bool is_thumbnail;

}heif_image_info;

typedef struct heif_image_data_
{
  heif_image_info info;

  //
  int tiles_count;
  int tile_rows;
  int tile_columns;
  int *arr_tile_data_len;
  uint8_t **arr_tile_data;
  heif_image_info *tile_info;

  //
  int thumb_count;
  uint8_t **arr_thumb_data;
  heif_image_info *thumb_info;

  // default YUV420
  uint8_t *heif_yuv_data;
  
}heif_image;

// Allocate a new context for reading HEIF files.
// Has to be freed again with heif_context_free().
LIBHEIF_API
heif_handle heif_hendle_alloc(void);

// Free a previously allocated HEIF context. You should not free a context twice.
LIBHEIF_API
void heif_handle_free(heif_handle h);

// Read a HEIF file from a named disk file.
LIBHEIF_API
struct heif_error heif_read_from_file(heif_handle h, const char* filename);

// Read a HEIF file stored completely in memory.
LIBHEIF_API
struct heif_error heif_read_from_memory(heif_handle h, const void* mem, size_t size);

// Get a handle to the primary image of the HEIF file.
// This is the image that should be displayed primarily when there are several images in the file.
LIBHEIF_API
int heif_get_primary_image_index(heif_handle h);

// Number of top-level image in the HEIF file. This does not include the thumbnails or the
// tile images that are composed to an image grid. You can get access to the thumbnails via
// the main image handle.
LIBHEIF_API
int heif_get_number_of_images(heif_handle h);

#if 0
LIBHEIF_API
heif_error heif_get_image_handle(heif_handle h, int image_idx, image_handle* img_handle);
#endif

LIBHEIF_API
int heif_get_image_tiles_count(heif_handle h, int image_idx);

LIBHEIF_API
heif_error heif_get_image_tiles_info(heif_handle h, int image_idx);

//LIBHEIF_API
//heif_error heif_get_image_tiles_compressed_data(heif_handle h, int image_idx, int tile_idx, std::vector<uint8_t>* out_data);


LIBHEIF_API
heif_error heif_get_image_compressed_data(heif_handle h, int image_idx, heif_image* out_data);






LIBHEIF_API
void heif_debug_dump_box(heif_handle h);

LIBHEIF_API
void heif_debug_dump_image_info(heif_handle h, int image_idx);




// --- heif_image

// Note: when converting images to colorspace_RGB/chroma_interleaved_24bit, the resulting
// image contains only a single channel of type channel_interleaved with 3 bytes per pixel,
// containing the interleaved R,G,B values.

enum heif_compression_format {
  heif_compression_undefined = 0,
  heif_compression_HEVC = 1,
  heif_compression_AVC = 2,
  heif_compression_JPEG = 3
};


enum heif_chroma {
  heif_chroma_undefined=99,
  heif_chroma_mono=0,
  heif_chroma_420=1,
  heif_chroma_422=2,
  heif_chroma_444=3,
  heif_chroma_interleaved_24bit=10
};

enum heif_colorspace {
  heif_colorspace_undefined=99,
  heif_colorspace_YCbCr=0,
  heif_colorspace_RGB  =1,
  heif_colorspace_monochrome=2
};

enum heif_channel {
  heif_channel_Y = 0,
  heif_channel_Cb = 1,
  heif_channel_Cr = 2,
  heif_channel_R = 3,
  heif_channel_G = 4,
  heif_channel_B = 5,
  heif_channel_Alpha = 6,
  heif_channel_interleaved = 10
};








#ifdef __cplusplus
}
#endif

#endif  // LIBHEIF_HEIF_H
