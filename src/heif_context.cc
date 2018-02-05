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

#include "heif_context.h"
#include <iostream>
#include <assert.h>
#include <math.h>
#include <string.h>

using namespace heif;


static int32_t readvec_signed(const std::vector<uint8_t>& data,int& ptr,int len)
{
  const uint32_t high_bit = 0x80<<((len-1)*8);

  uint32_t val=0;
  while (len--) {
    val <<= 8;
    val |= data[ptr++];
  }

  bool negative = (val & high_bit) != 0;
  val &= ~high_bit;

  if (negative) {
    return -(high_bit-val);
  }
  else {
    return val;
  }

  return val;
}


static uint32_t readvec(const std::vector<uint8_t>& data,int& ptr,int len)
{
  uint32_t val=0;
  while (len--) {
    val <<= 8;
    val |= data[ptr++];
  }

  return val;
}


class ImageGrid
{
public:
  Error parse(const std::vector<uint8_t>& data);

  std::string dump() const;

  uint32_t get_width() const { return m_output_width; }
  uint32_t get_height() const { return m_output_height; }
  uint16_t get_rows() const { return m_rows; }
  uint16_t get_columns() const { return m_columns; }

private:
  uint16_t m_rows;
  uint16_t m_columns;
  uint32_t m_output_width;
  uint32_t m_output_height;
};


Error ImageGrid::parse(const std::vector<uint8_t>& data)
{
  if (data.size() < 8) {
    return Error(heif_error_Invalid_input,
                 heif_suberror_Invalid_grid_data,
                 "Less than 8 bytes of data");
  }

  uint8_t version = data[0];
  (void)version; // version is unused

  uint8_t flags = data[1];
  int field_size = ((flags & 1) ? 32 : 16);

  m_rows    = static_cast<uint16_t>(data[2] +1);
  m_columns = static_cast<uint16_t>(data[3] +1);

  if (field_size == 32) {
    if (data.size() < 12) {
      return Error(heif_error_Invalid_input,
                   heif_suberror_Invalid_grid_data,
                   "Grid image data incomplete");
    }

    m_output_width = ((data[4] << 24) |
                      (data[5] << 16) |
                      (data[6] <<  8) |
                      (data[7]));

    m_output_height = ((data[ 8] << 24) |
                       (data[ 9] << 16) |
                       (data[10] <<  8) |
                       (data[11]));
  }
  else {
    m_output_width = ((data[4] << 8) |
                      (data[5]));

    m_output_height = ((data[ 6] << 8) |
                       (data[ 7]));
  }

  return Error::Ok;
}


std::string ImageGrid::dump() const
{
  std::ostringstream sstr;

  sstr << "rows: " << m_rows << "\n"
       << "columns: " << m_columns << "\n"
       << "output width: " << m_output_width << "\n"
       << "output height: " << m_output_height << "\n";

  return sstr.str();
}



class ImageOverlay
{
public:
  Error parse(size_t num_images, const std::vector<uint8_t>& data);

  std::string dump() const;

  void get_background_color(uint16_t col[4]) const;

  uint32_t get_canvas_width() const { return m_width; }
  uint32_t get_canvas_height() const { return m_height; }

  size_t get_num_offsets() const { return m_offsets.size(); }
  void get_offset(size_t image_index, int32_t* x, int32_t* y) const;

private:
  uint8_t  m_version;
  uint8_t  m_flags;
  uint16_t m_background_color[4];
  uint32_t m_width;
  uint32_t m_height;

  struct Offset {
    int32_t x,y;
  };

  std::vector<Offset> m_offsets;
};


