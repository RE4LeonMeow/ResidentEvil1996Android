// MarniDX_GL.cpp - OpenGL implementation of the MarniDX backend interface.
//
// This is the Linux counterpart of MarniDX.cpp (Direct3D 11). It defines the
// same class, the same globals and the same opaque MarniHandle table, so every
// caller in src/game/ works unchanged. See docs/LINUX_PORT.md sections 3 and 6.
//
// Written against the GLES-3-compatible subset: core-profile VAOs/VBOs, no
// fixed-function state, no desktop-only entry points, so the body can be
// repointed at GLES for Android/Switch later.
//
// Phase 2 scope: context lifecycle, Clear/Present, texture management and the
// 2D quad/triangle path. DrawTrianglesPersp/DrawTriangles3D currently route to
// the affine 2D path; real depth and perspective-correct interpolation land in
// Phase 4 with the TMD renderer.
#include "MarniDX.h"
#include "MarniGLFuncs.h"

#include <SDL2/SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// DisplayModeInfo / D3DRendererInfo live with the rest of the shared types.
#include "../game/Types.h"

#define MARNI_MAX_TEXTURES 2048

// ---------------------------------------------------------------------------
// Backend state
// ---------------------------------------------------------------------------

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include "../platform/web/interpolation.h"
#include "../platform/web/menu_layout.h"
#include "../platform/web/texture_coverage.h"
extern "C" void re1_web_presented(int generated);
extern "C" float re1_web_layout(float* promptShift,unsigned* promptFont,int* itemBox);
extern "C" unsigned re1_web_interpolation_scene();
extern "C" int re1_web_interpolation_wait_ms();
#include "../platform/web/hd_texture.inl"
#endif
static void FlushBatch(MarniDX::Impl* p);
static void FlushVertexStream(MarniDX::Impl* p);
static void StopInterpolation(MarniDX::Impl* p);
static void DrawBatch(MarniDX::Impl* p,const float* verts,int vertexCount,MarniHandle tex,
    MarniSampler sampler,MarniBlend blend,bool depthTest,bool depthWrite,bool persp);

struct MarniDX::Impl {
    SDL_Window*   window = nullptr;
    SDL_GLContext ctx    = nullptr;
    int  width  = 0;
    int  height = 0;
    bool ready  = false;
    bool compat = false;
#ifdef __EMSCRIPTEN__
    // Only adjacent draws with identical state are combined, in submission
    // order. Bound the queue so room geometry cannot grow it without limit.
    static constexpr int kBatchVertices=12288;
    float pendingVertices[kBatchVertices*10];
    int pendingCount=0;
    MarniHandle pendingTexture=MARNI_NULL_HANDLE;
    MarniHandle pendingTextures[4]={};
    unsigned char pendingPages[kBatchVertices];
    int pendingTextureCount=0;
    std::vector<float> uploadVertices;
    MarniSampler pendingSampler=MARNI_SAMPLER_POINT;
    MarniBlend pendingBlend=MARNI_BLEND_ALPHA;
    bool pendingOpaque=false,pendingBackdrop=false;
    bool pendingDepthTest=false, pendingDepthWrite=false, pendingPersp=false;
    float menuExtra=0;
    int itemBoxLayout=0;
    float promptShift=0;
    unsigned promptFont=0;
    int normalWidth=0;
    std::vector<float> menuVertices;
    WebRenderInterpolation interpolation;
    bool replaying=false;
    unsigned interpolationCooldown=0;
    double frameStarted=0,lastInterpolatedTick=0;
    // Upload before drawing: subData between draws can repeatedly copy/wait
    // for the whole VBO on mobile drivers, even for non-overlapping ranges.
    struct StreamDraw {
        unsigned first,count;
        MarniHandle textures[4];
        int textureCount;
        MarniSampler sampler;
        MarniBlend blend;
        bool depthTest,depthWrite,perspective;
    };
    static constexpr unsigned kStreamVertices=49152,kStreamDraws=1024;
    std::vector<float> streamVertices;
    std::vector<StreamDraw> streamDraws;
    GLuint otherBound[3]={~GLuint(0),~GLuint(0),~GLuint(0)};
    struct Shader {
        GLuint program=0;
        GLint mvp=-1,tex=-1,persp=-1;
        GLint samplers[3]={-1,-1,-1};
        int lastPersp=-1;
    } shaders[4]; // exact sampler count, without fragment discards
    Shader* activeShader=nullptr;
#endif

    GLuint program = 0;
    GLuint vao     = 0;
    GLuint vbo     = 0;
    GLint  uMVP    = -1;
    GLint  uTex    = -1;
    GLint  uPersp  = -1;

    bool pipelineBound=false, geometryBound=false, projectionValid=false;
    GLuint boundTexture=~GLuint(0);
    int lastPersp=-1, lastBlend=-1, lastDepthTest=-1, lastDepthWrite=-1;
    float lastMVP[16]={};

    void ResetBindings() {
        pipelineBound=geometryBound=projectionValid=false;
        boundTexture=~GLuint(0);
#ifdef __EMSCRIPTEN__
        for(auto& tex:otherBound)tex=~GLuint(0);
        for(auto& shader:shaders)shader.lastPersp=-1;
#endif
        lastPersp=lastBlend=lastDepthTest=lastDepthWrite=-1;
    }

    MarniHandle whiteHandle = MARNI_NULL_HANDLE;
    MarniHandle fontHandle  = MARNI_NULL_HANDLE;
    int fontW = 0, fontH = 0;

#ifdef __EMSCRIPTEN__
    struct HDTexture {
        GLuint tex=0;
        unsigned bytes=0,refs=0,stamp=0;
        int w=0,h=0,filter=-1;
        bool ready=false;
        WebTextureCoverage coverage;
    };
#endif
    struct TexSlot {
        GLuint tex = 0;
        int w = 0, h = 0;
        int filter = -1;
#ifdef __EMSCRIPTEN__
        // std::map nodes remain stable while this slot holds a reference.
        HDTexture* hdImage = nullptr;
        WebTextureCoverage coverage;
#endif
    } slots[MARNI_MAX_TEXTURES];

#ifdef __EMSCRIPTEN__
    std::map<std::string,HDTexture> hdTextures;
    std::vector<unsigned char> hdUploadPixels;
    unsigned hdBytes=0,hdStamp=0;
    void TrimHD() {
        // Keep neighbouring cameras resident. Only idle textures are evicted;
        // live scene textures can temporarily exceed this 96 MiB soft budget.
        while(hdBytes>96u*1024*1024) {
            auto oldest=hdTextures.end();
            for(auto it=hdTextures.begin();it!=hdTextures.end();++it)
                if(!it->second.refs&&(oldest==hdTextures.end()||it->second.stamp<oldest->second.stamp))oldest=it;
            if(oldest==hdTextures.end())break;
            hdBytes-=oldest->second.bytes;glDeleteTextures(1,&oldest->second.tex);hdTextures.erase(oldest);
            // Unused sampler units may retain this resident image. Deletion
            // unbinds it, so refill those units before the next draw.
            boundTexture=~GLuint(0);for(auto& tex:otherBound)tex=~GLuint(0);
        }
    }
    bool UploadHD(const std::string& key,HDTexture& image) {
        auto& raw=hdUploadPixels;int w,h;
        if(!WebHDPixels(key.c_str(),image.w,image.h,raw,w,h))return false;
        image.coverage.analyze(raw.data()+32,w,h);
        glBindTexture(GL_TEXTURE_2D,image.tex);
        glTexImage2D(GL_TEXTURE_2D,0,compat?GL_RGBA:GL_RGBA8,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,raw.data()+32);
        glBindTexture(GL_TEXTURE_2D,0);boundTexture=~GLuint(0);
        hdBytes-=image.bytes;image.bytes=unsigned(w)*h*4;hdBytes+=image.bytes;
        image.ready=true;image.stamp=++hdStamp;WebHDResult(1);return true;
    }
    void PollHD() {
        // One ready upload per frame bounds main-thread work. Until its worker
        // reply arrives, the original texture is usable immediately.
        for(auto& pair:hdTextures)if(pair.second.refs&&!pair.second.ready) {
            // A bounded decoded queue may discard an unused early reply.
            // Request deduplication/backoff belongs to the worker bridge.
            WebHDRequest(pair.first.c_str());
            if(UploadHD(pair.first,pair.second))break;
        }
        TrimHD();
    }
#endif

