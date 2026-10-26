// Actual GLES2/3: asynchronous replacements, resident reuse and dynamic video.
#include "marni/MarniDX.h"
#include "marni/MarniGLFuncs.h"
#include <SDL2/SDL.h>
#include <emscripten.h>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <vector>
extern "C" void re1_web_presented(int) {}
extern "C" float re1_web_layout(float* shift,unsigned* font,int* itemBox){*itemBox=0;*shift=0;*font=0;return 0;}
extern "C" unsigned re1_web_interpolation_scene(){return 0;}
extern "C" int re1_web_interpolation_wait_ms(){return 0;}
extern "C" void re1_web_hd_dynamic(int);
EM_JS(void, fixture,(const unsigned char* data,int size),{
 globalThis.hdBytes=HEAPU8.slice(data,data+size);globalThis.hdReady=false;globalThis.hdPending=false;
 globalThis.RE1HDPack={active:true,hits:0,misses:0,requests:0,size:()=>hdBytes.length,
 request:()=>{if(!hdPending){hdPending=true;RE1HDPack.requests++;}},pixels:()=>hdReady?hdBytes.length:0,
 take:(key,dest)=>{dest.set(hdBytes);hdReady=false;hdPending=false;return true;}};
});
static void pixel(MarniDX& dx,MarniHandle tex,unsigned char r,unsigned char g,unsigned char b){
 dx.Clear(0,0,0,1);dx.DrawSprite(0,0,64,64,0,0,1,1,0xffffffff,tex,MARNI_SAMPLER_POINT,MARNI_BLEND_DISABLE);
 void* raw=nullptr;DWORD w=0,h=0;assert(dx.CaptureBackbufferToRGBA(&raw,&w,&h));
 auto* p=static_cast<unsigned char*>(raw)+(32*w+32)*4;assert(p[0]==r&&p[1]==g&&p[2]==b);free(raw);
}
int main(int argc,char** argv){
 const bool compat=argc>1&&argv[1][0]=='2';if(compat)setenv("RE1_WEB_GLES","2",1);
 assert(SDL_Init(SDL_INIT_VIDEO)==0);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,compat?2:3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,0);SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES);
 auto* window=SDL_CreateWindow("HD test",0,0,64,64,SDL_WINDOW_OPENGL);assert(window);auto context=SDL_GL_CreateContext(window);assert(context);
 assert(MarniGL_LoadFunctions((void*(*)(const char*))SDL_GL_GetProcAddress));
 {
  MarniDX dx;assert(dx.Create((HWND)window,64,64,FALSE,nullptr,nullptr));
  std::vector<unsigned char> packed(32+18*18*4,0);
  for(int i=0;i<18*18;i++){packed[32+i*4+1]=255;packed[32+i*4+3]=255;}
  memcpy(packed.data(),"R1HD",4);unsigned header[]={2,4,4,18,18,18*18*4,0};memcpy(packed.data()+4,header,sizeof(header));fixture(packed.data(),packed.size());
  unsigned char red[4*4*4];for(int i=0;i<16;i++){red[i*4]=255;red[i*4+1]=0;red[i*4+2]=0;red[i*4+3]=255;}
  auto tex=dx.CreateTexture(4,4,32,red,nullptr,nullptr);assert(tex);pixel(dx,tex,255,0,0);
  // A worker reply makes the replacement available on the next frame.
  EM_ASM({hdReady=true;});pixel(dx,tex,0,255,0);
  int w,h;dx.GetTextureSize(tex,&w,&h);assert(w==4&&h==4);dx.DestroyTexture(tex);
  tex=dx.CreateTexture(4,4,32,red,nullptr,nullptr);pixel(dx,tex,0,255,0);
  assert(EM_ASM_INT({return RE1HDPack.requests===1&&RE1HDPack.reused===1?1:0;}));
  // Shared immutable slots survive either destruction order and sampler changes.
  auto shared=dx.CreateTexture(4,4,32,red,nullptr,nullptr);dx.DestroyTexture(tex);pixel(dx,shared,0,255,0);dx.DestroyTexture(shared);
  // Invalid worker dimensions never upload. Keep original pixels immediately.
  packed[16]=19;fixture(packed.data(),packed.size());red[0]=254;
  tex=dx.CreateTexture(4,4,32,red,nullptr,nullptr);EM_ASM({hdReady=true;});pixel(dx,tex,255,0,0);dx.DestroyTexture(tex);
  // Artist-authored mod textures use their own size, not the AI 4.5x size.
  packed.assign(32+8*8*4,0);memcpy(packed.data(),"R1HD",4);
  unsigned modHeader[]={3,4,4,8,8,8*8*4,0};memcpy(packed.data()+4,modHeader,sizeof(modHeader));
  for(int i=0;i<8*8;i++){packed[32+i*4+1]=255;packed[32+i*4+3]=255;}
  fixture(packed.data(),packed.size());red[0]=253;
  tex=dx.CreateTexture(4,4,32,red,nullptr,nullptr);EM_ASM({hdReady=true;});pixel(dx,tex,0,255,0);
  dx.GetTextureSize(tex,&w,&h);assert(w==4&&h==4);dx.DestroyTexture(tex);
  // Invalid version-3 dimensions are rejected before a GL upload as well.
  packed[16]=9;fixture(packed.data(),packed.size());red[0]=252;
  tex=dx.CreateTexture(4,4,32,red,nullptr,nullptr);EM_ASM({hdReady=true;});pixel(dx,tex,255,0,0);dx.DestroyTexture(tex);
  // TeamX's narrow pages retain every HD texel; logical UV dimensions stay
  // square here even when the imported texture is physically rectangular.
  packed.assign(32+8*12*4,0);memcpy(packed.data(),"R1HD",4);
  unsigned pageHeader[]={4,4,4,8,12,8*12*4,0};memcpy(packed.data()+4,pageHeader,sizeof(pageHeader));
  for(int y=0;y<12;y++)for(int x=0;x<8;x++){packed[32+(y*8+x)*4+(x<4?1:2)]=255;packed[32+(y*8+x)*4+3]=255;}
  fixture(packed.data(),packed.size());red[0]=251;tex=dx.CreateTexture(4,4,32,red,nullptr,nullptr);EM_ASM({hdReady=true;});
  dx.Clear(0,0,0,1);dx.DrawSprite(0,0,64,64,0,0,1,1,0xffffffff,tex,MARNI_SAMPLER_POINT,MARNI_BLEND_DISABLE);
  void* pagePixels=nullptr;DWORD pw=0,ph=0;assert(dx.CaptureBackbufferToRGBA(&pagePixels,&pw,&ph));
  auto* pp=static_cast<unsigned char*>(pagePixels);assert(pp[(32*pw+16)*4+1]==255&&pp[(32*pw+48)*4+2]==255);free(pagePixels);
  dx.GetTextureSize(tex,&w,&h);assert(w==4&&h==4);dx.DestroyTexture(tex);
  // Video textures bypass the replacement cache and remain mutable.
  re1_web_hd_dynamic(1);tex=dx.CreateTexture(4,4,32,red,nullptr,nullptr);re1_web_hd_dynamic(0);assert(tex);
  unsigned char blue[64];for(int i=0;i<16;i++){blue[i*4]=0;blue[i*4+1]=0;blue[i*4+2]=255;blue[i*4+3]=255;}
  assert(dx.UpdateTexturePixels(tex,blue,4,4,32));pixel(dx,tex,0,0,255);dx.DestroyTexture(tex);
  // Dynamic inventory pages resolve only the drawn 40x30 icon, preserving
  // page UVs and avoiding an HD atlas for every inventory combination.
  packed.assign(32+80*60*4,0);memcpy(packed.data(),"R1HD",4);
  unsigned iconHeader[]={3,40,30,80,60,80*60*4,0};memcpy(packed.data()+4,iconHeader,sizeof(iconHeader));
  for(int i=0;i<80*60;i++){packed[32+i*4+1]=255;packed[32+i*4+3]=255;}
  fixture(packed.data(),packed.size());
  EM_ASM({RE1HDPack.regionTiles=true;RE1HDPack.size=key=>key.endsWith('_40x30')?hdBytes.length:0;
    const request=RE1HDPack.request;RE1HDPack.request=key=>{globalThis.regionKey=key;request(key);};});
  std::vector<unsigned char> atlas(256*256*4,0);
  for(int i=0;i<256*256;i++)atlas[i*4+3]=255;
  for(int y=50;y<80;y++)for(int x=20;x<60;x++)atlas[(y*256+x)*4]=255;
  tex=dx.CreateTexture(256,256,32,atlas.data(),nullptr,nullptr);assert(tex);
  auto iconPixel=[&](unsigned char r,unsigned char g,unsigned char b){
    dx.Clear(0,0,0,1);dx.DrawSprite(0,0,64,64,20.f/256,50.f/256,60.f/256,80.f/256,0xffffffff,tex,MARNI_SAMPLER_POINT,MARNI_BLEND_DISABLE);
    void* image=nullptr;DWORD iw=0,ih=0;assert(dx.CaptureBackbufferToRGBA(&image,&iw,&ih));
    auto* center=static_cast<unsigned char*>(image)+(32*iw+32)*4;assert(center[0]==r&&center[1]==g&&center[2]==b);free(image);
  };
  iconPixel(255,0,0);EM_ASM({hdReady=true;});iconPixel(0,255,0);
  pixel(dx,tex,0,0,0); // Other regions of the composite page remain original.
  dx.GetTextureSize(tex,&w,&h);assert(w==256&&h==256);
  EM_ASM({RE1HDPack.size=key=>key===regionKey?hdBytes.length:0;});
  for(int y=50;y<80;y++)for(int x=20;x<60;x++){atlas[(y*256+x)*4]=0;atlas[(y*256+x)*4+2]=255;}
  assert(dx.UpdateTexturePixels(tex,atlas.data(),256,256,32));iconPixel(0,0,255);dx.DestroyTexture(tex);
 }
 SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();return 0;
}