Error ImageOverlay::parse(size_t num_images, const std::vector<uint8_t>& data)
{
  Error eofError(heif_error_Invalid_input,
                 heif_suberror_Invalid_grid_data,
                 "Overlay image data incomplete");

  if (data.size() < 2 + 4*2) {
    return eofError;
  }

  m_version = data[0];
  m_flags = data[1];

  if (m_version != 0) {
    std::stringstream sstr;
    sstr << "Overlay image data version " << m_version << " is not implemented yet";

    return Error(heif_error_Unsupported_feature,
                 heif_suberror_Unsupported_data_version,
                 sstr.str());
  }

  int field_len = ((m_flags & 1) ? 4 : 2);
  int ptr=2;

  if (ptr + 4*2 + 2*field_len + num_images*2*field_len > data.size()) {
    return eofError;
  }

  for (int i=0;i<4;i++) {
    uint16_t color = static_cast<uint16_t>(readvec(data,ptr,2));
    m_background_color[i] = color;
  }

  m_width  = readvec(data,ptr,field_len);
  m_height = readvec(data,ptr,field_len);

  m_offsets.resize(num_images);

  for (size_t i=0;i<num_images;i++) {
    m_offsets[i].x = readvec_signed(data,ptr,field_len);
    m_offsets[i].y = readvec_signed(data,ptr,field_len);
  }

  return Error::Ok;
}


std::string ImageOverlay::dump() const
{
  std::stringstream sstr;

  sstr << "version: " << ((int)m_version) << "\n"
       << "flags: " << ((int)m_flags) << "\n"
       << "background color: " << m_background_color[0]
       << ";" << m_background_color[1]
       << ";" << m_background_color[2]
       << ";" << m_background_color[3] << "\n"
       << "canvas size: " << m_width << "x" << m_height << "\n"
       << "offsets: ";

  for (const Offset& offset : m_offsets) {
    sstr << offset.x << ";" << offset.y << " ";
  }
  sstr << "\n";

  return sstr.str();
}


void ImageOverlay::get_background_color(uint16_t col[4]) const
{
  for (int i=0;i<4;i++) {
    col[i] = m_background_color[i];
  }
}


void ImageOverlay::get_offset(size_t image_index, int32_t* x, int32_t* y) const
{
  assert(image_index>=0 && image_index<m_offsets.size());
  assert(x && y);

  *x = m_offsets[image_index].x;
  *y = m_offsets[image_index].y;
}





HeifContext::HeifContext()
{
}

HeifContext::~HeifContext()
{
}

Error HeifContext::read_from_file(const char* input_filename)
{
  m_heif_file = std::make_shared<HeifFile>();
  Error err = m_heif_file->read_from_file(input_filename);
  if (err) {
    return err;
  }

  return interpret_heif_file();
}

Error HeifContext::read_from_memory(const void* data, size_t size)
{
  m_heif_file = std::make_shared<HeifFile>();
  Error err = m_heif_file->read_from_memory(data,size);
  if (err) {
    return err;
  }

  return interpret_heif_file();
}

std::string HeifContext::debug_dump_boxes() const
{
  return m_heif_file->debug_dump_boxes();
}


static bool item_type_is_image(const std::string& item_type)
{
  return (item_type=="hvc1" ||
          item_type=="grid" ||
          item_type=="iden" ||
          item_type=="iovl");
}


void HeifContext::remove_top_level_image(std::shared_ptr<Image> image)
{
  std::vector<std::shared_ptr<Image>> new_list;

  for (auto img : m_top_level_images) {
    if (img != image) {
      new_list.push_back(img);
    }
  }

  m_top_level_images = new_list;
}


class SEIMessage
{
public:
  virtual ~SEIMessage() { }
};


class SEIMessage_depth_representation_info : public SEIMessage,
                                             public heif_depth_representation_info
{
public:
};


double read_depth_rep_info_element(BitReader& reader)
{
  int sign_flag = reader.get_bits(1);
  int exponent  = reader.get_bits(7);
  int mantissa_len = reader.get_bits(5)+1;
  if (mantissa_len<1 || mantissa_len>32) {
    // TODO err
  }

  if (exponent==127) {
    // TODO value unspecified
  }

  int mantissa = reader.get_bits(mantissa_len);
  double value;

  //printf("sign:%d exponent:%d mantissa_len:%d mantissa:%d\n",sign_flag,exponent,mantissa_len,mantissa);

  if (exponent > 0) {
    value = pow(2, exponent-31) * (1.0 + mantissa / pow(2,mantissa_len));
  }
  else {
    value = pow(2, -(30+mantissa_len)) * mantissa;
  }

  if (sign_flag) {
    value = -value;
  }

  return value;
}