    void ReleaseSlot(MarniHandle h) {
        if (h == MARNI_NULL_HANDLE || h >= MARNI_MAX_TEXTURES) return;
        FlushBatch(this);
        StopInterpolation(this);
        boundTexture=~GLuint(0);
#ifdef __EMSCRIPTEN__
        for(auto& tex:otherBound)tex=~GLuint(0);
#endif
#ifdef __EMSCRIPTEN__
        if(slots[h].hdImage) {
            --slots[h].hdImage->refs;slots[h].hdImage->stamp=++hdStamp;
            slots[h].hdImage=nullptr;TrimHD();
        }else
#endif
        if (slots[h].tex != 0) glDeleteTextures(1, &slots[h].tex);
        slots[h].tex = 0;
        slots[h].w = slots[h].h = 0;
        slots[h].filter = -1;
#ifdef __EMSCRIPTEN__
        slots[h].coverage={};
#endif
    }

    void ReleaseAll() {
        for (MarniHandle h = 1; h < MARNI_MAX_TEXTURES; ++h) ReleaseSlot(h);
#ifdef __EMSCRIPTEN__
        for(auto& pair:hdTextures)glDeleteTextures(1,&pair.second.tex);
        hdTextures.clear();hdBytes=0;
#endif
        if (vbo) { glDeleteBuffers(1, &vbo); vbo = 0; }
        if (vao) { glDeleteVertexArrays(1, &vao); vao = 0; }
#ifdef __EMSCRIPTEN__
        for(auto& shader:shaders){if(shader.program)glDeleteProgram(shader.program);shader={};}
        program=0;
#else
        if (program) { glDeleteProgram(program); program = 0; }
#endif
        whiteHandle = MARNI_NULL_HANDLE;
        fontHandle  = MARNI_NULL_HANDLE;
    }
};

static MarniDX* g_pDX = nullptr;

MarniDX* Marni_DX() { return g_pDX; }

// ---------------------------------------------------------------------------
// Shader / pipeline setup
// ---------------------------------------------------------------------------

static const char* kVertexSrc =
    #ifdef __EMSCRIPTEN__
    "#version 300 es\nprecision highp float;\nprecision highp int;\n"
#else
    "#version 330 core\n"
#endif
    "layout(location = 0) in vec2 aPos;\n"
    "layout(location = 1) in vec2 aUV;\n"
    "layout(location = 2) in vec4 aCol;\n"
    "layout(location = 3) in vec2 aZW;\n"      // x = depth [0,1], y = view-space w
    "uniform mat4 uMVP;\n"
    "uniform int  uPersp;\n"
    "out vec2 vUV;\n"
    "out vec4 vCol;\n"
#ifdef __EMSCRIPTEN__
    "layout(location = 4) in float aPage; flat out int vPage;\n"
#endif
    "void main() {\n"
    // GL's NDC z is [-1,1]; the callers pass normalised [0,1] depth.
    "    float z = aZW.x * 2.0 - 1.0;\n"
    "    vec4 p = uMVP * vec4(aPos, z, 1.0);\n"
    // Perspective-correct path: premultiply by w so the rasteriser's divide
    // restores the same screen position while interpolating UV/colour with 1/w.
    // Clamped like the Windows VS (max(input.pos.w, 1e-4)) so a degenerate
    // vertex cannot produce a NaN.
    "    if (uPersp != 0) {\n"
    "        float w = max(aZW.y, 1e-4);\n"
    "        p = vec4(p.xy * w, p.z * w, w);\n"
    "    }\n"
    "    gl_Position = p;\n"
    "    vUV = aUV;\n"
    "    vCol = aCol;\n"
#ifdef __EMSCRIPTEN__
    "    vPage = int(aPage);\n"
#endif
    "}\n";

static const char* kFragmentSrc =
    #ifdef __EMSCRIPTEN__
    "#version 300 es\nprecision highp float;\nprecision highp int;\n"
#else
    "#version 330 core\n"
#endif
    "in vec2 vUV;\n"
    "in vec4 vCol;\n"
    "uniform sampler2D uTex;\n"
#ifdef __EMSCRIPTEN__
    "uniform sampler2D uTex1; uniform sampler2D uTex2; uniform sampler2D uTex3; flat in int vPage;\n"
#endif
    "out vec4 oCol;\n"
    "void main() {\n"
#ifdef __EMSCRIPTEN__
    "    vec4 pixel; if(vPage==0)pixel=texture(uTex,vUV); else if(vPage==1)pixel=texture(uTex1,vUV); else if(vPage==2)pixel=texture(uTex2,vUV); else pixel=texture(uTex3,vUV);\n"
    "    oCol = pixel * vCol;\n"
#else
    "    oCol = texture(uTex, vUV) * vCol;\n"
#endif
    "}\n";

#ifdef __EMSCRIPTEN__
static const char* kCompatVertexSrc =
    "precision highp float;\n"
    "attribute vec2 aPos; attribute vec2 aUV; attribute vec4 aCol; attribute vec2 aZW; attribute float aPage;\n"
    "uniform mat4 uMVP; uniform int uPersp; varying mediump vec2 vUV; varying mediump vec4 vCol; varying mediump float vPage;\n"
    "void main() { float z = aZW.x * 2.0 - 1.0; vec4 p = uMVP * vec4(aPos, z, 1.0);\n"
    "if (uPersp != 0) { float w = max(aZW.y, 1e-4); p = vec4(p.xy*w, p.z*w, w); }\n"
    "gl_Position = p; vUV = aUV; vCol = aCol; vPage = aPage; }\n";
// Compile exactly the samplers this batch uses. A weapon/actor pair should
// not send every fragment through a four-texture branch on mobile drivers.
static std::string WebFragmentSource(bool compat,int textures)
{
    std::string source=compat?
        "precision mediump float; varying mediump vec2 vUV; varying mediump vec4 vCol;\n":
        "#version 300 es\nprecision highp float; precision highp int; in vec2 vUV; in vec4 vCol; out vec4 oCol;\n";
    if(textures>1)source+=compat?"varying mediump float vPage;\n":"flat in int vPage;\n";
    const char* names[]={"uTex","uTex1","uTex2","uTex3"};
    for(int i=0;i<textures;i++)source+="uniform sampler2D "+std::string(names[i])+";\n";
    source+="void main(){vec4 pixel;";
    for(int i=0;i<textures;i++){
        if(i)source+="else ";
        if(i+1<textures)source+=compat?"if(vPage<"+std::to_string(i)+".5)":"if(vPage=="+std::to_string(i)+")";
        source+="pixel="+std::string(compat?"texture2D(":"texture(")+names[i]+",vUV);";
    }
    source+=compat?"gl_FragColor=pixel*vCol;}":"oCol=pixel*vCol;}";
    return source;
}
#endif

static void SetVertexAttributes()
{
    const GLsizei stride =
#ifdef __EMSCRIPTEN__
        11 * sizeof(float);
#else
        10 * sizeof(float);
#endif
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(4 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, stride, (void*)(2 * sizeof(float)));
#ifdef __EMSCRIPTEN__
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4,1,GL_FLOAT,GL_FALSE,stride,(void*)(10*sizeof(float)));
#endif
}

static GLuint CompileShader(GLenum type, const char* src)
{
    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &src, nullptr);
    glCompileShader(sh);

    GLint ok = 0;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        GLsizei n = 0;
        glGetShaderInfoLog(sh, sizeof(log) - 1, &n, log);
        log[n] = '\0';
        fprintf(stderr, "[GL] shader compile failed: %s\n", log);
        glDeleteShader(sh);
        return 0;
    }
    return sh;
}

