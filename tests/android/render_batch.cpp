// Asset-free pixel regression for the actual Android GL backend.
#include "marni/MarniDX.h"
#include "marni/MarniGLFuncs.h"
#include <SDL2/SDL.h>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <emscripten.h>

extern "C" void re1_web_presented(int) {}
extern "C" float re1_web_layout(float* shift,unsigned* font,int* itemBox) { *itemBox=0;*shift=0;*font=0;return 0; }
extern "C" unsigned re1_web_interpolation_scene() { return 0; }
extern "C" int re1_web_interpolation_wait_ms() { return 0; }

static void capture(MarniDX& dx, const char* name, int x, int y, int r, int g, int b)
{
    void* buffer=nullptr; DWORD w=0,h=0;
    assert(dx.CaptureBackbufferToRGBA(&buffer,&w,&h));
    auto* p=static_cast<unsigned char*>(buffer)+(y*w+x)*4;
    if (std::abs(int(p[0])-r)>2 || std::abs(int(p[1])-g)>2 || std::abs(int(p[2])-b)>2) {
        printf("FAIL: %s pixel %d,%d: %u,%u,%u expected %d,%d,%d\n",name,x,y,p[0],p[1],p[2],r,g,b);
        abort();
    }
    char path[128];snprintf(path,sizeof(path),"/%s.rgba",name);
    FILE* f=fopen(path,"wb");assert(f);fwrite(buffer,1,w*h*4,f);fclose(f);free(buffer);
}

static void sprite(MarniDX& dx, float x, MarniHandle tex, MarniSampler sampler=MARNI_SAMPLER_POINT)
{
    dx.DrawSprite(x,0,16,16,.5f,.5f,.5f,.5f,0xffffffff,tex,sampler,MARNI_BLEND_DISABLE);
}

static void square3D(MarniDX& dx, float depth, float r, float g, float b, float alpha, bool write,MarniHandle tex=MARNI_NULL_HANDLE)
{
    const float v[]={
        0,0,depth,1,0,0,r,g,b,alpha, 64,0,depth,1,0,0,r,g,b,alpha, 0,64,depth,1,0,0,r,g,b,alpha,
        0,64,depth,1,0,0,r,g,b,alpha, 64,0,depth,1,0,0,r,g,b,alpha, 64,64,depth,1,0,0,r,g,b,alpha};
    if(write)dx.DrawTriangles3D(v,2,tex,MARNI_SAMPLER_POINT,MARNI_BLEND_DISABLE,true);
    else dx.DrawTrianglesPersp(v,2,tex,MARNI_SAMPLER_POINT,MARNI_BLEND_ALPHA,true);
}