std::shared_ptr<SEIMessage> read_depth_representation_info(BitReader& reader)
{
  auto msg = std::make_shared<SEIMessage_depth_representation_info>();


  // default values

  msg->version = 1;

  msg->disparity_reference_view = 0;
  msg->depth_nonlinear_representation_model_size = 0;
  msg->depth_nonlinear_representation_model = nullptr;


  // read header

  msg->has_z_near = (uint8_t)reader.get_bits(1);
  msg->has_z_far  = (uint8_t)reader.get_bits(1);
  msg->has_d_min  = (uint8_t)reader.get_bits(1);
  msg->has_d_max  = (uint8_t)reader.get_bits(1);

  int rep_type;
  if (!reader.get_uvlc(&rep_type)) {
    // TODO error
  }
  // TODO: check rep_type range
  msg->depth_representation_type = (enum heif_depth_representation_type)rep_type;

  //printf("flags: %d %d %d %d\n",msg->has_z_near,msg->has_z_far,msg->has_d_min,msg->has_d_max);
  //printf("type: %d\n",rep_type);

  if (msg->has_d_min || msg->has_d_max) {
    int ref_view;
    if (!reader.get_uvlc(&ref_view)) {
      // TODO error
    }
    msg->disparity_reference_view = ref_view;

    //printf("ref_view: %d\n",msg->disparity_reference_view);
  }

  if (msg->has_z_near) msg->z_near = read_depth_rep_info_element(reader);
  if (msg->has_z_far ) msg->z_far  = read_depth_rep_info_element(reader);
  if (msg->has_d_min ) msg->d_min  = read_depth_rep_info_element(reader);
  if (msg->has_d_max ) msg->d_max  = read_depth_rep_info_element(reader);

  /*
  printf("z_near: %f\n",msg->z_near);
  printf("z_far: %f\n",msg->z_far);
  printf("dmin: %f\n",msg->d_min);
  printf("dmax: %f\n",msg->d_max);
  */

  if (msg->depth_representation_type == heif_depth_representation_type_nonuniform_disparity) {
    // TODO: load non-uniform response curve
  }

  return msg;
}


// aux subtypes: 00 00 00 11 / 00 00 00 0d / 4e 01 / b1 09 / 35 1e 78 c8 01 03 c5 d0 20

Error decode_hevc_aux_sei_messages(const std::vector<uint8_t>& data,
                                   std::vector<std::shared_ptr<SEIMessage>>& msgs)
{
  // TODO: we probably do not need a full BitReader just for the array size.
  // Read this and the NAL size directly on the array data.

  BitReader reader(data.data(), (int)data.size());
  uint32_t len = (uint32_t)reader.get_bits(32);

  if (len > data.size()-4) {
    // ERROR: read past end of data
  }

  while (reader.get_current_byte_index() < (int)len) {
    int currPos = reader.get_current_byte_index();
    BitReader sei_reader(data.data() + currPos, (int)data.size()-currPos);

    uint32_t nal_size = (uint32_t)sei_reader.get_bits(32);
    (void)nal_size;

    uint8_t nal_type = (uint8_t)(sei_reader.get_bits(8) >> 1);
    sei_reader.skip_bits(8);

    // SEI

    if (nal_type == 39 ||
        nal_type == 40) {

      // TODO: loading of multi-byte sei headers
      uint8_t payload_id = (uint8_t)(sei_reader.get_bits(8));
      uint8_t payload_size = (uint8_t)(sei_reader.get_bits(8));
      (void)payload_size;

      switch (payload_id) {
      case 177: // depth_representation_info
        std::shared_ptr<SEIMessage> sei = read_depth_representation_info(sei_reader);
        msgs.push_back(sei);
        break;
      }
    }

    break; // TODO: read next SEI
  }


  return Error::Ok;
}