static GLuint LinkPipeline(const char* vertex,const char* fragment,bool compat)
{
    GLuint vs = CompileShader(GL_VERTEX_SHADER, vertex);
    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fragment);
    if (!vs || !fs) {if(vs)glDeleteShader(vs);if(fs)glDeleteShader(fs);return 0;}

    const GLuint program=glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
#ifdef __EMSCRIPTEN__
    if (compat) {
        glBindAttribLocation(program, 0, "aPos");
        glBindAttribLocation(program, 1, "aUV");
        glBindAttribLocation(program, 2, "aCol");
        glBindAttribLocation(program, 3, "aZW");
        glBindAttribLocation(program, 4, "aPage");
    }
#endif
    glLinkProgram(program);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        GLsizei n = 0;
        glGetProgramInfoLog(program, sizeof(log) - 1, &n, log);
        log[n] = '\0';
        fprintf(stderr, "[GL] program link failed: %s\n", log);
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

#ifdef __EMSCRIPTEN__
static void SelectPipeline(MarniDX::Impl* p,int textureCount)
{
    // Keep both actors and transparency effects on the pre-build-20 shader
    // path. Empty sprites are culled before submission instead of using kill
    // instructions that can disable mobile-driver depth/tile optimizations.
    auto& shader=p->shaders[textureCount-1];
    if(p->program==shader.program)return;
    p->program=shader.program;p->uMVP=shader.mvp;p->uTex=shader.tex;p->uPersp=shader.persp;
    p->activeShader=&shader;
    // Uniform values belong to the selected program. Texture, vertex, blend
    // and depth bindings belong to the context and remain valid on a switch.
    p->pipelineBound=false;p->lastPersp=shader.lastPersp;
}
#endif

static bool BuildPipeline(MarniDX::Impl* p)
{
#ifdef __EMSCRIPTEN__
    p->compat = getenv("RE1_WEB_GLES") && strcmp(getenv("RE1_WEB_GLES"), "2") == 0;
    for(int i=0;i<4;i++){
        auto& shader=p->shaders[i];
        const std::string fragment=WebFragmentSource(p->compat,i+1);
        shader.program=LinkPipeline(p->compat?kCompatVertexSrc:kVertexSrc,fragment.c_str(),p->compat);
        if(!shader.program)return false;
        shader.mvp=glGetUniformLocation(shader.program,"uMVP");
        shader.tex=glGetUniformLocation(shader.program,"uTex");
        shader.persp=glGetUniformLocation(shader.program,"uPersp");
        const char* names[]={"uTex1","uTex2","uTex3"};
        for(int j=0;j<3;j++)shader.samplers[j]=glGetUniformLocation(shader.program,names[j]);
        // Sampler assignments are constant for the lifetime of each program.
        glUseProgram(shader.program);glUniform1i(shader.tex,0);
        for(int j=0;j<3;j++)glUniform1i(shader.samplers[j],j+1);
    }
    SelectPipeline(p,1);
#else
    p->program=LinkPipeline(kVertexSrc,kFragmentSrc,p->compat);
    if(!p->program)return false;
    p->uMVP = glGetUniformLocation(p->program, "uMVP");
    p->uTex = glGetUniformLocation(p->program, "uTex");
    p->uPersp = glGetUniformLocation(p->program, "uPersp");
#endif

    if (!p->compat) { glGenVertexArrays(1, &p->vao); glBindVertexArray(p->vao); }
    glGenBuffers(1, &p->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, p->vbo);
    SetVertexAttributes();
    if (!p->compat) glBindVertexArray(0);
    p->ResetBindings();
    return true;
}

// ---------------------------------------------------------------------------
// Internal draw
// ---------------------------------------------------------------------------

static void ApplyBlend(MarniBlend blend)
{
    switch (blend) {
    case MARNI_BLEND_ADD:
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        break;
    case MARNI_BLEND_DISABLE:
        glDisable(GL_BLEND);
        break;
    case MARNI_BLEND_ALPHA:
    default:
        glEnable(GL_BLEND);
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        break;
    }
}

static void ApplySampler(MarniSampler sampler)
{
    GLint filter = (sampler == MARNI_SAMPLER_POINT) ? GL_NEAREST : GL_LINEAR;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
}

static void SubmitBatchGPU(MarniDX::Impl* p, const float* verts, int vertexCount,
                      MarniHandle tex, MarniSampler sampler, MarniBlend blend,
                      bool depthTest, bool depthWrite, bool persp
#ifdef __EMSCRIPTEN__
                      ,const MarniHandle* textures=nullptr,int textureCount=1,const unsigned char* pages=nullptr,int uploadedFirst=-1
#endif
                      )
{
    if (p == nullptr || !p->ready || p->program == 0 || vertexCount <= 0) return;
#ifdef __EMSCRIPTEN__
    SelectPipeline(p,textureCount);
#endif

    // Y-down ortho: (0,0) top-left -> NDC (-1,+1).
    float mvp[16] = {0};
    mvp[0]  = 2.0f / (float)p->width;
    mvp[5]  = -2.0f / (float)p->height;
    mvp[10] = 1.0f;
    mvp[12] = -1.0f;
    mvp[13] = 1.0f;
    mvp[15] = 1.0f;

    // This backend owns its context. Keep shader, geometry and render-state
    // bindings until they change; texture mutation explicitly invalidates its
    // binding below. Avoid thousands of redundant wasm/WebGL calls per second.
    if (!p->pipelineBound) {
        glUseProgram(p->program);
#ifndef __EMSCRIPTEN__
        glUniform1i(p->uTex, 0);
#endif
        glActiveTexture(GL_TEXTURE0);
        p->pipelineBound=true;
    }
    if (!p->projectionValid || memcmp(p->lastMVP,mvp,sizeof(mvp))!=0) {
#ifdef __EMSCRIPTEN__
        // The ortho projection depends only on drawable size. Update all
        // variants once on resize, not on every character/effect switch.
        for(const auto& shader:p->shaders){glUseProgram(shader.program);glUniformMatrix4fv(shader.mvp,1,GL_FALSE,mvp);}
        glUseProgram(p->program);
#else
        glUniformMatrix4fv(p->uMVP, 1, GL_FALSE, mvp);
#endif
        memcpy(p->lastMVP,mvp,sizeof(mvp));p->projectionValid=true;
    }
    if (p->lastPersp!=int(persp)) {
        glUniform1i(p->uPersp,persp?1:0);p->lastPersp=persp;
#ifdef __EMSCRIPTEN__
        p->activeShader->lastPersp=persp;
#endif
    }
    const int filter=sampler==MARNI_SAMPLER_POINT?GL_NEAREST:GL_LINEAR;
#ifdef __EMSCRIPTEN__
    int activeUnit=0;
    for(int unit=0;unit<4;unit++) {
        GLuint* bound=unit?&p->otherBound[unit-1]:&p->boundTexture;
        // All samplers must stay complete, but unused units can retain their
        // previous complete texture. Combat alternates full texture banks and
        // single-effect draws; rebinding white and restoring the bank wastes
        // driver work without changing any sampled pixel.
        if(unit>=textureCount&&*bound!=~GLuint(0))continue;
        const MarniHandle selected=unit<textureCount?(textures?textures[unit]:tex):MARNI_NULL_HANDLE;
#else
    {
        const MarniHandle selected=tex;
#endif
        MarniHandle textureSlot=p->whiteHandle;
        if(selected!=MARNI_NULL_HANDLE&&selected<MARNI_MAX_TEXTURES&&p->slots[selected].tex)textureSlot=selected;
        const GLuint texture=textureSlot!=MARNI_NULL_HANDLE?p->slots[textureSlot].tex:0;
#ifndef __EMSCRIPTEN__
        GLuint* bound=&p->boundTexture;
#endif
        int* currentFilter=textureSlot==MARNI_NULL_HANDLE?nullptr:&p->slots[textureSlot].filter;
#ifdef __EMSCRIPTEN__
        if(textureSlot!=MARNI_NULL_HANDLE&&p->slots[textureSlot].hdImage)currentFilter=&p->slots[textureSlot].hdImage->filter;
#endif
        // A 1x1 white texel is identical under either sampler. It also fills
        // unused texture units, so changing its filter adds no useful work.
        const bool changeFilter=textureSlot!=p->whiteHandle&&(!currentFilter||*currentFilter!=filter);
#ifdef __EMSCRIPTEN__
        if((*bound!=texture||changeFilter)&&activeUnit!=unit){glActiveTexture(GL_TEXTURE0+unit);activeUnit=unit;}
#endif
        if(*bound!=texture){glBindTexture(GL_TEXTURE_2D,texture);*bound=texture;}
        if(changeFilter){ApplySampler(sampler);if(currentFilter)*currentFilter=filter;}
    }
#ifdef __EMSCRIPTEN__
    if(activeUnit)glActiveTexture(GL_TEXTURE0);
#endif

    if (p->lastBlend!=int(blend)) { ApplyBlend(blend);p->lastBlend=blend; }
    if (p->lastDepthTest!=int(depthTest)) {
        if (depthTest) { glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL); }
        else glDisable(GL_DEPTH_TEST);
        p->lastDepthTest=depthTest;
    }
    const bool write=depthTest&&depthWrite;
    if (p->lastDepthWrite!=int(write)) {
        glDepthMask(write?GL_TRUE:GL_FALSE);p->lastDepthWrite=write;
    }

    if (!p->geometryBound) {
        if (!p->compat) glBindVertexArray(p->vao);
        glBindBuffer(GL_ARRAY_BUFFER, p->vbo);p->geometryBound=true;
    }
    // Both the default GLES 2 array state and the GLES 3 VAO retain the VBO
    // attribute bindings installed by BuildPipeline. No other backend uses it.
#ifdef __EMSCRIPTEN__
    if(uploadedFirst<0) {
        p->uploadVertices.resize(unsigned(vertexCount)*11);
        for(int v=0;v<vertexCount;v++) {
            memcpy(p->uploadVertices.data()+v*11,verts+v*10,10*sizeof(float));
            p->uploadVertices[v*11+10]=pages?pages[v]:0;
        }
        glBufferData(GL_ARRAY_BUFFER,unsigned(vertexCount)*11*sizeof(float),p->uploadVertices.data(),GL_STREAM_DRAW);
        uploadedFirst=0;
    }
    glDrawArrays(GL_TRIANGLES,uploadedFirst,vertexCount);
#else
    glBufferData(GL_ARRAY_BUFFER,(GLsizeiptr)(vertexCount*10*sizeof(float)),verts,GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES,0,vertexCount);
#endif
}

