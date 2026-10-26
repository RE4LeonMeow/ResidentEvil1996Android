// Developer-only compatibility metadata helper. No game/texture bytes are output.
#include "platform/web/hd_image.h"
#include <cstring>
#ifdef _WIN32
#define RE1_PROFILE_EXPORT __declspec(dllexport)
#else
#define RE1_PROFILE_EXPORT
#endif
extern "C" RE1_PROFILE_EXPORT void re1_mod_profile_key(const unsigned char* pixels,int w,int h,char* output) {
    char key[64];WebHD::Key(pixels,w,h,key);std::strcpy(output,key);
}
