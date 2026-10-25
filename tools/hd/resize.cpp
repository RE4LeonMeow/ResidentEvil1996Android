// Independent install-time neural worker. No inference runs during gameplay.
#include "platform/web/hd_neural.h"
#include <cstring>
#include <zlib.h>
extern "C" {
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