static void FlushVertexStream(MarniDX::Impl* p)
{
#ifdef __EMSCRIPTEN__
    if(!p||p->streamDraws.empty())return;
    if(!p->geometryBound) {
        if(!p->compat)glBindVertexArray(p->vao);
        glBindBuffer(GL_ARRAY_BUFFER,p->vbo);p->geometryBound=true;
    }
    glBufferData(GL_ARRAY_BUFFER,p->streamVertices.size()*sizeof(float),p->streamVertices.data(),GL_STREAM_DRAW);
    for(const auto& draw:p->streamDraws)SubmitBatchGPU(p,nullptr,draw.count,draw.textures[0],
        draw.sampler,draw.blend,draw.depthTest,draw.depthWrite,draw.perspective,
        draw.textures,draw.textureCount,nullptr,draw.first);
    p->streamVertices.clear();p->streamDraws.clear();
#endif
}

static void SubmitBatchDirect(MarniDX::Impl* p,const float* verts,int count,MarniHandle tex,
    MarniSampler sampler,MarniBlend blend,bool depthTest,bool depthWrite,bool perspective
#ifdef __EMSCRIPTEN__
    ,const MarniHandle* textures=nullptr,int textureCount=1,const unsigned char* pages=nullptr
#endif
    )
{
    if(!p||!p->ready||count<=0)return;
#ifdef __EMSCRIPTEN__
    if(p->streamVertices.size()/11+unsigned(count)>p->kStreamVertices||p->streamDraws.size()>=p->kStreamDraws)FlushVertexStream(p);
    if(unsigned(count)<=p->kStreamVertices) {
        p->streamVertices.reserve(p->kStreamVertices*11);p->streamDraws.reserve(p->kStreamDraws);
        const unsigned first=p->streamVertices.size()/11;
        MarniDX::Impl::StreamDraw draw={first,unsigned(count),{},textureCount,sampler,blend,depthTest,depthWrite,perspective};
        for(int i=0;i<textureCount;i++)draw.textures[i]=textures?textures[i]:tex;
        p->streamDraws.push_back(draw);p->streamVertices.resize((first+unsigned(count))*11);
        for(int v=0;v<count;v++) {
            float* out=p->streamVertices.data()+(first+v)*11;
            memcpy(out,verts+v*10,10*sizeof(float));out[10]=pages?pages[v]:0;
        }
        return;
    }
    SubmitBatchGPU(p,verts,count,tex,sampler,blend,depthTest,depthWrite,perspective,textures,textureCount,pages);
#else
    SubmitBatchGPU(p,verts,count,tex,sampler,blend,depthTest,depthWrite,perspective);
#endif
}

#ifdef __EMSCRIPTEN__
static void ReplayFrame(MarniDX::Impl* p, const WebRenderFrame& frame, const float* vertices)
{
    FlushVertexStream(p);
    glViewport(0,0,p->width,p->height);
    glClearColor(frame.clear[0],frame.clear[1],frame.clear[2],frame.clear[3]);
    glDepthMask(GL_TRUE);p->lastDepthWrite=1;
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    p->replaying=true;
    for(const auto& draw:frame.draws)DrawBatch(p,vertices+draw.offset,draw.count,
        draw.texture,MarniSampler(draw.sampler),MarniBlend(draw.blend),
        draw.depthTest,draw.depthWrite,draw.perspective);
    FlushBatch(p);FlushVertexStream(p);p->replaying=false;
}
#endif

static void StopInterpolation(MarniDX::Impl* p)
{
#ifdef __EMSCRIPTEN__
    if(!p)return;
    auto& frames=p->interpolation;
    // Resolve deferred draws before texture mutation, readback or resize.
    if(frames.recording)ReplayFrame(p,frames.current,frames.current.vertices.data());
    frames.invalidate();
    FlushVertexStream(p);
#endif
}

static void SubmitBatch(MarniDX::Impl* p, const float* verts, int vertexCount,
                      MarniHandle tex, MarniSampler sampler, MarniBlend blend,
                      bool depthTest, bool depthWrite, bool persp)
{
#ifdef __EMSCRIPTEN__
    if(p && p->interpolation.recording&&!p->replaying){
        if(p->interpolation.append(verts,vertexCount,tex,sampler,blend,depthTest,depthWrite,persp))return;
        StopInterpolation(p); // bounded capture overflow: render normally
    }
#endif
    SubmitBatchDirect(p,verts,vertexCount,tex,sampler,blend,depthTest,depthWrite,persp);
}

static void FlushBatch(MarniDX::Impl* p)
{
#ifdef __EMSCRIPTEN__
    if (!p || !p->pendingCount) return;
    const int count=p->pendingCount;
    const MarniBlend blend=p->pendingBlend==MARNI_BLEND_ALPHA&&p->pendingOpaque?MARNI_BLEND_DISABLE:p->pendingBlend;
    p->pendingCount=0;
    if(p->pendingTextureCount>1) {
        SubmitBatchDirect(p,p->pendingVertices,count,p->pendingTexture,p->pendingSampler,
            blend,p->pendingDepthTest,p->pendingDepthWrite,p->pendingPersp,
            p->pendingTextures,p->pendingTextureCount,p->pendingPages);
        return;
    }
    SubmitBatch(p, p->pendingVertices, count, p->pendingTexture, p->pendingSampler,
        blend, p->pendingDepthTest, p->pendingDepthWrite, p->pendingPersp);
#endif
}

