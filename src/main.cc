#include "heif.h"

#include <iostream>
#include <vector>
#include <sstream>
#include <string.h>
#include <unistd.h>

#include "libde265/de265.h"
#include "jpeglib.h"
#include "jerror.h"

#include "SDL2/SDL.h"

#define DISPLAY_BY_SDL 1

using namespace std;

#define TEMP_TEST 0

int sdl_refresh_image();


int merge_tile_to_heif_image(const struct de265_image* tile_img, int idx_tile, heif_image *heif_img)
{
    int row_x, col_y;
    uint8_t *heif_img_yuv[3] = { 0 };
    int heif_img_width = 0, heif_img_height = 0;

    int tile_width = 0, tile_height = 0;
#if 0
    int bits = 0;
    int chroma_fmt = 0;
    int colorspace = 0;

    //
    bits = heif_img->info.bit_depth;
    chroma_fmt = heif_img->info.chroma;
    colorspace = heif_img->info.colorspace;
#endif

    row_x = idx_tile / heif_img->tile_columns;
    col_y = idx_tile % heif_img->tile_columns;

    int len = heif_img->width * heif_img->height;

    if(!heif_img->yuv_image) {
        // default for YUV420
        heif_img->yuv_len = len * 3 / 2;
        heif_img->yuv_image = new uint8_t[len * 3 / 2];
        memset(heif_img->yuv_image, 0, len * 3 / 2);
    }
    else if((len * 3 / 2) > heif_img->yuv_len){
        delete [] heif_img->yuv_image;
        heif_img->yuv_len = len * 3 / 2;
        heif_img->yuv_image = new uint8_t[len * 3 / 2];
        memset(heif_img->yuv_image, 0, len * 3 / 2);
    }

    heif_img_yuv[0] = heif_img->yuv_image;
    heif_img_yuv[1] = heif_img_yuv[0] + len;
    heif_img_yuv[2] = heif_img_yuv[1] + len / 4;

    heif_img_width = heif_img->width;
    heif_img_height = heif_img->height;


    //
    tile_width = de265_get_image_width(tile_img, 0);   // heif_img->tile_info->width;
    tile_height = de265_get_image_height(tile_img, 0);   // heif_img->tile_info->height;
    // printf("merge_tile_to_heif_image() idx_tile = %d\n", idx_tile);

    // ---------------------------------------------------------------------------
    for (int c = 0; c < 3; c++) {

        int stride;
        const uint8_t* data = de265_get_image_plane(tile_img, c, &stride);

        int w = de265_get_image_width(tile_img, c);
        int h = de265_get_image_height(tile_img, c);

        // correct heif_img_width
        int dst_width = 0, dst_height = 0;
        if(c == 0) {
            dst_width = heif_img_width;
            dst_height = heif_img_height;
        }
        else {
            dst_width = heif_img_width / 2;
            dst_height = heif_img_height / 2;
        }


        tile_width = w;
        tile_height = h;


        uint8_t *dst_mem = heif_img_yuv[c] + (row_x * tile_height * dst_width) + (col_y * tile_width);

        // correct w and h for right and bottom crop
        if(col_y + 1 == heif_img->tile_columns) {
            w = dst_width - col_y * tile_width;
        }

        if(row_x + 1 == heif_img->tile_rows) {
            h = dst_height - row_x * tile_height;
        }


        for (int y=0; y<h; y++) {
            memcpy(dst_mem + y*dst_width, data + y*stride, w);
        }
    }


    return 0;
}


#if 0


