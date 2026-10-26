// Port-owned HTML media bridge for installed Android MP4 replacements.
#include <emscripten.h>
static bool s_webVideo=false;
EM_JS(int, WebModVideoOpen, (const char* path,int to), {
    return globalThis.RE1ModVideo?.open(UTF8ToString(path),to)?1:0;
});
EM_JS(int, WebModVideo, (int command,int value), {
    const movie=globalThis.RE1ModVideo;if(!movie)return 0;
    switch(command){
        case 0:movie.close();break;
        case 1:movie.playFrom(value);break;
        case 2:movie.stop();break;
        case 3:return movie.isActive()?1:0;
        case 4:return movie.takeEnd()?1:0;
        case 5:movie.tick();break;
    }return 0;
});
