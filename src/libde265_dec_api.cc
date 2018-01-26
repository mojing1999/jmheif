#include "libde265_dec_api.h"
#include <stdio.h>

/**
 *  create H265 decoder
 */
de265_decoder_context *heif_create_hevc_decoder()
{
    de265_decoder_context *ctx = de265_new_decoder();
    de265_start_worker_threads(ctx, 1);

    return ctx;
}

void heif_destroy_hevc_decoder(de265_decoder_context *ctx)
{
    de265_free_decoder(ctx);

    return;
}

const struct de265_image *heif_decode_hevc_image(de265_decoder_context *ctx, uint8_t *data, int data_len, de265_PTS pts, void* user_data)
{
    const struct de265_image *img = 0;
    int more = 0;

    de265_push_data(ctx, data, data_len, pts, user_data);
    de265_push_end_of_frame(ctx);
    de265_flush_data(ctx);

    do{
        de265_error err = de265_decode(ctx, &more);
        //cout << "err = " << err << " more = " << more << endl;
        img = de265_get_next_picture(ctx);

        for(;;) {
            err = de265_get_warning(ctx);
            if(err == DE265_OK) {
                break;
            }
            printf("WARNING: %s\n", de265_get_error_text(err));
        }
    }while(more & !img);

    return img;
}

void heif_reset_hevc_decoder(de265_decoder_context *ctx)
{
    de265_reset(ctx);
}