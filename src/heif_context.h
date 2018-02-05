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

#ifndef LIBHEIF_HEIF_CONTEXT_H
#define LIBHEIF_HEIF_CONTEXT_H

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "error.h"

#include "heif_file.h"
#include "heif.h"

namespace heif {


  class ImageMetadata
  {
  public:
    std::string item_type;  // e.g. "Exif"
    std::vector<uint8_t> m_data;
  };


  // This is a higher-level view than HeifFile.
  // Images are grouped logically into main images and their thumbnails.
  // The class also handles automatic color-space conversion.
  class HeifContext : public ErrorBuffer {
  public:
    HeifContext();
    ~HeifContext();

    Error read_from_file(const char* input_filename);
    Error read_from_memory(const void* data, size_t size);

    Error get_heif_image_data(heif_image_id ID, heif_image* out_data);

    heif_image* create_heif_image_buffer();
    // destory image compressed data buffer
    int destory_heif_image_buffer(heif_image* out_data);

    heif_image_id image_index_to_id(int img_index);
    int image_id_to_index(heif_image_id ID);

    class Image : public ErrorBuffer {
    public:
      Image(HeifContext* file, heif_image_id id);
      ~Image();

      void set_resolution(int w,int h) { m_width=w; m_height=h; }

      void set_primary(bool flag=true) { m_is_primary=flag; }

      heif_image_id get_id() const { return m_id; }

      int get_width() const { return m_width; }
      int get_height() const { return m_height; }

      bool is_primary() const { return m_is_primary; }

      // -- thumbnails

      void set_is_thumbnail_of(heif_image_id id) { m_is_thumbnail=true; m_thumbnail_ref_id=id; }
      void add_thumbnail(std::shared_ptr<Image> img) { m_thumbnails.push_back(img); }

      bool is_thumbnail() const { return m_is_thumbnail; }
      std::vector<std::shared_ptr<Image>> get_thumbnails() const { return m_thumbnails; }


      // --- alpha channel

      void set_is_alpha_channel_of(heif_image_id id) { m_is_alpha_channel=true; m_alpha_channel_ref_id=id; }
      void set_alpha_channel(std::shared_ptr<Image> img) { m_alpha_channel=img; }

      bool is_alpha_channel() const { return m_is_alpha_channel; }
      std::shared_ptr<Image> get_alpha_channel() const { return m_alpha_channel; }


      // --- depth channel

      void set_is_depth_channel_of(heif_image_id id) { m_is_depth_channel=true; m_depth_channel_ref_id=id; }
      void set_depth_channel(std::shared_ptr<Image> img) { m_depth_channel=img; }

      bool is_depth_channel() const { return m_is_depth_channel; }
      std::shared_ptr<Image> get_depth_channel() const { return m_depth_channel; }


      void set_depth_representation_info(struct heif_depth_representation_info& info) {
        m_has_depth_representation_info = true;
        m_depth_representation_info = info;
      }

      bool has_depth_representation_info() const {
        return m_has_depth_representation_info;
      }

      const struct heif_depth_representation_info& get_depth_representation_info() const {
        return m_depth_representation_info;
      }


      // --- metadata

      void add_metadata(std::shared_ptr<ImageMetadata> metadata) {
        m_metadata.push_back(metadata);
      }

      std::vector<std::shared_ptr<ImageMetadata>> get_metadata() const { return m_metadata; }

    private:
      HeifContext* m_heif_context;

      heif_image_id m_id;
      uint32_t m_width=0, m_height=0;
      bool     m_is_primary = false;

      bool     m_is_thumbnail = false;
      heif_image_id m_thumbnail_ref_id;

      std::vector<std::shared_ptr<Image>> m_thumbnails;
      // // add 
      // uint32_t m_image_type = 0;  // 0:hvc1, 1:grid, 2:iovl
      // uint32_t m_image_codec = 0; // default HEVC
      // uint32_t m_tiles_count = 1; // for apple HEIF format, there are many tiles. else = 1;

      bool m_is_alpha_channel = false;
      heif_image_id m_alpha_channel_ref_id;
      std::shared_ptr<Image> m_alpha_channel;

      bool m_is_depth_channel = false;
      heif_image_id m_depth_channel_ref_id;
      std::shared_ptr<Image> m_depth_channel;

      bool m_has_depth_representation_info = false;
      struct heif_depth_representation_info m_depth_representation_info;

      std::vector<std::shared_ptr<ImageMetadata>> m_metadata;
    };


    std::vector<std::shared_ptr<Image>> get_top_level_images() { return m_top_level_images; }

    std::shared_ptr<Image> get_primary_image() { return m_primary_image; }


    // debug info
    std::string debug_dump_boxes() const;
    // std::string debug_dump_image_info(int img_idx);

  private:
    std::map<heif_image_id, std::shared_ptr<Image>> m_all_images;

    // We store this in a vector because we need stable indices for the C API.
    std::vector<std::shared_ptr<Image>> m_top_level_images;

    std::shared_ptr<Image> m_primary_image; // shortcut to primary image

    std::shared_ptr<HeifFile> m_heif_file;

    Error interpret_heif_file();

    void remove_top_level_image(std::shared_ptr<Image> image);

    Error get_grid_image_data(heif_image_id ID, heif_image* out_data);

    int base_image_add_data(uint8_t *data, int data_len, base_image *base);
    void destory_base_image_buffer(base_image *base);
    int add_heif_sub_image(uint8_t *data, int data_len, heif_image *img);

    void reset_heif_image_buffer(heif_image *img);


  };
}

#endif
