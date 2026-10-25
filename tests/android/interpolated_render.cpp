// Actual GLES pixels: generated geometry must be between original positions,
// then the original endpoint must be restored without touching simulation.
#include "marni/MarniDX.h"
#include "marni/MarniGLFuncs.h"
#include <SDL2/SDL.h>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <emscripten.h>

static unsigned scene=1;
static int waitMs=16, originals=0, generated=0;
extern "C" unsigned re1_web_interpolation_scene(){return scene;}
extern "C" int re1_web_interpolation_wait_ms(){return waitMs;}
extern "C" float re1_web_layout(float* shift,unsigned* font,int* itemBox){*itemBox=0;*shift=0;*font=0;return 0;}
static bool red(int x){unsigned char c[4];glReadPixels(x,48,1,1,GL_RGBA,GL_UNSIGNED_BYTE,c);return c[0]>240&&c[2]<10;}
extern "C" void re1_web_presented(int gen){
 if(gen){
  assert(originals==1);
  if(!red(7)||red(11)){
   unsigned char pixels[8];glReadPixels(7,48,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixels);glReadPixels(11,48,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixels+4);
   printf("Midpoint pixels: %u,%u,%u / %u,%u,%u; GL error %u\n",pixels[0],pixels[1],pixels[2],pixels[4],pixels[5],pixels[6],glGetError());
   assert(false);
  }
  ++generated;
 }
 else {if(originals==1)assert(!red(7)&&red(11));++originals;}
}
static void draw(MarniDX& dx,float x){
 dx.Clear(0,0,1,1);
 const float v[]={x,8,.3f,1,0,0,1,0,0,1, x+4,8,.3f,1,1,0,1,0,0,1, x,24,.3f,1,0,1,1,0,0,1,
 x,24,.3f,1,0,1,1,0,0,1, x+4,8,.3f,1,1,0,1,0,0,1, x+4,24,.3f,1,1,1,1,0,0,1};
 const MarniTriangleKey keys[]={{1,2,0},{1,2,1}};
 dx.DrawTriangles3DWithKeys(v,2,MARNI_NULL_HANDLE,MARNI_SAMPLER_POINT,MARNI_BLEND_DISABLE,true,keys);
}
EM_JS(void, beginSamples,(),{
 const gl=GL.currentContext.GLctx;
 globalThis.effectQuery=gl.createQuery();gl.beginQuery(gl.ANY_SAMPLES_PASSED,effectQuery);
});
static int endSamples(){
 EM_ASM({const gl=GL.currentContext.GLctx;gl.endQuery(gl.ANY_SAMPLES_PASSED);gl.flush();});
 for(int i=0;i<120;i++){
  emscripten_sleep(5);
  const int visible=EM_ASM_INT({
   const gl=GL.currentContext.GLctx;
   if(!gl.getQueryParameter(effectQuery,gl.QUERY_RESULT_AVAILABLE))return -1;
   return gl.getQueryParameter(effectQuery,gl.QUERY_RESULT)?1:0;
  });
  if(visible>=0){EM_ASM({GL.currentContext.GLctx.deleteQuery(effectQuery);});return visible;}
 }
 EM_ASM({GL.currentContext.GLctx.deleteQuery(effectQuery);});return -1;
}
int main(int argc,char** argv){
 const bool compat=argc>1&&argv[1][0]=='2';if(compat)setenv("RE1_WEB_GLES","2",1);
 assert(SDL_Init(SDL_INIT_VIDEO)==0);
 SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,compat?2:3);
 SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES);
 SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,24);
 auto* window=SDL_CreateWindow("Geometry interpolation",0,0,64,64,SDL_WINDOW_OPENGL|SDL_WINDOW_SHOWN);
 assert(window);auto ctx=SDL_GL_CreateContext(window);assert(ctx);
 {
  MarniDX dx;assert(dx.Create((HWND)window,64,64,FALSE,nullptr,nullptr));
  int simulationTicks=0;
  ++simulationTicks;draw(dx,4);dx.Present();assert(originals==1&&generated==0);
  ++simulationTicks;draw(dx,8);dx.Present();assert(originals==2&&generated==1&&simulationTicks==2);
  emscripten_sleep(65); // compositor/GPU delay after the CPU budget check
  ++simulationTicks;draw(dx,9);dx.Present();assert(generated==1&&simulationTicks==3);
  const unsigned char pixel[]={0,255,0,255};auto tex=dx.CreateTexture(1,1,32,pixel,nullptr,nullptr);
  draw(dx,12);dx.Present();assert(generated==1); // texture change invalidates history
  scene=2;draw(dx,16);dx.Present();assert(generated==1); // camera cut
  waitMs=0;draw(dx,20);dx.Present();assert(generated==1); // insufficient frame budget
  waitMs=16;draw(dx,24);void* pixels=nullptr;DWORD w,h;
  assert(dx.CaptureBackbufferToRGBA(&pixels,&w,&h));free(pixels);dx.Present();assert(generated==1);
  dx.DestroyTexture(tex);
  if(!compat){
   // Verify actual fragment work, not just identical black/transparent pixels.
   // Empty alpha effects must produce no covered samples; colored additive
   // pixels with the same zero alpha must still reach the framebuffer.
   scene=0;const unsigned char emptyRed[]={255,0,0,0};
   tex=dx.CreateTexture(1,1,32,emptyRed,nullptr,nullptr);
   dx.Clear(0,0,1,1);beginSamples();
   dx.DrawSprite(0,0,64,64,0,0,1,1,0xffffffff,tex,MARNI_SAMPLER_POINT,MARNI_BLEND_ALPHA);
   dx.Present();assert(endSamples()==0);
   dx.Clear(0,0,1,1);beginSamples();
   dx.DrawSprite(0,0,64,64,0,0,1,1,0xffffffff,tex,MARNI_SAMPLER_POINT,MARNI_BLEND_ADD);
   dx.Present();assert(endSamples()==1);dx.DestroyTexture(tex);
  }
 }
 SDL_GL_DeleteContext(ctx);SDL_DestroyWindow(window);SDL_Quit();
 puts("PASS: generated midpoint pixels, exact endpoint, unchanged tick count; texture/camera/budget/readback fallback; GLES3 invisible-effect samples skipped");
}