Error HeifContext::interpret_heif_file()
{
  m_all_images.clear();
  m_top_level_images.clear();
  m_primary_image.reset();


  // --- reference all non-hidden images

  std::vector<heif_image_id> image_IDs = m_heif_file->get_item_IDs();

  for (heif_image_id id : image_IDs) {
    auto infe_box = m_heif_file->get_infe_box(id);

    
    if (!infe_box) {
      // TODO(farindk): Should we return an error instead of skipping the invalid id?
      continue;
    }

    // std::cout << "image id = " << id << " size = " << m_all_images.size() << std::endl;
    
    if (item_type_is_image(infe_box->get_item_type())) {
      auto image = std::make_shared<Image>(this, id);
      m_all_images.insert(std::make_pair(id, image));

      if (!infe_box->is_hidden_item()) {
        if (id==m_heif_file->get_primary_image_ID()) {
          image->set_primary(true);
          m_primary_image = image;
        }

        // std::cout << "Is hidden_item id = " << id << " top level size  " << m_top_level_images.size() << std::endl;
        m_top_level_images.push_back(image);
      }
    }
  }


  if (!m_primary_image) {
    return Error(heif_error_Invalid_input,
                 heif_suberror_Nonexisting_image_referenced,
                 "'pitm' box references a non-existing image");
  }


  // --- remove thumbnails from top-level images and assign to their respective image

  auto iref_box = m_heif_file->get_iref_box();
  if (iref_box) {
    // m_top_level_images.clear();

    for (auto& pair : m_all_images) {
      auto& image = pair.second;

      uint32_t type = iref_box->get_reference_type(image->get_id());

      if (type==fourcc("thmb")) {
        // --- this is a thumbnail image, attach to the main image

        std::vector<heif_image_id> refs = iref_box->get_references(image->get_id());
        if (refs.size() != 1) {
          return Error(heif_error_Invalid_input,
                       heif_suberror_Unspecified,
                       "Too many thumbnail references");
        }

        image->set_is_thumbnail_of(refs[0]);

        auto master_iter = m_all_images.find(refs[0]);
        if (master_iter == m_all_images.end()) {
          return Error(heif_error_Invalid_input,
                       heif_suberror_Nonexisting_image_referenced,
                       "Thumbnail references a non-existing image");
        }

        if (master_iter->second->is_thumbnail()) {
          return Error(heif_error_Invalid_input,
                       heif_suberror_Nonexisting_image_referenced,
                       "Thumbnail references another thumbnail");
        }

        master_iter->second->add_thumbnail(image);

        remove_top_level_image(image);
      }
      else if (type==fourcc("auxl")) {

        // --- this is an auxiliary image
        //     check whether it is an alpha channel and attach to the main image if yes

        std::vector<Box_ipco::Property> properties;
        Error err = m_heif_file->get_properties(image->get_id(), properties);
        if (err) {
          return err;
        }

        std::shared_ptr<Box_auxC> auxC_property;
        for (const auto& property : properties) {
          auto auxC = std::dynamic_pointer_cast<Box_auxC>(property.property);
          if (auxC) {
            auxC_property = auxC;
          }
        }

        if (!auxC_property) {
          std::stringstream sstr;
          sstr << "No auxC property for image " << image->get_id();
          return Error(heif_error_Invalid_input,
                       heif_suberror_Auxiliary_image_type_unspecified,
                       sstr.str());
        }

        std::vector<heif_image_id> refs = iref_box->get_references(image->get_id());
        if (refs.size() != 1) {
          return Error(heif_error_Invalid_input,
                       heif_suberror_Unspecified,
                       "Too many auxiliary image references");
        }


        // alpha channel

        if (auxC_property->get_aux_type() == "urn:mpeg:avc:2015:auxid:1" ||
            auxC_property->get_aux_type() == "urn:mpeg:hevc:2015:auxid:1") {
          image->set_is_alpha_channel_of(refs[0]);

          auto master_iter = m_all_images.find(refs[0]);
          master_iter->second->set_alpha_channel(image);
        }


        // depth channel

        if (auxC_property->get_aux_type() == "urn:mpeg:hevc:2015:auxid:2") {
          image->set_is_depth_channel_of(refs[0]);

          auto master_iter = m_all_images.find(refs[0]);
          master_iter->second->set_depth_channel(image);

          auto subtypes = auxC_property->get_subtypes();

          std::vector<std::shared_ptr<SEIMessage>> sei_messages;
          Error err = decode_hevc_aux_sei_messages(subtypes, sei_messages);

          for (auto& msg : sei_messages) {
            auto depth_msg = std::dynamic_pointer_cast<SEIMessage_depth_representation_info>(msg);
            if (depth_msg) {
              image->set_depth_representation_info(*depth_msg);
            }
          }
        }

        remove_top_level_image(image);
      }
      else {
        // 'image' is a normal image, keep it as a top-level image
      }
    }
  }


  // --- read through properties for each image and extract image resolutions

  for (auto& pair : m_all_images) {
    auto& image = pair.second;

    std::vector<Box_ipco::Property> properties;

    Error err = m_heif_file->get_properties(pair.first, properties);
    if (err) {
      return err;
    }

    bool ispe_read = false;
    for (const auto& prop : properties) {
      auto ispe = std::dynamic_pointer_cast<Box_ispe>(prop.property);
      if (ispe) {
        uint32_t width = ispe->get_width();
        uint32_t height = ispe->get_height();


        // --- check whether the image size is "too large"

        if (width  >= static_cast<uint32_t>(std::numeric_limits<int>::max()) ||
            height >= static_cast<uint32_t>(std::numeric_limits<int>::max())) {
          std::stringstream sstr;
          sstr << "Image size " << width << "x" << height << " exceeds the maximum image size "
               << std::numeric_limits<int>::max() << "x"
               << std::numeric_limits<int>::max() << "\n";

          return Error(heif_error_Memory_allocation_error,
                       heif_suberror_Security_limit_exceeded,
                       sstr.str());
        }

        image->set_resolution(width, height);
        ispe_read = true;
      }

      if (ispe_read) {
        auto clap = std::dynamic_pointer_cast<Box_clap>(prop.property);
        if (clap) {
          image->set_resolution( clap->get_width_rounded(),
                                 clap->get_height_rounded() );
        }

        auto irot = std::dynamic_pointer_cast<Box_irot>(prop.property);
        if (irot) {
          if (irot->get_rotation()==90 ||
              irot->get_rotation()==270) {
            // swap width and height
            image->set_resolution( image->get_height(),
                                   image->get_width() );
          }
        }
      }
    }
  }



  // --- read metadata and assign to image

  for (uint32_t id : image_IDs) {
    std::string item_type = m_heif_file->get_item_type(id);
    if (item_type == "Exif") {
      std::shared_ptr<ImageMetadata> metadata = std::make_shared<ImageMetadata>();
      metadata->item_type = item_type;

      Error err = m_heif_file->get_compressed_image_data(id, &(metadata->m_data));
      if (err) {
        return err;
      }

      //std::cerr.write((const char*)data.data(), data.size());


      // --- assign metadata to the image

      if (iref_box) {
        uint32_t type = iref_box->get_reference_type(id);
        if (type == fourcc("cdsc")) {
          std::vector<uint32_t> refs = iref_box->get_references(id);
          if (refs.size() != 1) {
            return Error(heif_error_Invalid_input,
                         heif_suberror_Unspecified,
                         "Exif data not correctly assigned to image");
          }

          uint32_t exif_image_id = refs[0];
          auto img_iter = m_all_images.find(exif_image_id);
          if (img_iter == m_all_images.end()) {
            return Error(heif_error_Invalid_input,
                         heif_suberror_Nonexisting_image_referenced,
                         "Exif data assigned to non-existing image");
          }

          img_iter->second->add_metadata(metadata);
        }
      }
    }
  }

  return Error::Ok;
}


