/*****************************************************************************
 * Copyright (C) 2014 - 2017, CT-ACCEL 
 * All rights reserverd. 
 * 
 * @Author: Justin Mo 
 * @Date: 2018-02-02 13:55:37 
 * @Last Modified by: Justin Mo
 * @Last Modified time: 2018-02-02 17:17:21
 * @Version: V0.01 
 * @Description: 
*****************************************************************************/
#include "libde265/de265.h"
#include <malloc.h>

#include <stdio.h>

int main(int argc, char **argv)
{
    // const struct de265_image *img = 0;
    int more = 1;
    de265_decoder_context *ctx = de265_new_decoder();
    de265_start_worker_threads(ctx, 1);
    de265_error err =DE265_OK;
    int frame_count = 0;


    //
    uint8_t *data = NULL;
    int buf_size = 2*1024*1024;
    int data_len = 0;
    data = malloc(sizeof(uint8_t)*buf_size);

    const char *infile_name = "../grid_image.hevc";
    FILE *ifile = fopen(infile_name, "rb");
    data_len = fread(data, 1, buf_size, ifile);
    if(ifile) fclose(ifile);

    printf("HEVC file len: %d \n", data_len);
    de265_push_data(ctx, data, data_len, 0, 0);
    // de265_push_end_of_frame(ctx);
    de265_flush_data(ctx);

    
    while(more) {
        more = 0;
        err = de265_decode(ctx, &more);
        if(err != DE265_OK) {
            more = 0;
            break;
        }

        const struct de265_image *img = de265_get_next_picture(ctx);
        if(img) {
            // 
            printf("de265 decode %d framse\n", frame_count++);
        }

        // img = 0;
        // printf("more = %d , img = %p\n", more, img);
        // show warnings

        for (;;) {
            de265_error warning = de265_get_warning(ctx);
            if (warning==DE265_OK) {
              break;
            }

            fprintf(stderr,"WARNING: %s\n", de265_get_error_text(warning));
          }
    }




    if(data) free(data);

    de265_free_decoder(ctx);

    return 0;
}