#ifdef __EMSCRIPTEN__
static bool OpaqueDraw(MarniDX::Impl* p,const float* vertices,int count,MarniHandle tex)
{
    MarniHandle handle=p->whiteHandle;
    if(tex!=MARNI_NULL_HANDLE&&tex<MARNI_MAX_TEXTURES&&p->slots[tex].tex)handle=tex;
    if(handle==MARNI_NULL_HANDLE)return false;
    const auto& slot=p->slots[handle];
    const auto& coverage=slot.hdImage&&slot.hdImage->ready?slot.hdImage->coverage:slot.coverage;
    float left=vertices[4],right=left,top=vertices[5],bottom=top;
    for(int i=0;i<count;i++){
        const float* v=vertices+i*10;
        if(v[9]!=1.0f||!std::isfinite(v[4])||!std::isfinite(v[5]))return false;
        left=std::min(left,v[4]);right=std::max(right,v[4]);
        top=std::min(top,v[5]);bottom=std::max(bottom,v[5]);
    }
    return coverage.coversOpaque(left,top,right,bottom);
}
#endif

static void DrawBatch(MarniDX::Impl* p, const float* verts, int vertexCount,
                      MarniHandle tex, MarniSampler sampler, MarniBlend blend,
                      bool depthTest, bool depthWrite, bool persp)
{
    if (!p || !p->ready || vertexCount<=0) return;
#ifdef __EMSCRIPTEN__
    // Isolate an opaque full-screen quad, not individual opaque mask tiles.
    // The backdrop uses one sampler/full writes; masks and actors retain
    // their existing batching and submission order.
    const bool opaque=blend==MARNI_BLEND_ALPHA&&OpaqueDraw(p,verts,vertexCount,tex);
    bool backdrop=false;
    if(opaque&&!persp&&vertexCount==6){
        float left=verts[0],right=left,top=verts[1],bottom=top;
        for(int i=1;i<6;i++){
            left=std::min(left,verts[i*10]);right=std::max(right,verts[i*10]);
            top=std::min(top,verts[i*10+1]);bottom=std::max(bottom,verts[i*10+1]);
        }
        backdrop=left<=0&&top<=0&&right>=p->width&&bottom>=p->height;
    }
    // World messages keep the current camera crop. Translate only message
    // glyphs (including shadows and Yes/No), never room masks or geometry.
    if(p->promptShift>0&&!p->replaying&&!persp&&tex==p->promptFont&&
        verts[1]>=180.0f*p->height/240.0f){
        p->menuVertices.resize(size_t(vertexCount)*10);
        memcpy(p->menuVertices.data(),verts,size_t(vertexCount)*10*sizeof(float));
        for(int i=0;i<vertexCount;i++)p->menuVertices[i*10+1]-=p->promptShift*p->height/240.0f;
        verts=p->menuVertices.data();
    }
    if (p->pendingCount && ((p->interpolation.recording&&!p->replaying&&p->pendingTexture!=tex) || p->pendingSampler!=sampler ||
        p->pendingBlend!=blend || p->pendingDepthTest!=depthTest ||
        p->pendingDepthWrite!=depthWrite || p->pendingPersp!=persp ||
        p->pendingBackdrop!=backdrop ||
        p->pendingCount+vertexCount>MarniDX::Impl::kBatchVertices)) FlushBatch(p);
    if (vertexCount<=MarniDX::Impl::kBatchVertices) {
        if(!p->pendingCount)p->pendingTextureCount=0;
        int page=0;
        while(page<p->pendingTextureCount&&p->pendingTextures[page]!=tex)page++;
        if(page==4){FlushBatch(p);p->pendingTextureCount=0;page=0;}
        if(page==p->pendingTextureCount)p->pendingTextures[p->pendingTextureCount++]=tex;
        if (!p->pendingCount) {
            p->pendingTexture=tex; p->pendingSampler=sampler; p->pendingBlend=blend;
            p->pendingOpaque=opaque;p->pendingBackdrop=backdrop;
            p->pendingDepthTest=depthTest; p->pendingDepthWrite=depthWrite; p->pendingPersp=persp;
        }else p->pendingOpaque&=opaque;
        float* dest=p->pendingVertices+p->pendingCount*10;
        if(p->menuExtra>0&&!p->replaying)
            WebReflowMenuDraw(verts,dest,vertexCount,p->width,p->height,p->menuExtra,
                persp,tex==p->whiteHandle||tex==MARNI_NULL_HANDLE,p->itemBoxLayout);
        else memcpy(dest,verts,size_t(vertexCount)*10*sizeof(float));
        memset(p->pendingPages+p->pendingCount,page,vertexCount);
        p->pendingCount+=vertexCount;
        return;
    }
    if(p->menuExtra>0&&!p->replaying){
        p->menuVertices.resize(size_t(vertexCount)*10);
        WebReflowMenuDraw(verts,p->menuVertices.data(),vertexCount,p->width,p->height,
            p->menuExtra,persp,tex==p->whiteHandle||tex==MARNI_NULL_HANDLE,p->itemBoxLayout);
        verts=p->menuVertices.data();
    }
    if(opaque)blend=MARNI_BLEND_DISABLE;
#endif
    SubmitBatch(p, verts, vertexCount, tex, sampler, blend, depthTest, depthWrite, persp);
}

// Expand a caller vertex into the internal 10-float layout
// (x, y, z, w, u, v, r, g, b, a). `src` points at one vertex in the caller's
// layout; `hasZ`/`hasW` say which of z/w the caller already provides.
static inline void ExpandVertex(float* dst, const float* src, bool hasZ, bool hasW)
{
    if (hasZ && hasW) {
        // Already { x, y, z, w, u, v, r, g, b, a } - DrawTrianglesPersp.
        for (int i = 0; i < 10; ++i) dst[i] = src[i];
        return;
    }

    dst[0] = src[0];   // x
    dst[1] = src[1];   // y
    dst[2] = hasZ ? src[2] : 0.0f;   // z
    dst[3] = 1.0f;                   // w

    const float* rest = src + (hasZ ? 3 : 2);   // u, v, r, g, b, a
    for (int i = 0; i < 6; ++i) dst[4 + i] = rest[i];
}

// Scratch buffer for the expansion, reused across calls (single-threaded).
static float* ExpandBatch(const float* src, int vertexCount, bool hasZ, bool hasW)
{
    static float* s_buf = nullptr;
    static int s_cap = 0;
    int need = vertexCount * 10;
    if (need > s_cap) {
        float* grown = (float*)realloc(s_buf, (size_t)need * sizeof(float));
        if (grown == nullptr) return nullptr;
        s_buf = grown;
        s_cap = need;
    }
    int inStride = hasZ && hasW ? 10 : (hasZ ? 9 : 8);
    for (int v = 0; v < vertexCount; ++v) {
        ExpandVertex(s_buf + v * 10, src + v * inStride, hasZ, hasW);
    }
    return s_buf;
}

// ---------------------------------------------------------------------------
// MarniDX lifecycle
// ---------------------------------------------------------------------------

MarniDX::MarniDX() : m_pImpl(new Impl()) {}
MarniDX::~MarniDX() { Destroy(); delete m_pImpl; m_pImpl = nullptr; }

