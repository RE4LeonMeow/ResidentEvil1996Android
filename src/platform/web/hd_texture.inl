// Worker-decoded image replacements. No synchronous storage reads, resizing,
// decompression or neural inference on the game/render thread.
#include "hd_image.h"
#include <map>
#include <string>
static bool s_hdDynamic=false;
extern "C" void re1_web_hd_dynamic(int dynamic) {s_hdDynamic=dynamic!=0;}
EM_JS(int, WebHDEnabled, (), {return globalThis.RE1HDPack?.active?1:0;});
EM_JS(int, WebHDSize, (const char* key), {return globalThis.RE1HDPack?.size(UTF8ToString(key))||0;});
EM_JS(void, WebHDRequest, (const char* key), {globalThis.RE1HDPack?.request(UTF8ToString(key));});
EM_JS(int, WebHDReady, (const char* key), {return globalThis.RE1HDPack?.pixels(UTF8ToString(key))||0;});
EM_JS(int, WebHDTake, (const char* key,unsigned char* dest,int length), {
    return globalThis.RE1HDPack?.take(UTF8ToString(key),HEAPU8.subarray(dest,dest+length))?1:0;
});
EM_JS(void, WebHDResult, (int hit), {if(globalThis.RE1HDPack){if(hit)RE1HDPack.hits++;else RE1HDPack.misses++;}});
EM_JS(void, WebHDReused, (), {if(globalThis.RE1HDPack)RE1HDPack.reused=(RE1HDPack.reused||0)+1;});
static unsigned WebHDU32(const unsigned char* p) {return unsigned(p[0])|unsigned(p[1])<<8|unsigned(p[2])<<16|unsigned(p[3])<<24;}
static bool WebHDPixels(const char* key,int w,int h,std::vector<unsigned char>& raw,int& ow,int& oh) {
    const int size=WebHDReady(key);
    if(size<=32||size>int(WebHD::MaxPixels*4+32)||!WebHD::Dimensions(w,h,ow,oh))return false;
    // Retain the upload scratch across camera changes; avoid repeatedly
    // allocating and zero-filling multi-megabyte image buffers.
    if(raw.capacity()<unsigned(size))raw.reserve(size);
    raw.resize(size);
    return WebHDTake(key,raw.data(),size)&&memcmp(raw.data(),"R1HD",4)==0&&
        WebHDU32(raw.data()+4)==2&&WebHDU32(raw.data()+8)==unsigned(w)&&WebHDU32(raw.data()+12)==unsigned(h)&&
        WebHDU32(raw.data()+16)==unsigned(ow)&&WebHDU32(raw.data()+20)==unsigned(oh)&&
        WebHDU32(raw.data()+24)==unsigned(ow)*oh*4&&WebHDU32(raw.data()+28)==0&&unsigned(size)==unsigned(ow)*oh*4+32;
}
