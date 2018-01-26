#include "heif.h"

#include <iostream>
#include <vector>
#include <sstream>
#include <string.h>

#include "libde265_dec_api.h"

#include "jpeglib.h"
#include "jerror.h"

#include "SDL2/SDL.h"

#define DISPLAY_BY_SDL 1

using namespace std;

#define TEMP_TEST 0

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

    int len = heif_img->info.width * heif_img->info.height;

    if(!heif_img->heif_yuv_data) {
        // default for YUV420
        heif_img->heif_yuv_data = new uint8_t[len * 3 / 2];
        memset(heif_img->heif_yuv_data, 0, len * 3 / 2);
    }

    heif_img_yuv[0] = heif_img->heif_yuv_data;
    heif_img_yuv[1] = heif_img_yuv[0] + len;
    heif_img_yuv[2] = heif_img_yuv[1] + len / 4;

    heif_img_width = heif_img->info.width;
    heif_img_height = heif_img->info.height;


    //
    tile_width = de265_get_image_width(tile_img, 0);   // heif_img->tile_info->width;
    tile_height = de265_get_image_height(tile_img, 0);   // heif_img->tile_info->height;


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



heif_image *create_heif_image_buffer()
{
    heif_image *img = new heif_image;

    memset(img, 0x0, sizeof(heif_image));

    return img;
}

void destroy_heif_image_buffer(heif_image *img)
{
    if(img->heif_yuv_data) {
        delete [] img->heif_yuv_data;
        img->heif_yuv_data = 0;
    }

    if(img->arr_tile_data_len) {
        delete [] img->arr_tile_data_len;
        img->arr_tile_data_len = 0;
    }
    
    cout << " delete image data tile data" << endl;
    if(img->arr_tile_data) {
        for(int i = 0; i < img->tiles_count; i++) {
            if(img->arr_tile_data[i]) {
                delete [] img->arr_tile_data[i];
                img->arr_tile_data[i] = 0;
            }
        }
        
        delete [] img->arr_tile_data;
        img->arr_tile_data = 0;
    }

    return ;
}


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

    h = heif_hendle_alloc();

    err = heif_read_from_file(h, in_filename.c_str());
    if(0 != err.code) {
        std::cerr << "Can not read HEIF file " << err.message << endl;
    }


    //heif_debug_dump_box(h);

    cout << "----------------------------------------------" << endl;

    num_imgs = heif_get_number_of_images(h);

    cout << "number of images: " << num_imgs << endl;


    uint32_t idx_primary = heif_get_primary_image_index(h);

    cout << "primary image index: " << idx_primary << endl;


#if 0
    image_handle img_handle;
    err = heif_get_image_handle(h, idx_primary, &img_handle);

    cout << "primary image handle " << img_handle << endl;
#endif

    heif_debug_dump_image_info(h, idx_primary);



    heif_image *image_data = create_heif_image_buffer();
    //memset(&image_data, 0x0, sizeof(heif_image));
    err = heif_get_image_compressed_data(h, idx_primary, image_data);

    if(0 != err.code) {
        std::cerr << "Can not get HEIF image data " << err.message << endl;

        return 0;
    }

    cout << "tiles count: " << image_data->tiles_count  << endl;
    cout << "tile rows: " << image_data->tile_rows << " columns: " << image_data->tile_columns << endl;
    //cout << "tile width: " << image_data.tile_info->width << " height: " << image_data.tile_info->height << endl;

    cout << "tile data len: " << image_data->arr_tile_data_len[0] << endl;
    cout << "tile data: " << image_data->arr_tile_data[0] << endl;


#if 0
    for(int i = 0; i < image_data->tiles_count; i++) {
        string filename;
        std::ostringstream s;
        s << "tile-" << i << ".hevc";
        filename.assign(s.str());
        FILE *ofile = fopen(filename.c_str(), "wb");
        fwrite(image_data->arr_tile_data[i], 1, image_data->arr_tile_data_len[i], ofile);
        fclose(ofile);
    }


    cout << "save hevc finsh" << endl;
#endif

    // TODO：

    int width = image_data->info.width;
    int height = image_data->info.height;





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

    dstrect.x = 0;
    dstrect.y = 0;
    dstrect.w = 800;
    dstrect.h = 600;

    if(SDL_Init(SDL_INIT_VIDEO) < 0) {
        cout << "SDL can not init, err: " << SDL_GetError() << endl;
        return 0;
    }

    //
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

    window = SDL_CreateWindow("HEIF player",
            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
            800, 600, SDL_WINDOW_SHOWN);

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

    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_STREAMING, width, height);
    if(NULL == texture) {
        cout << "SDL texture can not be create, err: " << SDL_GetError() << endl;
        return 0;
    }
    
#endif




    const struct de265_image *img = 0;
    de265_decoder_context *dec_ctx = heif_create_hevc_decoder();

    for(int i = 0; i < image_data->tiles_count; i++) {
    //for(int i = image_data->tiles_count - 1; i >= 0 ; i--) {
        // decoding every tiles
        img = heif_decode_hevc_image(dec_ctx, image_data->arr_tile_data[i], image_data->arr_tile_data_len[i], 0, 0);

        if(img) {
            // copy tiles data to big picture
            merge_tile_to_heif_image(img, i, image_data);

#if DISPLAY_BY_SDL

            int stride = width, chroma_stride = width / 2;
            const uint8_t* y = image_data->heif_yuv_data;
            const uint8_t* cb = y + width * height;
            const uint8_t* cr = cb + width*height / 4;

            //printf("y=%p, stride=%d, cb = %p, cr = %p, chroma_stride = %d\n", y, stride, cb, cr, chroma_stride);
            SDL_UpdateYUVTexture(texture, NULL, 
                                y, stride,
                                cb, chroma_stride,
                                cr, chroma_stride);

            //SDL_UpdateTexture(texture, NULL, image_data->heif_yuv_data, width);

            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, texture, &srcrect, &dstrect);
            SDL_RenderPresent(renderer);

        }


        heif_reset_hevc_decoder(dec_ctx);
        de265_release_next_picture(dec_ctx);
    }

#endif
    std::string jpg_name = in_filename + ".jpg";
    //char * jpeg_name = (char*)"heif2jpeg.jpg";
    convert_yuv420_to_jpeg(image_data->heif_yuv_data, width, height, 80, jpg_name.c_str());

    // show picture in sdl


#if DISPLAY_BY_SDL
    for(;;) {
        SDL_WaitEvent(&event);
        if(event.type == SDL_QUIT) {
            break;
        }
        else if(event.type == SDL_KEYDOWN) {
            if(event.key.keysym.sym == SDLK_ESCAPE) {
                break;
            }
        }
    }


    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
#endif


    // free heif_image
    heif_destroy_hevc_decoder(dec_ctx);

    destroy_heif_image_buffer(image_data);
    heif_handle_free(h);

    return 0;
}