BOOL MarniDX::Create(HWND hWnd, int width, int height, BOOL fullScreen,
                     int* outWidth, int* outHeight)
{
    Impl* p = m_pImpl;
    if (p == nullptr) return FALSE;

    // The window and its GL context are created by the platform entry point
    // (main.cpp); this binds the backend to them.
    p->window = (SDL_Window*)hWnd;
    p->ctx = SDL_GL_GetCurrentContext();
    if (p->window == nullptr || p->ctx == nullptr) {
        fprintf(stderr, "[GL] Create: no window or GL context\n");
        return FALSE;
    }

    int dw = 0, dh = 0;
    SDL_GL_GetDrawableSize(p->window, &dw, &dh);
    p->width  = (dw > 0) ? dw : width;
    p->height = (dh > 0) ? dh : height;
#ifdef __EMSCRIPTEN__
    p->normalWidth=p->width;
#endif

    if (!BuildPipeline(p)) return FALSE;

    // 1x1 opaque white, the fallback for solid-colour quads.
    const DWORD white = 0xFFFFFFFFu;
    p->whiteHandle = CreateTexture(1, 1, 32, &white, nullptr, nullptr);
    if (p->whiteHandle == MARNI_NULL_HANDLE) return FALSE;

    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    p->ready = true;
    if (outWidth)  *outWidth  = p->width;
    if (outHeight) *outHeight = p->height;
    return TRUE;
}

void MarniDX::Destroy()
{
    Impl* p = m_pImpl;
    if (p == nullptr) return;
    p->ReleaseAll();
    p->ready = false;
    p->window = nullptr;
    p->ctx = nullptr;
}

BOOL MarniDX::IsReady() const
{
    return (m_pImpl != nullptr && m_pImpl->ready) ? TRUE : FALSE;
}

void MarniDX::GetBackBufferSize(DWORD* outWidth, DWORD* outHeight) const
{
    Impl* p = m_pImpl;
    if (outWidth)  *outWidth  = (p != nullptr) ? (DWORD)p->width : 0;
    if (outHeight) *outHeight = (p != nullptr) ? (DWORD)p->height : 0;
}

int MarniDX::ChangeDisplayMode(DWORD newWidth, DWORD newHeight, BOOL fullScreen)
{
    Impl* p = m_pImpl;
    if (p == nullptr || p->window == nullptr) return 0;
    FlushBatch(p);

    StopInterpolation(p);
    SDL_SetWindowSize(p->window, (int)newWidth, (int)newHeight);
    if (fullScreen) {
        SDL_SetWindowFullscreen(p->window, SDL_WINDOW_FULLSCREEN_DESKTOP);
    } else {
        SDL_SetWindowFullscreen(p->window, 0);
    }

    int dw = 0, dh = 0;
    SDL_GL_GetDrawableSize(p->window, &dw, &dh);
    if (dw > 0) p->width = dw;
    if (dh > 0) p->height = dh;
#ifdef __EMSCRIPTEN__
    p->normalWidth=p->width;
#endif
    return 1;
}

int MarniDX::HandleWindowMessage(HWND, UINT msg, WPARAM, LPARAM)
{
    // SDL owns the event loop on this platform; the only message that matters
    // to the backend is a resize.
    if (msg == WM_SIZE && m_pImpl != nullptr && m_pImpl->window != nullptr) {
        FlushBatch(m_pImpl);
        StopInterpolation(m_pImpl);
        int dw = 0, dh = 0;
        SDL_GL_GetDrawableSize(m_pImpl->window, &dw, &dh);
        if (dw > 0) m_pImpl->width = dw;
        if (dh > 0) m_pImpl->height = dh;
    }
    return 1;
}

unsigned int MarniDX::QueryVideoMemory(BOOL) const
{
    return 128u * 1024u * 1024u;
}

void MarniDX::EnumerateDisplayModes(DisplayModeInfo* outModes, int maxModes,
                                    int* outCount)
{
    int count = 0;
    int display = 0;
    int numModes = SDL_GetNumDisplayModes(display);

    for (int i = 0; i < numModes && count < maxModes; ++i) {
        SDL_DisplayMode mode;
        if (SDL_GetDisplayMode(display, i, &mode) != 0) continue;
        if (outModes != nullptr) {
            outModes[count].dwWidth       = (DWORD)mode.w;
            outModes[count].dwHeight      = (DWORD)mode.h;
            outModes[count].dwBPP         = 32;
            outModes[count].dwRefreshRate = (DWORD)mode.refresh_rate;
            outModes[count].dwFlags       = 0;
        }
        ++count;
    }
    if (outCount) *outCount = count;
}

void MarniDX::EnumerateAdapters(D3DRendererInfo* outRenderers, int max,
                                int* outCount)
{
    int count = 0;
    if (outRenderers != nullptr && max > 0) {
        const char* name = (const char*)glGetString(GL_RENDERER);
        strncpy(outRenderers[0].name, name ? name : "OpenGL", sizeof(outRenderers[0].name) - 1);
        outRenderers[0].name[sizeof(outRenderers[0].name) - 1] = '\0';
        outRenderers[0].flags = 0;
        count = 1;
    }
    if (outCount) *outCount = count;
}