HeifContext::Image::Image(HeifContext* context, heif_image_id id)
  : m_heif_context(context),
    m_id(id)
{
}

HeifContext::Image::~Image()
{
}

heif_image_id HeifContext::image_index_to_id(int img_index)
{

  int count = m_top_level_images.size();
  if(img_index < 0 || img_index >= count) {
    return INVALID_IMAGE_ID;
  }

  return m_top_level_images[img_index]->get_id();
}

int HeifContext::image_id_to_index(heif_image_id ID)
{
  // std::cout << " image_id_to_index() ID = " << ID << std::endl;
  // std::cout << " image_id_to_index() m_all_images size = " << m_top_level_images.size() << std::endl;

  int count = m_top_level_images.size();
  for(int i = 0; i < count; i++) {
    if(ID == m_top_level_images[i]->get_id()) {
      return i;
    }
  }

  return -1;
}

heif_image* HeifContext::create_heif_image_buffer()
{
  heif_image* img = new heif_image;
  memset(img, 0x0, sizeof(heif_image));
  img->image_type = HEIF_IMAGE_TYPE_UNKNOW;

  return img;
}

int HeifContext::destory_heif_image_buffer(heif_image* img)
{

  if(img->image_type == HEIF_IMAGE_TYPE_HVC1) {
    // destory_base_image_buffer(&img->_image);
  }
  else if(img->image_type == HEIF_IMAGE_TYPE_GRID) {

  }
  else if(img->image_type == HEIF_IMAGE_TYPE_IDEN) {

  }
  else if(img->image_type == HEIF_IMAGE_TYPE_IOVL) {
    // unknow type
  }

  // release base image buffer
  destory_base_image_buffer(&img->_image);

  // release thumb image buffer
  destory_base_image_buffer(&img->thumb);

  img->image_type = HEIF_IMAGE_TYPE_UNKNOW;

  if(img->yuv_image) {
    delete [] img->yuv_image;
    img->yuv_image = nullptr;
  }

  delete img;

  return 0;
}