int main(int argc, char** argv)
{
    const bool compat=argc>1 && argv[1][0]=='2';
    if(compat)setenv("RE1_WEB_GLES","2",1);
    assert(SDL_Init(SDL_INIT_VIDEO)==0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,compat?2:3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,24);
    SDL_Window* window=SDL_CreateWindow("Pixel regressions",0,0,64,64,SDL_WINDOW_OPENGL|SDL_WINDOW_SHOWN);
    assert(window);SDL_GLContext ctx=SDL_GL_CreateContext(window);assert(ctx);
    assert(MarniGL_LoadFunctions((void*(*)(const char*))SDL_GL_GetProcAddress));
    {
        MarniDX dx;assert(dx.Create((HWND)window,64,64,FALSE,nullptr,nullptr));
        dx.Clear(0,0,0,1);
        dx.DrawRect(0,0,64,64,0xff0000ff);
        dx.DrawRect(0,0,64,64,0x80ff0000);
        dx.DrawRect(0,0,32,64,0x8000ff00);
        capture(dx,"alpha-order",16,16,64,128,63);

        const unsigned char red[]={255,0,0,255},blue[]={0,0,255,255};
        MarniHandle tex=dx.CreateTexture(1,1,32,red,nullptr,nullptr);
        dx.Clear(0,0,0,1);sprite(dx,0,tex);
        assert(dx.UpdateTexturePixels(tex,blue,1,1,32));sprite(dx,16,tex);
        dx.DestroyTexture(tex);

        capture(dx,"texture-mutation",8,8,255,0,0);
        capture(dx,"texture-delete",24,8,0,0,255);

        const unsigned char stripes[]={255,0,0,255,0,255,0,255};
        tex=dx.CreateTexture(2,1,32,stripes,nullptr,nullptr);
        dx.Clear(0,0,0,1);
        sprite(dx,0,tex,MARNI_SAMPLER_LINEAR);sprite(dx,16,tex);sprite(dx,32,tex,MARNI_SAMPLER_LINEAR);
        capture(dx,"filter-linear",8,8,128,128,0);
        capture(dx,"filter-point",24,8,0,255,0);
        capture(dx,"filter-return",40,8,128,128,0);
        dx.DestroyTexture(tex);

        dx.Clear(0,0,0,1);
        for(int i=0;i<5000;++i)dx.DrawRect(0,0,64,64,i==4999?0xff00ff00:0xffff0000);
        dx.Present();capture(dx,"large-batch",32,32,0,255,0);

        dx.Clear(0,0,0,1);
        square3D(dx,.7f,0,0,1,1,true);square3D(dx,.3f,1,0,0,1,true);square3D(dx,.5f,0,1,0,1,true);
        capture(dx,"depth-order",32,32,255,0,0);
        square3D(dx,.3f,1,1,0,.5f,false);
        capture(dx,"coplanar-alpha",32,32,255,128,0);
        dx.Present();
        // Empty effect texels may skip blending only when they cannot alter
        // depth or color. Colored alpha-zero additive/replace pixels still
        // contribute, and alpha 1/255 must never be thresholded away.
        const unsigned char clearRed[]={255,0,0,0},faintGreen[]={0,255,0,1};
        auto empty=dx.CreateTexture(1,1,32,clearRed,nullptr,nullptr);
        dx.Clear(0,0,1,1);
        dx.DrawSprite(0,0,64,64,0,0,1,1,0xffffffff,empty,MARNI_SAMPLER_POINT,MARNI_BLEND_ALPHA);
        capture(dx,"empty-alpha",32,32,0,0,255);
        dx.Clear(0,0,1,1);
        dx.DrawSprite(0,0,64,64,0,0,1,1,0xffffffff,empty,MARNI_SAMPLER_POINT,MARNI_BLEND_ADD);
        capture(dx,"empty-additive",32,32,255,0,255);
        dx.Clear(0,0,1,1);
        dx.DrawSprite(0,0,64,64,0,0,1,1,0xffffffff,empty,MARNI_SAMPLER_POINT,MARNI_BLEND_DISABLE);
        capture(dx,"empty-replace",32,32,255,0,0);
        dx.Clear(0,0,1,1);
        const float transparentDepth[]={
            0,0,.2f,1,0,0,1,1,1,1,64,0,.2f,1,1,0,1,1,1,1,0,64,.2f,1,0,1,1,1,1,1,
            0,64,.2f,1,0,1,1,1,1,1,64,0,.2f,1,1,0,1,1,1,1,64,64,.2f,1,1,1,1,1,1,1};
        dx.DrawTriangles3D(transparentDepth,2,empty,MARNI_SAMPLER_POINT,MARNI_BLEND_ALPHA,true);
        square3D(dx,.5f,0,1,0,1,true);
        capture(dx,"empty-depth",32,32,0,0,255);
        assert(dx.UpdateTexturePixels(empty,faintGreen,1,1,32));
        dx.Clear(0,0,1,1);
        dx.DrawSprite(0,0,64,64,0,0,1,1,0xffffffff,empty,MARNI_SAMPLER_POINT,MARNI_BLEND_ALPHA);
        capture(dx,"faint-alpha",32,32,0,1,254);dx.DestroyTexture(empty);
        // Only the sampled opaque part of an atlas may use full writes.
        // Holes, faded vertices, linear filtering and updates keep blending.
        std::vector<unsigned char> atlas(64*32*4,0);
        for(int y=0;y<32;y++)for(int x=32;x<64;x++){atlas[(y*64+x)*4+1]=255;atlas[(y*64+x)*4+3]=255;}
        auto coverage=dx.CreateTexture(64,32,32,atlas.data(),nullptr,nullptr);
        EM_ASM({const gl=GL.currentContext.GLctx;const draw=gl.drawArrays.bind(gl);window.coverageDraws=0;gl.drawArrays=(...a)=>{coverageDraws++;return draw(...a);};});
        dx.Clear(0,0,1,1);
        dx.DrawSprite(0,0,16,64,.75f,.5f,.75f,.5f,0xffffffff,coverage,MARNI_SAMPLER_POINT,MARNI_BLEND_ALPHA);
        dx.DrawSprite(16,0,16,64,.25f,.5f,.25f,.5f,0xffffffff,coverage,MARNI_SAMPLER_POINT,MARNI_BLEND_ALPHA);
        dx.DrawSprite(32,0,16,64,.75f,.5f,.75f,.5f,0x80ffffff,coverage,MARNI_SAMPLER_POINT,MARNI_BLEND_ALPHA);
        dx.DrawSprite(48,0,16,64,.25f,0,.75f,1,0xffffffff,coverage,MARNI_SAMPLER_LINEAR,MARNI_BLEND_ALPHA);
        void* coveragePixels=nullptr;DWORD cw,ch;
        assert(dx.CaptureBackbufferToRGBA(&coveragePixels,&cw,&ch));
        assert(EM_ASM_INT({return coverageDraws===2;})); // small mask tiles retain their original batching
        auto* cp=static_cast<unsigned char*>(coveragePixels);
        assert(cp[(32*cw+8)*4+1]==255&&cp[(32*cw+24)*4+2]==255);
        assert(cp[(32*cw+40)*4+1]==128&&cp[(32*cw+40)*4+2]==127);
        assert(cp[(32*cw+49)*4+2]==255&&cp[(32*cw+63)*4+1]==255);free(coveragePixels);
        for(int y=0;y<32;y++)for(int x=32;x<64;x++)atlas[(y*64+x)*4+3]=128;
        assert(dx.UpdateTexturePixels(coverage,atlas.data(),64,32,32));dx.Clear(0,0,1,1);
        dx.DrawSprite(0,0,64,64,.75f,.5f,.75f,.5f,0xffffffff,coverage,MARNI_SAMPLER_POINT,MARNI_BLEND_ALPHA);
        assert(dx.CaptureBackbufferToRGBA(&coveragePixels,&cw,&ch));cp=static_cast<unsigned char*>(coveragePixels);
        assert(cp[(32*cw+32)*4+1]==128&&cp[(32*cw+32)*4+2]==127);free(coveragePixels);dx.DestroyTexture(coverage);
        // A full-screen opaque backdrop must keep its single-texture,
        // unblended path even when transparent masks immediately follow it.
        auto backdrop=dx.CreateTexture(1,1,32,red,nullptr,nullptr);
        const unsigned char halfBlue[]={0,0,255,128};
        auto mask=dx.CreateTexture(1,1,32,halfBlue,nullptr,nullptr);
        dx.Clear(0,0,0,1);
        EM_ASM({
            const gl=GL.currentContext.GLctx;const draw=gl.drawArrays.bind(gl);
            window.backdropPasses=[];
            gl.drawArrays=(...args)=>{const program=gl.getParameter(gl.CURRENT_PROGRAM);backdropPasses.push([gl.isEnabled(gl.BLEND),gl.getUniformLocation(program,'uTex1')!==null]);return draw(...args);};
        });
        dx.DrawSprite(0,0,64,64,0,0,1,1,0xffffffff,backdrop,MARNI_SAMPLER_POINT,MARNI_BLEND_ALPHA);
        dx.DrawSprite(0,0,32,64,0,0,1,1,0xffffffff,mask,MARNI_SAMPLER_POINT,MARNI_BLEND_ALPHA);
        void* backdropPixels=nullptr;DWORD bw,bh;
        assert(dx.CaptureBackbufferToRGBA(&backdropPixels,&bw,&bh));
        auto* bp=static_cast<unsigned char*>(backdropPixels);
        assert(bp[(32*bw+16)*4]==127&&bp[(32*bw+16)*4+2]==128);
        assert(bp[(32*bw+48)*4]==255&&bp[(32*bw+48)*4+2]==0);free(backdropPixels);
        assert(EM_ASM_INT({return backdropPasses.length===2&&!backdropPasses[0][0]&&!backdropPasses[0][1]&&backdropPasses[1][0];}));
        dx.DestroyTexture(backdrop);dx.DestroyTexture(mask);
        // Four texture slots share one draw, and a fifth forces a capacity
        // flush. Both cases must preserve exact primitive and blending order.
        const unsigned char colors[5][4]={{255,0,0,255},{0,255,0,255},{0,0,255,255},{255,255,0,255},{255,0,255,255}};
        MarniHandle pages[5];for(int i=0;i<5;i++)pages[i]=dx.CreateTexture(1,1,32,colors[i],nullptr,nullptr);
        dx.Clear(0,0,0,1);
        for(int i=0;i<5;i++)dx.DrawSprite(i*12,0,12,64,0,0,1,1,0xffffffff,pages[i],MARNI_SAMPLER_POINT,MARNI_BLEND_DISABLE);
        capture(dx,"texture-pages",54,32,255,0,255);
        dx.Clear(0,0,0,1);
        for(int i=0;i<5;i++)dx.DrawSprite(0,0,64,64,0,0,1,1,0x80ffffff,pages[i],MARNI_SAMPLER_POINT,MARNI_BLEND_ALPHA);
        capture(dx,"texture-alpha-order",32,32,200,80,160);
        dx.Clear(0,0,0,1);
        square3D(dx,.2f,1,1,1,1,true,pages[0]);square3D(dx,.7f,1,1,1,1,true,pages[1]);
        square3D(dx,.5f,1,1,1,1,true,pages[2]);square3D(dx,.2f,1,1,1,1,true,pages[3]);
        square3D(dx,.9f,1,1,1,1,true,pages[4]);capture(dx,"texture-depth-order",32,32,255,255,0);
        for(auto page:pages)dx.DestroyTexture(page);

        // Alternating a full texture bank with a one-texture pass must retain
        // the unused complete samplers, without white/rebind churn.
        for(int i=0;i<4;i++)pages[i]=dx.CreateTexture(1,1,32,colors[i],nullptr,nullptr);
        dx.Clear(0,0,0,1);
        for(int i=0;i<4;i++)sprite(dx,i*12,pages[i]);
        dx.Present();
        EM_ASM({
            const gl=GL.currentContext.GLctx;
            const fn=gl.texParameteri.bind(gl);window.samplerChanges=0;gl.texParameteri=(...a)=>{samplerChanges++;return fn(...a);};
            const bind=gl.bindTexture.bind(gl);window.textureBinds=0;gl.bindTexture=(...a)=>{textureBinds++;return bind(...a);};
            const use=gl.useProgram.bind(gl);window.shaderKinds=new Set();
            gl.useProgram=program=>{if(program){let count=1;for(let i=1;i<4;i++)if(gl.getUniformLocation(program,'uTex'+i)!==null)count++;shaderKinds.add(count);for(const shader of gl.getAttachedShaders(program))if((gl.getShaderSource(shader)||String()).includes('discard'))throw Error('Fragment discard regression');}return use(program);};
        });
        dx.Clear(0,0,0,1);
        for(int repeat=0;repeat<20;repeat++){
            for(int i=0;i<4;i++)dx.DrawSprite(i*12,0,12,64,0,0,1,1,0xffffffff,pages[i],MARNI_SAMPLER_POINT,MARNI_BLEND_DISABLE);
            dx.DrawSprite(0,0,12,64,0,0,1,1,0x80ffffff,pages[0],MARNI_SAMPLER_POINT,MARNI_BLEND_ALPHA);
        }
        dx.Present();assert(EM_ASM_INT({return samplerChanges===0&&textureBinds===0;}));
        assert(EM_ASM_INT({return shaderKinds.has(1)&&shaderKinds.has(4);}));
        for(int count=2;count<=3;count++){
            dx.Clear(0,0,0,1);
            for(int i=0;i<count;i++)sprite(dx,i*12,pages[i]);
            void* pixels=nullptr;DWORD w,h;assert(dx.CaptureBackbufferToRGBA(&pixels,&w,&h));
            auto* rgba=static_cast<unsigned char*>(pixels);
            for(int i=0;i<count;i++)for(int c=0;c<4;c++)assert(rgba[(4*w+i*12+4)*4+c]==colors[i][c]);
            free(pixels);dx.Present();
        }
        assert(EM_ASM_INT({return shaderKinds.has(1)&&shaderKinds.has(2)&&shaderKinds.has(3)&&shaderKinds.has(4);}));
        // Deleting textures previously used on secondary units must leave
        // a single-texture draw valid too (WebGL completeness).
        for(int i=1;i<4;i++)dx.DestroyTexture(pages[i]);
        dx.Clear(0,0,0,1);sprite(dx,0,pages[0]);dx.Present();
        assert(glGetError()==GL_NO_ERROR);dx.DestroyTexture(pages[0]);

        // A busy blood/actor-style pass changes state between small draws.
        // Submit its vertices once, before any draw reads the GPU buffer.
        EM_ASM({
            window.streamStats=({uploads:0,updates:0,draws:0});
            const gl=GL.currentContext.GLctx;
            for(const [name,key] of [['bufferData','uploads'],['bufferSubData','updates'],['drawArrays','draws']]) {
                const fn=gl[name].bind(gl);gl[name]=(...args)=>{streamStats[key]++;return fn(...args);};
            }
        });
        dx.Clear(0,0,0,1);
        for(int i=0;i<80;i++)square3D(dx,.2f+i*.001f,1,0,0,1,i%2==0);
        dx.Present();
        assert(EM_ASM_INT({return streamStats.uploads===1&&streamStats.updates===0&&streamStats.draws===80;}));
    }
    SDL_GL_DeleteContext(ctx);SDL_DestroyWindow(window);SDL_Quit();
    puts("PASS: alpha order, texture update/delete, sampler changes, large batches, depth and coplanar transparency");
}