void convert_yuv420_to_jpeg(uint8_t *yuv_buf, int width, int height, int quality, const char *jpeg_name)
{
    FILE *ofile = fopen(jpeg_name, "wb");

    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    jpeg_stdio_dest(&cinfo, ofile);

    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_YCbCr;
    jpeg_set_defaults(&cinfo);
    
    //static const bool kForceBaseline = true;
    jpeg_set_quality(&cinfo, quality, TRUE);
    //cinfo.raw_data_in = TRUE;

    jpeg_start_compress(&cinfo, TRUE);

    // get yuv data
    int stride_y = width;
    int stride_u = width / 2;
    int stride_v = width / 2;

    const uint8_t *row_y = yuv_buf;
    const uint8_t *row_u = row_y + width * height;
    const uint8_t *row_v = row_u + width * height / 4;

    //
    JSAMPARRAY buffer = cinfo.mem->alloc_sarray(
        reinterpret_cast<j_common_ptr>(&cinfo), JPOOL_IMAGE,
        cinfo.image_width * cinfo.input_components, 1);
    JSAMPROW row[1] = { buffer[0] };

    while(cinfo.next_scanline < cinfo.image_height) {
        size_t offset_y = cinfo.next_scanline * stride_y;
        const uint8_t *start_y = &row_y[offset_y];

        size_t offset_u = (cinfo.next_scanline / 2) * stride_u;
        const uint8_t *start_u = &row_u[offset_u];

        size_t offset_v = (cinfo.next_scanline / 2) * stride_v;
        const uint8_t *start_v = &row_v[offset_v];

        JOCTET *bufp = buffer[0];
        for(JDIMENSION x = 0; x < cinfo.image_width; ++x) {
            *bufp++ = start_y[x];
            *bufp++ = start_u[x/2];
            *bufp++ = start_v[x/2];
        }

        jpeg_write_scanlines(&cinfo, row, 1);

    }
    jpeg_finish_compress(&cinfo);

    fclose(ofile);
    jpeg_destroy_compress(&cinfo);

    return ;
}

#endif


/**
 *  libde265 decode image
 * 
 */
static int heif_decode_grid_image(heif_image *heif_img)
{
    de265_error err =DE265_OK;
    int frame_index = 0;
    int more = 1;

    // printf("heif_decode_grid_image(), width = %d, height = %d, data_len = %d\n", heif_img->width, heif_img->height, heif_img->_image.data_len);

    de265_decoder_context *ctx = de265_new_decoder();
    de265_start_worker_threads(ctx, 1);


    // push data to decoder
    de265_push_data(ctx, heif_img->_image.buf, heif_img->_image.data_len, 0, 0);
    // de265_push_end_of_frame(ctx);
    de265_flush_data(ctx);

    // get image
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
            // printf("de265 decode %d framse\n", frame_index);
            merge_tile_to_heif_image(img, frame_index, heif_img);

            sdl_refresh_image();
            frame_index ++;

            // usleep(100000);
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




    // delete decoder
    de265_free_decoder(ctx);

    return 0;
}

static int heif_decode_hvc1_image(heif_image *heif_img)
{
    de265_error err =DE265_OK;
    int frame_index = 0;
    int more = 1;

    // printf("heif_decode_hvc1_image(), width = %d, height = %d\n", heif_img->width, heif_img->height);
    de265_decoder_context *ctx = de265_new_decoder();
    de265_reset(ctx);
    de265_start_worker_threads(ctx, 1);


    // push data to decoder
    de265_push_data(ctx, heif_img->_image.buf, heif_img->_image.data_len, 0, 0);
    // de265_push_end_of_frame(ctx);
    de265_flush_data(ctx);

    // get image
    while(more) {
        more = 0;
        err = de265_decode(ctx, &more);
        if(err != DE265_OK) {
            more = 0;

            printf("----------- de265_decode err = %d\n", err);
            break;
        }

        const struct de265_image *img = de265_get_next_picture(ctx);
        if(img) {
            // printf("de265 decode %d framse, img = %p\n", frame_index, img);
            // 
            // merge_tile_to_heif_image(img, frame_index, heif_img);
            uint8_t *heif_img_yuv[3] = { 0 };

            int width = de265_get_image_width(img, 0);
            int height = de265_get_image_height(img, 0);
            int len = width * height;

            if(!heif_img->yuv_image) {
                // default for YUV420
                heif_img->yuv_len = len * 3 / 2;
                heif_img->yuv_image = new uint8_t[len * 3 / 2];
                memset(heif_img->yuv_image, 0, len * 3 / 2);

                // printf("heif_img->yuv_image = %p, len = %d\n", heif_img->yuv_image, heif_img->yuv_len);
            }
            else if((len * 3 / 2) > heif_img->yuv_len){
                delete [] heif_img->yuv_image;
                heif_img->yuv_len = len * 3 / 2;
                heif_img->yuv_image = new uint8_t[len * 3 / 2];
                memset(heif_img->yuv_image, 0, len * 3 / 2);
                // printf("heif_img->yuv_image = %p, len = %d\n", heif_img->yuv_image, heif_img->yuv_len);
            }

            heif_img_yuv[0] = heif_img->yuv_image;
            heif_img_yuv[1] = heif_img_yuv[0] + len;
            heif_img_yuv[2] = heif_img_yuv[1] + len / 4;
            
            for (int c = 0; c < 3; c++) {
                int stride;
                const uint8_t* data = de265_get_image_plane(img, c, &stride);

                int w = de265_get_image_width(img, c);
                int h = de265_get_image_height(img, c);

                // printf("w = %d, h = %d, stride = %d\n", w, h, stride);

                uint8_t *dst_mem = heif_img_yuv[c];
                for(int y = 0; y < h; y++) {
                    memcpy(dst_mem + y*w, data+y*stride, w);
                }

            }




            sdl_refresh_image();
            frame_index ++;
            break;
            // usleep(100000);
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
            break;
          }
    }




    // delete decoder
    de265_free_decoder(ctx);

    return 0;
}