#if 0
base_image *HeifContext::create_base_image_buffer(uint8_t *data, int _size)
{
  base_image *img = new base_image;
  memset(img, 0x0, sizeof(base_image));

  img->buf_size = _size;
  img->buf = new uint8_t[img->buf_size];
  memset(img->buf, 0x0, img->buf_size);
  memcpy(img->buf, data, img->buf_size);
  img->data_len = _size;

  return img;
}


void HeifContext::destroy_base_image_buffer(base_image **base_img)
{
  base_image *img = *base_img;

  if(img) {
    if(img->buf) {
      delete [] img->buf;
      img->buf = 0;
      img->buf_size = 0;
      img->data_len = 0;
    }

    img = nullptr;
  }

  return ;
}

thumb_image *HeifContext::create_thumb_image_buffer(int width, int height, uint8_t *data, int _size)
{
  thumb_image *img = new thumb_image;
  memset(img, 0x0, sizeof(thumb_image));

  img->thumb = create_base_image_buffer(data, _size);
  img->thumb_width = width;
  img->thumb_height = height;

  return img;
}

void HeifContext::destory_thumb_image_buffer(thumb_image **thumb_img)
{
  thumb_image *img = *thumb_img;

  if(img) {
    destroy_base_image_buffer(&img->thumb);

    delete img;
    img = nullptr;
  }
}

hvc1_image *HeifContext::create_hvc1_image_buffer(uint8_t *data, int _size)
{
  hvc1_image *img = new hvc1_image;
  memset(img, 0x0, sizeof(hvc1_image));

  img->hvc1 = create_base_image_buffer(data, _size);

  return img;
}


void HeifContext::destory_hvc1_image_buffer(hvc1_image **hvc1_img)
{
  hvc1_image *img = *hvc1_img;

  if(img) {
    destroy_base_image_buffer(&img->hvc1);

    delete img;
    img = nullptr;
  }

  return ;
}

