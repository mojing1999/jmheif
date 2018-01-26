/**
 *  libde265 decode API
 * 
 */ 

#ifndef _HEIF_HEVC_DEC_
#define _HEIF_HEVC_DEC_

#include "libde265/de265.h"



de265_decoder_context *heif_create_hevc_decoder();

void heif_destroy_hevc_decoder(de265_decoder_context *ctx);

const struct de265_image *heif_decode_hevc_image(de265_decoder_context *ctx, uint8_t *data, int data_len, de265_PTS pts, void* user_data);

void heif_reset_hevc_decoder(de265_decoder_context *ctx);

#endif  // _HEIF_HEVC_DEC_