void MarniDX::Clear(float r, float g, float b, float a)
{
    Impl* p = m_pImpl;
    if (p == nullptr || !p->ready) return;
    FlushBatch(p);
#ifdef __EMSCRIPTEN__
    FlushVertexStream(p);
    const double now=emscripten_get_now();
    // GPU/compositor work can stall after GL submission returns. Account for
    // the whole tick as well as the CPU-only deadline checked in Present.
    if(p->lastInterpolatedTick && now-p->lastInterpolatedTick>50.0)
        p->interpolationCooldown=150;
    p->lastInterpolatedTick=0;p->frameStarted=now;
    p->PollHD();
    p->menuExtra=re1_web_layout(&p->promptShift,&p->promptFont,&p->itemBoxLayout);
    // Keep square source pixels at the selected vertical resolution. The
    // wider UI gets a wider drawable, rather than fewer horizontal samples.
    const int uiWidth=p->menuExtra>0?int(std::lround(p->height*(320.0f+p->menuExtra)/240.0f)):p->normalWidth;
    if(uiWidth>0&&uiWidth!=p->width){
        StopInterpolation(p);
        SDL_SetWindowSize(p->window,uiWidth,p->height);p->width=uiWidth;
    }
    // A Clear without a preceding Present abandons an incomplete capture.
    if(p->interpolation.recording)StopInterpolation(p);
    const float clear[]={r,g,b,a};
    unsigned scene=re1_web_interpolation_scene();
    if(p->interpolationCooldown){--p->interpolationCooldown;scene=0;}
    p->interpolation.begin(scene,p->width,p->height,clear);
#endif
    glViewport(0, 0, p->width, p->height);
    glClearColor(r, g, b, a);
    // glClear is masked by the depth write mask. The 2D path leaves it GL_FALSE
    // (DrawBatch disables it for every non-depth draw), so without forcing it
    // back on the depth buffer was never cleared at all - each frame's 3D pass
    // tested against the PREVIOUS frame's depths. Static geometry survived
    // (same depths every frame), which is why it went unnoticed; the item
    // examine screen, whose model rotates, lost everything except the few
    // fragments that happened to be no farther than last frame's surface.
    glDepthMask(GL_TRUE);
    p->lastDepthWrite=1;
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void MarniDX::Present()
{
    Impl* p = m_pImpl;
    if (p == nullptr || !p->ready || p->window == nullptr) return;
    FlushBatch(p);
#ifdef __EMSCRIPTEN__
    auto& frames=p->interpolation;
    if(frames.recording){
        const int wait=re1_web_interpolation_wait_ms();
        if(wait && frames.makeMidpoint()){
            ReplayFrame(p,frames.current,frames.midpoint.data());
            // Drawing itself can exhaust the budget (especially in software
            // GL). Recheck before committing/waiting; back off for about two
            // seconds when the device cannot fit an extra frame.
            if(re1_web_interpolation_wait_ms()){
                p->lastInterpolatedTick=p->frameStarted;
                re1_web_presented(1);
                SDL_GL_SwapWindow(p->window);
                emscripten_sleep(wait);
            }else p->interpolationCooldown=60;
        }
        if(!wait)p->interpolationCooldown=60;
        ReplayFrame(p,frames.current,frames.current.vertices.data());
        frames.finish();
    }
    FlushVertexStream(p);
#endif
#ifdef __EMSCRIPTEN__
    re1_web_presented(0);
#endif
    SDL_GL_SwapWindow(p->window);
}

// ---------------------------------------------------------------------------
// Textures
// ---------------------------------------------------------------------------

MarniHandle MarniDX::CreateTexture(int width, int height, int bpp,
                                   const void* pixelData,
                                   int* outWidth, int* outHeight)
{
    Impl* p = m_pImpl;
    if (p == nullptr || width <= 0 || height <= 0 || pixelData == nullptr) {
        return MARNI_NULL_HANDLE;
    }
    FlushBatch(p);

    StopInterpolation(p);
    // Only the layouts the game actually uploads are supported here: 32-bit
    // RGBA (r in the lowest byte) and 24-bit BGR. Everything else is converted
    // by the caller before reaching this point.
    const unsigned char* src = (const unsigned char*)pixelData;
    const unsigned char* upload = src;
    unsigned char* converted = nullptr;

    if (bpp == 24) {
        converted = (unsigned char*)malloc((size_t)width * height * 4);
        if (converted == nullptr) return MARNI_NULL_HANDLE;
        for (int i = 0; i < width * height; ++i) {
            converted[i * 4 + 0] = src[i * 3 + 2];  // R
            converted[i * 4 + 1] = src[i * 3 + 1];  // G
            converted[i * 4 + 2] = src[i * 3 + 0];  // B
            converted[i * 4 + 3] = 0xFF;
        }
        upload = converted;
    } else if (bpp != 32) {
        return MARNI_NULL_HANDLE;
    }

    MarniHandle handle = MARNI_NULL_HANDLE;
    for (MarniHandle h = 1; h < MARNI_MAX_TEXTURES; ++h) {
        if (p->slots[h].tex == 0) { handle = h; break; }
    }
    if (handle == MARNI_NULL_HANDLE) {
        free(converted);
        return MARNI_NULL_HANDLE;
    }

    int uploadWidth=width,uploadHeight=height;
#ifdef __EMSCRIPTEN__
    p->slots[handle].coverage.analyze(upload,width,height);
    std::string hdKey;
    if(!s_hdDynamic&&WebHDEnabled()&&width>=4&&height>=4) {
        char key[64];WebHD::Key(upload,width,height,key);
        auto found=p->hdTextures.find(key);
        if(found!=p->hdTextures.end()) {
            auto& image=found->second;++image.refs;image.stamp=++p->hdStamp;
            p->slots[handle].tex=image.tex;p->slots[handle].w=width;p->slots[handle].h=height;p->slots[handle].hdImage=&image;
            if(!image.ready){WebHDRequest(key);p->UploadHD(key,image);}else WebHDReused();
            free(converted);if(outWidth)*outWidth=width;if(outHeight)*outHeight=height;return handle;
        }
        if(WebHDSize(key)>32)hdKey=key;
        else WebHDResult(0); // Unknown composites retain original pixels.
    }
#endif
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, p->compat ? GL_RGBA : GL_RGBA8, uploadWidth, uploadHeight, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, upload);
    glBindTexture(GL_TEXTURE_2D, 0);
    p->boundTexture=~GLuint(0);

    free(converted);

    p->slots[handle].tex = tex;
    p->slots[handle].w = width;
    p->slots[handle].h = height;
#ifdef __EMSCRIPTEN__
    if(!hdKey.empty()) {
        auto& image=p->hdTextures[hdKey];image.tex=tex;image.bytes=unsigned(width)*height*4;
        image.refs=1;image.w=width;image.h=height;image.stamp=++p->hdStamp;
        p->hdBytes+=image.bytes;p->slots[handle].hdImage=&image;
        WebHDRequest(hdKey.c_str());p->UploadHD(hdKey,image);p->TrimHD();
    }
#endif

    if (outWidth)  *outWidth  = width;
    if (outHeight) *outHeight = height;
    return handle;
}

MarniHandle MarniDX::CreateTextureFromBits(CMarniBits* bits)
{
    if (bits == nullptr || bits->m_pPixelData == nullptr) return MARNI_NULL_HANDLE;
    return CreateTexture((int)bits->m_width, (int)bits->m_height,
                         (int)bits->m_bitDepth, bits->m_pPixelData, nullptr, nullptr);
}

BOOL MarniDX::UpdateTexturePixels(MarniHandle tex, const void* pixelData,
                                  int width, int height, int bpp)
{
    Impl* p = m_pImpl;
    if (p == nullptr || tex == MARNI_NULL_HANDLE || tex >= MARNI_MAX_TEXTURES) return FALSE;
    if (p->slots[tex].tex == 0 || pixelData == nullptr) return FALSE;
    if (width != p->slots[tex].w || height != p->slots[tex].h) return FALSE;
    if (bpp != 32) return FALSE;
#ifdef __EMSCRIPTEN__
    // Live video uses the dynamic path. Mutating an immutable replacement
    // would also mutate other slots sharing the resident texture.
    if(p->slots[tex].hdImage)return FALSE;
#endif
    FlushBatch(p);

    StopInterpolation(p);
#ifdef __EMSCRIPTEN__
    p->slots[tex].coverage.analyze(static_cast<const unsigned char*>(pixelData),width,height);
#endif
    glBindTexture(GL_TEXTURE_2D, p->slots[tex].tex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,
                    GL_RGBA, GL_UNSIGNED_BYTE, pixelData);
    glBindTexture(GL_TEXTURE_2D, 0);
    p->boundTexture=~GLuint(0);
    return TRUE;
}

void MarniDX::GetTextureSize(MarniHandle tex, int* outWidth, int* outHeight) const
{
    Impl* p = m_pImpl;
    if (outWidth)  *outWidth  = 0;
    if (outHeight) *outHeight = 0;
    if (p == nullptr || tex == MARNI_NULL_HANDLE || tex >= MARNI_MAX_TEXTURES) return;
    if (outWidth)  *outWidth  = p->slots[tex].w;
    if (outHeight) *outHeight = p->slots[tex].h;
}

void MarniDX::DestroyTexture(MarniHandle tex)
{
    if (m_pImpl != nullptr) m_pImpl->ReleaseSlot(tex);
}

