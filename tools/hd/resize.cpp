// Independent install-time neural worker. No inference runs during gameplay.
#include "platform/web/hd_neural.h"
#include <cstring>
#include <zlib.h>
#include <webp/decode.h>
#define STBI_NO_STDIO
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_MAX_DIMENSIONS 4096
#define STB_IMAGE_IMPLEMENTATION
#include "third_party/stb_image.h"
extern "C" {
int re1_mod_decode(const unsigned char* input,int bytes,int w,int h,unsigned char* output) {
    if(!input||!output||bytes<1||bytes>64*1024*1024||w<1||h<1||w>4096||h>4096||unsigned(w)>WebHD::MaxModPixels/unsigned(h))return 0;
    int width=0,height=0;
    if(WebPGetInfo(input,bytes,&width,&height)) {
        if(width!=w||height!=h)return 0;
        return WebPDecodeRGBAInto(input,bytes,output,unsigned(w)*h*4,w*4)?1:0;
    }
    int channels;
    if(!stbi_info_from_memory(input,bytes,&width,&height,&channels)||width!=w||height!=h)return 0;
    unsigned char* rgba=stbi_load_from_memory(input,bytes,&width,&height,&channels,4);
    if(!rgba)return 0;
    memcpy(output,rgba,unsigned(w)*h*4);stbi_image_free(rgba);return 1;
}
const char* re1_hd_key(const unsigned char* rgba,int w,int h) {
    static char key[64];int ow,oh;
    if(!rgba||!WebHD::Dimensions(w,h,ow,oh))return "";
    WebHD::Key(rgba,w,h,key);return key;
}
int re1_hd_resize(const unsigned char* rgba,int w,int h,unsigned char* output,float sharpness) {
    std::vector<unsigned char> result;int ow,oh;
    if(!output||!WebHDNeural::Upscale(rgba,w,h,result,ow,oh,sharpness))return 0;
    memcpy(output,result.data(),result.size());return 1;
}
int re1_hd_compress(const unsigned char* rgba,int bytes,unsigned char* output,int capacity) {
    if(!rgba||!output||bytes<1||unsigned(bytes)>WebHD::MaxPixels*4||capacity<1)return 0;
    uLongf size=capacity;
    // Lossless level 1 keeps exactly the same RGBA pixels and avoids spending
    // most of a phone-side build on the browser's fixed compression level.
    return compress2(output,&size,rgba,bytes,Z_BEST_SPEED)==Z_OK?int(size):0;
}
}