// save hevc data to file
int save_hevc_to_file(const char *file_name, uint8_t *data, int data_len)
{
    FILE *ofile = fopen(file_name, "wb");

    fwrite(data, 1, data_len, ofile);

    fclose(ofile);

    return 0;
}

/**
 *  SDL refresh
 */
#define SFM_REFRESH_EVENT  (SDL_USEREVENT + 1) 
int sdl_refresh_image()
{
    // printf("sdl_refresh_image() ----- \n");
    SDL_Event event;
    event.type = SFM_REFRESH_EVENT;
    SDL_PushEvent(&event);

    return 0;
}

int heif_switch_image(heif_handle h, int index, heif_image *img)
{
    heif_get_image_data(h, index, img);
    if(HEIF_IMAGE_TYPE_GRID == img->image_type) {
        heif_decode_grid_image(img);
    }
    if(HEIF_IMAGE_TYPE_HVC1 == img->image_type) {
        heif_decode_hvc1_image(img);
    }

    sdl_refresh_image();


    return 0;
}

int main(int argc, char **argv)
{
    //const char *heif_file_name = "/home/justin/media/IMG_4453.HEIC";
    //std::string in_filename("/home/justin/media/IMG_4453.HEIC");
    //std::string in_filename("/home/justin/media/IMG_4454.HEIC");

    if(argc < 2) {
        printf("Usage: test_heif input_file");
        return 0;
    }

    std::string in_filename(argv[1]);

    heif_handle h;
    heif_error err;

    int num_imgs = 0;
    int index = 0;

    h = heif_hendle_alloc();

    err = heif_read_from_file(h, in_filename.c_str());
    if(0 != err.code) {
        std::cerr << "Can not read HEIF file " << err.message << endl;
    }


    // heif_debug_dump_box(h);

    cout << "----------------------------------------------" << endl;

    num_imgs = heif_get_number_of_images(h);

    cout << "number of images: " << num_imgs << endl;


    uint32_t idx_primary = heif_get_primary_image_index(h);

    cout << "primary image index: " << idx_primary << endl;



    // heif_debug_dump_image_info(h, idx_primary);


    index = idx_primary;
    heif_image *image_data = heif_create_image_buffer(h);
    //memset(&image_data, 0x0, sizeof(heif_image));
    err = heif_get_image_data(h, index, image_data);

    if(0 != err.code) {
        std::cerr << "Can not get HEIF image data " << err.message << endl;

        return 0;
    }


#if 1
    if(HEIF_IMAGE_TYPE_GRID == image_data->image_type) {
        heif_decode_grid_image(image_data);

        // std::cout << "grid image size " << image_data->_image.data_len << std::endl;
        // const char *file_name = "grid_image.hevc";
        // save_hevc_to_file(file_name, image_data->_image.buf, image_data->_image.data_len);
        
    }
    else if(HEIF_IMAGE_TYPE_HVC1 == image_data->image_type) {
        // const char *file_name = "hvc1_image.hevc";
        // save_hevc_to_file(file_name, image_data->_image.buf, image_data->_image.data_len);
        heif_decode_hvc1_image(image_data);
    }


    // cout << "save hevc finsh" << endl;
#endif

    // TODO：


    int width = image_data->width;
    int height = image_data->height;

#if DISPLAY_BY_SDL
    // SDL2
    SDL_Window *window = 0;
    SDL_Renderer *renderer = 0;
    SDL_Texture *texture = 0;
    SDL_Event event;
    SDL_Rect srcrect, dstrect;
    srcrect.x = 0;
    srcrect.y = 0;
    srcrect.w = width;
    srcrect.h = height;

    int wnd_width = 800;
    int wnd_height = 600;
    dstrect.x = 0;
    dstrect.y = 0;
    dstrect.w = wnd_width;
    dstrect.h = wnd_height;


    if(SDL_Init(SDL_INIT_VIDEO) < 0) {
        cout << "SDL can not init, err: " << SDL_GetError() << endl;
        return 0;
    }

    //
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

    window = SDL_CreateWindow("HEIF player",
            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
            wnd_width, wnd_height, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE/* | SDL_WINDOW_MAXIMIZED*/);

    if(NULL == window) {
        cout << "SDL window can not be created, err: " << SDL_GetError() << endl;
        return 0;
    }

    renderer = SDL_CreateRenderer(window, -1, 0);
    if(NULL == renderer) {
        cout << "SDL renderer can not be create, err: " << SDL_GetError() << endl;
        return 0;
    }

    SDL_RendererInfo info;
    SDL_GetRendererInfo(renderer, &info);
    cout << "Using " << info.name << " rendering" << endl;

    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_IYUV/*SDL_PIXELFORMAT_YV12*/, SDL_TEXTUREACCESS_STATIC/*SDL_TEXTUREACCESS_STREAMING*/, width, height);
    if(NULL == texture) {
        cout << "SDL texture can not be create, err: " << SDL_GetError() << endl;
        return 0;
    }

    // 
    SDL_GetWindowSize(window, &wnd_width, &wnd_height);
    dstrect.w = wnd_width;
    dstrect.h = wnd_height;