MarniHandle MarniDX::WhiteTexture() const
{
    return (m_pImpl != nullptr) ? m_pImpl->whiteHandle : MARNI_NULL_HANDLE;
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------

void MarniDX::DrawSprite(float x, float y, float w, float h,
                         float u0, float v0, float u1, float v1,
                         DWORD color, MarniHandle tex,
                         MarniSampler sampler, MarniBlend blend)
{
#ifdef __EMSCRIPTEN__
    if(blend==MARNI_BLEND_ALPHA){
        if((color>>24)==0)return;
        Impl* p=m_pImpl;
        if(p&&tex!=MARNI_NULL_HANDLE&&tex<MARNI_MAX_TEXTURES&&p->slots[tex].tex){
            const auto& slot=p->slots[tex];
            const auto& coverage=slot.hdImage&&slot.hdImage->ready?slot.hdImage->coverage:slot.coverage;
            if(coverage.coversEmpty(u0,v0,u1,v1))return;
        }
    }
#endif
    float r = ((color >> 16) & 0xFF) / 255.0f;
    float g = ((color >> 8)  & 0xFF) / 255.0f;
    float b = ( color        & 0xFF) / 255.0f;
    float a = ((color >> 24) & 0xFF) / 255.0f;

    float x1 = x + w, y1 = y + h;
    const float verts[6 * 8] = {
        x,  y,  u0, v0, r, g, b, a,
        x1, y,  u1, v0, r, g, b, a,
        x,  y1, u0, v1, r, g, b, a,
        x,  y1, u0, v1, r, g, b, a,
        x1, y,  u1, v0, r, g, b, a,
        x1, y1, u1, v1, r, g, b, a,
    };
    const float* expanded = ExpandBatch(verts, 6, false, false);
    if (expanded != nullptr) {
        DrawBatch(m_pImpl, expanded, 6, tex, sampler, blend, false, false, false);
    }
}

void MarniDX::DrawRect(int x, int y, int w, int h, DWORD color)
{
    DrawSprite((float)x, (float)y, (float)w, (float)h,
               0.0f, 0.0f, 1.0f, 1.0f, color,
               (m_pImpl != nullptr) ? m_pImpl->whiteHandle : MARNI_NULL_HANDLE,
               MARNI_SAMPLER_POINT, MARNI_BLEND_ALPHA);
}

void MarniDX::DrawLine(float x0, float y0, float x1, float y1,
                       float thickness, DWORD color)
{
    float dx = x1 - x0;
    float dy = y1 - y0;
    float len = sqrtf(dx * dx + dy * dy);
    if (len <= 0.0f) return;

    // Perpendicular offset of half the thickness.
    float nx = -dy / len * (thickness * 0.5f);
    float ny =  dx / len * (thickness * 0.5f);

    float r = ((color >> 16) & 0xFF) / 255.0f;
    float g = ((color >> 8)  & 0xFF) / 255.0f;
    float b = ( color        & 0xFF) / 255.0f;
    float a = ((color >> 24) & 0xFF) / 255.0f;

    float ax = x0 + nx, ay = y0 + ny;
    float bx = x1 + nx, by = y1 + ny;
    float cx = x1 - nx, cy = y1 - ny;
    float ex = x0 - nx, ey = y0 - ny;

    const float verts[6 * 8] = {
        ax, ay, 0.0f, 0.0f, r, g, b, a,
        bx, by, 1.0f, 0.0f, r, g, b, a,
        ex, ey, 0.0f, 1.0f, r, g, b, a,
        ex, ey, 0.0f, 1.0f, r, g, b, a,
        bx, by, 1.0f, 0.0f, r, g, b, a,
        cx, cy, 1.0f, 1.0f, r, g, b, a,
    };
    const float* expanded = ExpandBatch(verts, 6, false, false);
    if (expanded != nullptr) {
        DrawBatch(m_pImpl, expanded, 6,
                  (m_pImpl != nullptr) ? m_pImpl->whiteHandle : MARNI_NULL_HANDLE,
                  MARNI_SAMPLER_POINT, MARNI_BLEND_ALPHA,
                  false, false, false);
    }
}

void MarniDX::DrawTriangles(const float* verts, int triCount, MarniHandle tex,
                            MarniSampler sampler, MarniBlend blend)
{
    if (verts == nullptr || triCount <= 0) return;
    const float* expanded = ExpandBatch(verts, triCount * 3, false, false);
    if (expanded != nullptr) {
        DrawBatch(m_pImpl, expanded, triCount * 3, tex, sampler, blend,
                  false, false, false);
    }
}

void MarniDX::DrawTrianglesPersp(const float* verts, int triCount, MarniHandle tex,
                                 MarniSampler sampler, MarniBlend blend,
                                 bool depthTest)
{
    // Input is already { x, y, z, w, u, v, r, g, b, a }.
    if (verts == nullptr || triCount <= 0) return;
    // Already in the internal format. DrawBatch copies into its owned queue;
    // an intermediate copy through ExpandBatch only repeats the same work.
    // Test only, never write: preserve the depth-tested fade polygons.
    DrawBatch(m_pImpl, verts, triCount * 3, tex, sampler, blend,
              depthTest, false, true);
}

void MarniDX::DrawTriangles3D(const float* verts, int triCount, MarniHandle tex,
                              MarniSampler sampler, MarniBlend blend,
                              bool depthWrite)
{
    // Input is { x, y, z (NDC [0,1]), w (view-space Z), u, v, r, g, b, a } -
    // same layout as DrawTrianglesPersp. The header's "9 floats" comment is
    // stale: TmdRenderer's TMD_VERT_FLOATS is 10 and the Windows backend
    // memcpy's sizeof(Model3DVertex) (10 floats) per vertex. Both 3D paths
    // run the perspective vertex shader; only the depth state differs.
    if (verts == nullptr || triCount <= 0) return;
    DrawBatch(m_pImpl, verts, triCount * 3, tex, sampler, blend,
              true, depthWrite, true);
}

#ifdef __EMSCRIPTEN__
bool MarniDX::WantsInterpolation() const
{
    return m_pImpl && m_pImpl->interpolation.recording;
}
void MarniDX::DrawTriangles3DWithKeys(const float* verts,int triCount,MarniHandle tex,
    MarniSampler sampler,MarniBlend blend,bool depthWrite,const MarniTriangleKey* keys)
{
    Impl* p=m_pImpl;
    if(!p||!p->ready||!verts||triCount<=0)return;
    if(!p->interpolation.recording||!keys){DrawTriangles3D(verts,triCount,tex,sampler,blend,depthWrite);return;}
    FlushBatch(p);
    if(p->interpolation.append(verts,triCount*3,tex,sampler,blend,true,depthWrite,true,keys))return;
    StopInterpolation(p);
    SubmitBatchDirect(p,verts,triCount*3,tex,sampler,blend,true,depthWrite,true);
}
#endif

// ---------------------------------------------------------------------------
// Readback
// ---------------------------------------------------------------------------

BOOL MarniDX::CaptureBackbufferToRGBA(void** outPixels, DWORD* outWidth, DWORD* outHeight)
{
    Impl* p = m_pImpl;
    if (p == nullptr || !p->ready || outPixels == nullptr) return FALSE;
    FlushBatch(p);

    StopInterpolation(p);
    size_t size = (size_t)p->width * p->height * 4;
    unsigned char* buf = (unsigned char*)malloc(size);
    if (buf == nullptr) return FALSE;

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, p->width, p->height, GL_RGBA, GL_UNSIGNED_BYTE, buf);

    // GL reads bottom-up; the callers expect top-down.
    unsigned char* row = (unsigned char*)malloc((size_t)p->width * 4);
    if (row != nullptr) {
        for (int y = 0; y < p->height / 2; ++y) {
            unsigned char* a = buf + (size_t)y * p->width * 4;
            unsigned char* b = buf + (size_t)(p->height - 1 - y) * p->width * 4;
            memcpy(row, a, (size_t)p->width * 4);
            memcpy(a, b, (size_t)p->width * 4);
            memcpy(b, row, (size_t)p->width * 4);
        }
        free(row);
    }

    *outPixels = buf;
    if (outWidth)  *outWidth  = (DWORD)p->width;
    if (outHeight) *outHeight = (DWORD)p->height;
    return TRUE;
}

BOOL MarniDX::ReadTextureRGBA(MarniHandle, void**, int*, int*)
{
    // Phase 8: needs a framebuffer-object round trip (glGetTexImage is not in
    // the GLES subset). Only the character-select card mask uses this.
    return FALSE;
}

// ---------------------------------------------------------------------------
// Font texture
// ---------------------------------------------------------------------------

void MarniDX::SetFontTexture(MarniHandle tex, int width, int height)
{
    if (m_pImpl == nullptr) return;
    m_pImpl->fontHandle = tex;
    m_pImpl->fontW = width;
    m_pImpl->fontH = height;
}

MarniHandle MarniDX::FontTexture() const
{
    return (m_pImpl != nullptr) ? m_pImpl->fontHandle : MARNI_NULL_HANDLE;
}

int MarniDX::FontTextureWidth() const
{
    return (m_pImpl != nullptr) ? m_pImpl->fontW : 0;
}

int MarniDX::FontTextureHeight() const
{
    return (m_pImpl != nullptr) ? m_pImpl->fontH : 0;
}

// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------

MarniDX* MarniDX_Create()
{
    if (g_pDX != nullptr) return g_pDX;
    g_pDX = new MarniDX();
    return g_pDX;
}

void MarniDX_DestroyGlobal()
{
    if (g_pDX != nullptr) {
        g_pDX->Destroy();
        delete g_pDX;
        g_pDX = nullptr;
    }
}