grid_image *HeifContext::create_grid_image_buffer(int _count, 
                                    int _rows, int _columns,
                                    int _width, int _height)
{
  grid_image *img = new grid_image;
  memset(img, 0x0, sizeof(grid_image));

  img->tiles = new base_image*[_count];
  memset(img->tiles, 0x0, sizeof(base_image*) * _count);

  img->tiles_count = 0;//_count;
  img->tile_rows = _rows;
  img->tile_columns = _columns;
  img->tile_width = _width;
  img->tile_height = _height;

  return img;
}

bool HeifContext::grid_image_add_tile(grid_image *img, uint8_t *tile_data, int tile_data_len)
{
  if(img->tiles_count >= img->tile_rows * img->tile_columns) {
    // error
    return false;
  }

  base_image *tile = create_base_image_buffer(tile_data, tile_data_len);
  img->tiles[img->tiles_count++] = tile;

  return 0;
}

void HeifContext::destroy_grid_image_buffer(grid_image **grid_img)
{
  grid_image *img = *grid_img;
  if(img) {
    for(int i = 0; i < img->tiles_count; i++) {
      destroy_base_image_buffer(&img->tiles[i]);
    }

    // delete tiles
    delete [] img->tiles;

    // delete img
    delete img;
    img = nullptr;
  }

  return ;
}
#endif

void HeifContext::destory_base_image_buffer(base_image *base) 
{
  if(base->buf) {
    delete [] base->buf;
    base->buf = 0;
  }

  base->buf_size = 0;
  base->data_len = 0;

  return;
}

int HeifContext::base_image_add_data(uint8_t *data, int data_len, base_image *base)
{
  // printf("base_image_add_data() data_len = %d\n", data_len);
#define BASE_IMAGE_BUF_SIZE (2 << 20)
  if(!base->buf) {
    base->buf = new uint8_t[BASE_IMAGE_BUF_SIZE];
    base->buf_size = BASE_IMAGE_BUF_SIZE;
  }

  if(data_len > base->buf_size - base->data_len) {
    // not enought buffer, realloc
    int len = data_len > BASE_IMAGE_BUF_SIZE ? data_len : BASE_IMAGE_BUF_SIZE;
    len += base->buf_size;
    uint8_t *tmp = new uint8_t[len];
    memcpy(tmp, base->buf, base->data_len);
    base->buf_size = len;
    delete [] base->buf;
    base->buf = tmp;
  }

  memcpy(base->buf+base->data_len, data, data_len);
  base->data_len += data_len;

  return base->data_len;
}

int HeifContext::add_heif_sub_image(uint8_t *data, int data_len, heif_image *img)
{
  if(HEIF_IMAGE_TYPE_HVC1 == img->image_type){
    // hvc1
    base_image_add_data(data, data_len, &img->_image);

  } else if(HEIF_IMAGE_TYPE_GRID == img->image_type){
    // grid image

    // if(img->tiles_count > img->tile_rows * img->tile_columns) {
    //   return false;
    // }

    // add data to buffer
    base_image_add_data(data, data_len, &img->_image);


  } else if(HEIF_IMAGE_TYPE_IOVL == img->image_type){
  
  } else if(HEIF_IMAGE_TYPE_IOVL == img->image_type){

  }

  return 0;
}

void HeifContext::reset_heif_image_buffer(heif_image *img)
{
  img->width = 0;
  img->height = 0;
  img->bit_depth = 0;
  img->chroma = 0;
  img->codec_type = 0;
  img->image_type = HEIF_IMAGE_TYPE_UNKNOW;
  img->_image.data_len = 0;

  img->thumb.data_len = 0;
  img->thumb_width = 0;
  img->thumb_height  = 0;

  img->tiles_count = 0;
  img->tile_rows = 0;
  img->tile_columns = 0;
  img->tile_width = 0;
  img->tile_height  = 0;


  return ;
}