#endif


#if 0   // save to jpg
    std::string jpg_name = in_filename + ".jpg";
    //char * jpeg_name = (char*)"heif2jpeg.jpg";
    convert_yuv420_to_jpeg(image_data->heif_yuv_data, width, height, 80, jpg_name.c_str());


#endif


#if DISPLAY_BY_SDL

    sdl_refresh_image();

    for(;;) {
        SDL_WaitEvent(&event);
        if(event.type == SDL_QUIT) {
            printf("--- SDL SDL_QUIT\n");
            break;
        }
        else if(event.type == SFM_REFRESH_EVENT) {
            // display
            // printf("--- event.type == SFM_REFRESH_EVENT(%d)\n", event.type);
            if(image_data->yuv_image) {
                width = image_data->width;
                int stride = width, chroma_stride = width / 2;
                const uint8_t* y = image_data->yuv_image;
                const uint8_t* cb = y + width * height;
                const uint8_t* cr = cb + width*height / 4;

                // printf("");
                // printf("y=%p, stride=%d, cb = %p, cr = %p, chroma_stride = %d\n", y, stride, cb, cr, chroma_stride);
                SDL_UpdateYUVTexture(texture, NULL, 
                                    y, stride,
                                    cb, chroma_stride,
                                    cr, chroma_stride);

                // SDL_UpdateTexture(texture, NULL, image_data->yuv_image, width);

                SDL_RenderClear(renderer);
                SDL_RenderCopy(renderer, texture, &srcrect, &dstrect);
                SDL_RenderPresent(renderer);
            }
        }
        else if(event.type == SDL_KEYDOWN) {
            // printf("--- SDL keydown : %d\n", event.key.keysym.sym);
            if(event.key.keysym.sym == SDLK_ESCAPE) {
                break;
            }
            else if(event.key.keysym.sym == SDLK_RIGHT) {
                // switch image display
                index = (index+1) %  num_imgs;
                heif_switch_image(h, index, image_data);
            }
        }
    }


    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
#endif


    // free heif_image
    // heif_destroy_hevc_decoder(dec_ctx);

    heif_destory_image_buffer(h, image_data);
    heif_handle_free(h);

    return 0;
}