// Synthetic inventory: real GLES draws, complete wide framing, unchanged
// glyph/icon proportions, centered item model, and original-mode restoration.
#include "marni/MarniDX.h"
#include "marni/MarniGLFuncs.h"
#include <SDL2/SDL.h>
#include <cassert>
#include <cstdlib>
#include <cstdio>
static float extra=320.0f/3;
static float promptShift=0;
static unsigned promptFont=0;
static int boxLayout=0;
extern "C" float re1_web_layout(float* shift,unsigned* font,int* itemBox){*itemBox=boxLayout;*shift=promptShift;*font=promptFont;return extra;}
extern "C" unsigned re1_web_interpolation_scene(){return 0;}
extern "C" int re1_web_interpolation_wait_ms(){return 0;}
extern "C" void re1_web_presented(int){}
int main(int argc,char**argv){
 const bool compat=argc>1&&argv[1][0]=='2';if(compat)setenv("RE1_WEB_GLES","2",1);
 assert(SDL_Init(SDL_INIT_VIDEO)==0);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,compat?2:3);
 SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES);
 SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,24);
 auto* window=SDL_CreateWindow("Wide menu",0,0,320,240,SDL_WINDOW_OPENGL);assert(window);
 auto ctx=SDL_GL_CreateContext(window);assert(ctx);
 assert(MarniGL_LoadFunctions((void*(*)(const char*))SDL_GL_GetProcAddress));
 {
  MarniDX dx;assert(dx.Create((HWND)window,320,240,FALSE,nullptr,nullptr));dx.Clear(0,0,0,1);
  DWORD w,h;dx.GetBackBufferSize(&w,&h);assert(w==427&&h==240);
  const float sx=w/320.0f;
  // Border strip extends; glyph and sidebar item translate with no stretching.
  dx.DrawRect(int(32*sx),12,int(160*sx),8,0xffffffff);
  dx.DrawRect(int(32*sx),128,int(160*sx),12,0xffffffff);
  // A broad text-panel fill must cover the full widened lower frame.
  dx.DrawRect(int(8*sx),186,int(304*sx),30,0xff222222);
  // Action buttons and their highlight sit just left of the right column.
  const unsigned char white[]={255,255,255,255};
  auto action=dx.CreateTexture(1,1,32,white,nullptr,nullptr);assert(action);
  for(int row=0;row<3;row++)
   dx.DrawSprite(144*sx,57+24*row,48*sx,24,0,0,1,1,0xffffff00,action,MARNI_SAMPLER_POINT,MARNI_BLEND_DISABLE);
  // Real mask bounds draw after the actions, without covering the new space.
  dx.DrawRect(0,12,int(9*sx),128,0xff000000);
  dx.DrawRect(int(199*sx),12,int(121*sx),128,0xff000000);
  dx.DrawSprite(224*sx,88,8*sx,14,0,0,1,1,0xff00ff00,dx.WhiteTexture(),MARNI_SAMPLER_POINT,MARNI_BLEND_DISABLE);
  dx.DrawSprite(48*sx,186,8*sx,14,0,0,1,1,0xff0000ff,dx.WhiteTexture(),MARNI_SAMPLER_POINT,MARNI_BLEND_DISABLE);
  const float x=112*sx,y=76;
  const float verts[]={x-4*sx,y-4,.2f,1,0,0,1,0,0,1, x+4*sx,y-4,.2f,1,1,0,1,0,0,1, x,y+4,.2f,1,0,1,1,0,0,1};
  dx.DrawTriangles3D(verts,1,dx.WhiteTexture(),MARNI_SAMPLER_POINT,MARNI_BLEND_DISABLE,true);
  void* raw=nullptr;assert(dx.CaptureBackbufferToRGBA(&raw,&w,&h));auto* p=static_cast<unsigned char*>(raw);
  auto is=[&](int px,int py,int channel){return p[(py*w+px)*4+channel]>240;};
  assert(is(40,15,0)&&is(285,15,0)); // extended preview frame
  assert(is(40,135,0)&&is(285,135,0)); // complete lower frame, including original row 128
  assert(is(332,90,1)&&is(337,90,1)&&!is(340,90,1)); // 8-pixel glyph, correct right column
  assert(is(103,190,2)&&!is(112,190,2)); // description remains complete and centered
  assert(is(165,76,0)&&!is(112,76,0)); // model centered in widened preview
  assert(is(253,60,0)&&is(293,60,0)&&!is(220,60,0)); // right-anchored actions
  assert(is(253,115,0)&&is(293,115,0)); // Combine stays aligned and visible
  assert(p[(208*w+16)*4]==34&&p[(208*w+410)*4]==34); // no lower panel gaps
  free(raw);dx.Present();extra=0;dx.Clear(0,0,0,1);dx.GetBackBufferSize(&w,&h);assert(w==320&&h==240);
  dx.ChangeDisplayMode(1440,1080,FALSE);extra=320.0f/3;dx.Clear(0,0,0,1);
  dx.GetBackBufferSize(&w,&h);assert(w==1920&&h==1080);
  dx.DrawSprite(224*6,88*4.5f,8*6,14*4.5f,0,0,1,1,0xff00ff00,dx.WhiteTexture(),MARNI_SAMPLER_POINT,MARNI_BLEND_DISABLE);
  // Pixel snapping puts the last action edge at 581 instead of 580.5.
  dx.DrawSprite(144*6,473,48*6,108,0,0,1,1,0xffffff00,action,MARNI_SAMPLER_POINT,MARNI_BLEND_DISABLE);
  assert(dx.CaptureBackbufferToRGBA(&raw,&w,&h));p=static_cast<unsigned char*>(raw);
  assert(is(1490,405,1)&&is(1522,405,1)&&!is(1527,405,1));
  assert(is(1200,525,0)&&is(1320,525,0)&&!is(1000,525,0));free(raw);
  dx.Present();extra=0;dx.Clear(0,0,0,1);dx.GetBackBufferSize(&w,&h);assert(w==1440&&h==1080);
  dx.ChangeDisplayMode(320,240,FALSE);extra=320.0f/3;boxLayout=1;dx.Clear(0,0,0,1);
  dx.GetBackBufferSize(&w,&h);const float bx=w/320.f;
  dx.DrawSprite(41*bx,33,128*bx,1,0,0,1,1,0xffffffff,action,MARNI_SAMPLER_POINT,MARNI_BLEND_DISABLE);
  dx.DrawSprite(175*bx,64,8*bx,18,0,0,1,1,0xff00ff00,action,MARNI_SAMPLER_POINT,MARNI_BLEND_DISABLE);
  dx.DrawSprite(42*bx,50,8*bx,14,0,0,1,1,0xff0000ff,action,MARNI_SAMPLER_POINT,MARNI_BLEND_DISABLE);
  dx.DrawRect(int(42*bx),81,int(49*bx),15,0xff222222);
  dx.DrawSprite(224*bx,88,8*bx,14,0,0,1,1,0xffff0000,action,MARNI_SAMPLER_POINT,MARNI_BLEND_DISABLE);
  assert(dx.CaptureBackbufferToRGBA(&raw,&w,&h));p=static_cast<unsigned char*>(raw);
  assert(w==427&&h==240&&is(96,33,0)&&is(220,33,0)); // centered list frame
  assert(is(230,70,1)&&!is(283,70,1)); // box arrows stay with the list
  assert(is(97,52,2)&&!is(107,52,2)); // proportional names
  assert(p[(84*w+97)*4]==34&&is(333,90,0)); // masks/list and right inventory
  free(raw);dx.Present();boxLayout=2;dx.Clear(0,0,0,1);
  // A map/file page is one proportional coordinate system, including glyphs
  // beyond the old inventory sidebar boundary. Full-width fills still extend.
  dx.DrawRect(0,0,int(w),240,0xff222222);
  dx.DrawSprite(20*bx,30,280*bx,160,0,0,1,1,0xff0000ff,action,MARNI_SAMPLER_POINT,MARNI_BLEND_DISABLE);
  dx.DrawSprite(210*bx,60,8*bx,14,0,0,1,1,0xff00ff00,action,MARNI_SAMPLER_POINT,MARNI_BLEND_DISABLE);
  assert(dx.CaptureBackbufferToRGBA(&raw,&w,&h));p=static_cast<unsigned char*>(raw);
  assert(w==427&&p[(12*w+4)*4]==34&&p[(12*w+420)*4]==34);
  assert(is(75,40,2)&&is(350,40,2));
  assert(is(266,65,1)&&!is(319,65,1)); // no split-off last text columns
  free(raw);dx.Present();boxLayout=0;extra=0;
  promptShift=30;promptFont=action;dx.Clear(0,0,0,1);
  // Keep the world background and other low scene sprites fixed. Shift only
  // the real message font, including later lines, shadows and Yes/No.
  dx.DrawRect(0,0,320,240,0xff0000ff);
  dx.DrawSprite(40,186,8,14,0,0,1,1,0xff00ff00,action,MARNI_SAMPLER_POINT,MARNI_BLEND_DISABLE);
  dx.DrawSprite(56,202,8,14,0,0,1,1,0xff00ff00,action,MARNI_SAMPLER_POINT,MARNI_BLEND_DISABLE);
  dx.DrawSprite(72,186,8,14,0,0,1,1,0xffff0000,dx.WhiteTexture(),MARNI_SAMPLER_POINT,MARNI_BLEND_DISABLE);
  assert(dx.CaptureBackbufferToRGBA(&raw,&w,&h));p=static_cast<unsigned char*>(raw);
  assert(w==320&&h==240&&is(42,160,1)&&is(58,176,1));
  assert(is(42,190,2)&&is(74,190,0)&&is(200,200,2));free(raw);dx.Present();
 }
 SDL_GL_DeleteContext(ctx);SDL_DestroyWindow(window);SDL_Quit();puts("PASS: complete wide UI, proportional glyphs, centered preview, expanded border and original drawable restoration");
}