Error HeifContext::get_heif_image_data(heif_image_id ID, heif_image* out_data)
{

  std::string image_type = m_heif_file->get_item_type(ID);

  Error err;
  
  //
  // std::cout << "image id " << ID << " image_type = " << image_type << std::endl;

  // TODO:
  reset_heif_image_buffer(out_data);

  if(image_type == "hvc1") {
    std::vector<uint8_t> data;
    err = m_heif_file->get_compressed_image_data(ID, &data);

    out_data->image_type = HEIF_IMAGE_TYPE_HVC1;

    // info
    const std::shared_ptr<Image> hvc1 = m_all_images.find(ID)->second;
    out_data->width       = hvc1->get_width();
    out_data->height      = hvc1->get_height();
    // out_data->bit_depth   = ;
    // out_data->chroma      = ;
    // out_data->codec_type  = ;

    add_heif_sub_image(data.data(), data.size(), out_data);
  }
  else if(image_type == "grid") {
    std::cout << "grid id: " << ID << std::endl;
    out_data->image_type = HEIF_IMAGE_TYPE_GRID;
    err = get_grid_image_data(ID, out_data);    
  }
  else {
    out_data->image_type = HEIF_IMAGE_TYPE_UNKNOW;
  }

  return Error::Ok;
}



Error HeifContext::get_grid_image_data(heif_image_id ID, heif_image* out_data)
{
  std::vector<uint8_t> data;
  Error err = m_heif_file->get_compressed_image_data(ID, &data);


  ImageGrid grid;
  grid.parse(data);

  std::cout << grid.dump();

  auto iref_box = m_heif_file->get_iref_box();

  if (!iref_box) {
  return Error(heif_error_Invalid_input,
                heif_suberror_No_iref_box,
                "No iref box available, but needed for grid image");
  }

  std::vector<uint32_t> image_references = iref_box->get_references(ID);

  if ((int)image_references.size() != grid.get_rows() * grid.get_columns()) {
    std::stringstream sstr;
    sstr << "Tiled image with " << grid.get_rows() << "x" <<  grid.get_columns() << "="
        << (grid.get_rows() * grid.get_columns()) << " tiles, but only "
        << image_references.size() << " tile images in file";

    return Error(heif_error_Invalid_input,
                heif_suberror_Missing_grid_images,
                sstr.str());
  }

  // info
  out_data->image_type  = HEIF_IMAGE_TYPE_GRID;
  out_data->width       = grid.get_width();
  out_data->height      = grid.get_height();
  // out_data->bit_depth   = ;
  // out_data->chroma      = ;
  // out_data->codec_type  = ;

  bool first_time = true;
  
  heif_image_id tileID = 0;
  int reference_idx = 0;
  // int X0 = 0, Y0 = 0; // tile left-top point
  for(int y = 0; y < grid.get_rows(); y++) {
    for(int x = 0; x < grid.get_columns(); x++) {

      tileID = image_references[reference_idx];

      const std::shared_ptr<Image> tileImg = m_all_images.find(tileID)->second;
      int src_width = tileImg->get_width();
      int src_height = tileImg->get_height();

      std::vector<uint8_t> tile_data;
      // std::cout << "tile id: " << tileID << std::endl;
      err = m_heif_file->get_compressed_image_data(tileID, &tile_data);
      if(err) {
        std::cout << " get compressed image data error " << err.message << std::endl;
      }

      if(first_time) {
        out_data->tiles_count = image_references.size();
        out_data->tile_rows = grid.get_rows();
        out_data->tile_columns = grid.get_columns();
        out_data->tile_width = src_width;
        out_data->tile_height = src_height;
        first_time = false;
        printf("grid image row[%d], col[%d], width[%d], height[%d]\n", grid.get_rows(), grid.get_columns(), src_width, src_height);
      }

      // add grid image tiles
      add_heif_sub_image(tile_data.data(), tile_data.size(), out_data);
      
      reference_idx++;
    } // for (int x = 0; ...)
  } // for (int y = 0; ...)

  return Error::Ok;
}

