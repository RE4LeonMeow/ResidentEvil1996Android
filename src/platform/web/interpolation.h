// Port-owned render snapshots. Simulation, scripts and animation ticks stay at
// their original rate. This never blends finished images or lowers resolution.
#pragma once
#include <vector>
#include <cstring>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include "../../marni/MarniRenderKey.h"

struct WebKeyedTriangle {
    MarniTriangleKey key;
    unsigned offset,texture,state;
    bool operator<(const WebKeyedTriangle& b) const {
        if(key<b.key)return true;if(b.key<key)return false;
        if(texture!=b.texture)return texture<b.texture;
        return state<b.state;
    }
};

struct WebRenderDraw {
    unsigned offset, count, texture;
    int sampler, blend;
    bool depthTest, depthWrite, perspective;
    bool operator==(const WebRenderDraw& b) const {
        return offset==b.offset && count==b.count && texture==b.texture &&
            sampler==b.sampler && blend==b.blend && depthTest==b.depthTest &&
            depthWrite==b.depthWrite && perspective==b.perspective;
    }
};

struct WebRenderFrame {
    std::vector<float> vertices;
    std::vector<WebRenderDraw> draws;
    std::vector<WebKeyedTriangle> keys;
    uint32_t flatHash=2166136261u;
    unsigned scene=0;
    int width=0, height=0;
    float clear[4]={};
    void reset() { vertices.clear(); draws.clear(); keys.clear(); flatHash=2166136261u; scene=0; }
};

class WebRenderInterpolation {
public:
    // Three vertex buffers (previous, current, midpoint), bounded below 8 MiB.
    static constexpr unsigned MaxVertices=49152, MaxDraws=1024;
    WebRenderFrame previous, current;
    std::vector<float> midpoint;
    bool recording=false;

    void invalidate() { previous.reset(); current.reset(); recording=false; }
    void begin(unsigned scene, int width, int height, const float* clear) {
        current.reset(); recording=scene!=0;
        if (!recording) { previous.reset(); return; }
        // Reserve the fixed limits once: vector growth must not overshoot the
        // advertised three-buffer memory bound as draw sizes fluctuate.
        current.vertices.reserve(MaxVertices*10);
        current.draws.reserve(MaxDraws);
        current.keys.reserve(MaxVertices/3);
        current.scene=scene; current.width=width; current.height=height;
        std::memcpy(current.clear,clear,4*sizeof(float));
    }
    bool append(const float* vertices, unsigned count, unsigned texture,
                int sampler, int blend, bool depthTest, bool depthWrite, bool perspective,
                const MarniTriangleKey* keys=nullptr) {
        if (!recording || count>MaxVertices || current.vertices.size()/10+count>MaxVertices ||
            current.draws.size()>=MaxDraws) return false;
        current.draws.push_back({unsigned(current.vertices.size()),count,texture,
            sampler,blend,depthTest,depthWrite,perspective});
        const unsigned state=(unsigned(sampler)<<16)|(unsigned(blend)<<8)|
            (depthTest?1:0)|(depthWrite?2:0)|(perspective?4:0);
        if(keys)for(unsigned t=0;t<count/3;++t)current.keys.push_back({keys[t],
            unsigned(current.vertices.size())+t*30,texture,state});
        if(!perspective){
            const auto mix=[&](uint32_t word){current.flatHash=(current.flatHash^word)*16777619u;};
            mix(state);mix(count);mix(texture);
            for(unsigned i=0;i<count*10;++i){uint32_t bits;std::memcpy(&bits,vertices+i,4);mix(bits);}
        }
        current.vertices.insert(current.vertices.end(),vertices,vertices+count*10);
        return true;
    }
    bool makeMidpoint() {
        if (!recording || !previous.scene || current.scene!=previous.scene ||
            current.width!=previous.width || current.height!=previous.height ||
            std::memcmp(current.clear,previous.clear,sizeof(current.clear))!=0) return false;
        if(!current.keys.empty()){
            if(previous.keys.empty()||current.flatHash!=previous.flatHash)return false;
            midpoint.reserve(MaxVertices*10);midpoint=current.vertices;
            unsigned matched=0;
            for(const auto& triangle:current.keys){
                const auto old=std::lower_bound(previous.keys.begin(),previous.keys.end(),triangle);
                if(old==previous.keys.end()||triangle<*old||*old<triangle)continue;
                const float* a=previous.vertices.data()+old->offset;
                const float* b=current.vertices.data()+triangle.offset;
                bool valid=true;
                for(unsigned v=0;v<3&&valid;++v){
                    const unsigned i=v*10;
                    valid=a[i+4]==b[i+4]&&a[i+5]==b[i+5]&&
                        std::fabs(a[i]-b[i])<=current.width/16.0f&&
                        std::fabs(a[i+1]-b[i+1])<=current.height/16.0f;
                    for(unsigned k=0;k<10&&valid;++k)valid=std::isfinite(a[i+k])&&std::isfinite(b[i+k]);
                }
                if(!valid)continue;
                ++matched;
                for(unsigned i=0;i<30;++i)midpoint[triangle.offset+i]=a[i]*.5f+b[i]*.5f;
            }
            // Newly visible/clipped faces use the current pose. A large
            // topology change falls back to the original complete frame.
            return matched*100>=current.keys.size()*85;
        }
        if(current.draws!=previous.draws||current.vertices.size()!=previous.vertices.size())return false;
        bool geometry=false;
        for (const auto& draw:current.draws) {
            const auto* a=previous.vertices.data()+draw.offset;
            const auto* b=current.vertices.data()+draw.offset;
            if (!draw.perspective) {
                // UI, room masks and backgrounds must be identical. Reject the
                // whole generated frame if its composition changed.
                if (std::memcmp(a,b,draw.count*10*sizeof(float))) return false;
                continue;
            }
            geometry=true;
            for (unsigned v=0;v<draw.count;++v,a+=10,b+=10) {
                // Same topology/UVs and short continuous movement only. Depth
                // sort changes, clipping, changed poses/meshes and large jumps
                // fall back to the original frame rather than mixing faces.
                if (a[4]!=b[4] || a[5]!=b[5] ||
                    std::fabs(a[0]-b[0])>current.width/16.0f ||
                    std::fabs(a[1]-b[1])>current.height/16.0f) return false;
                for (unsigned k=0;k<10;++k)if(!std::isfinite(a[k])||!std::isfinite(b[k]))return false;
            }
        }
        if (!geometry) return false;
        midpoint.reserve(MaxVertices*10);
        midpoint=current.vertices;
        for (const auto& draw:current.draws)if(draw.perspective) {
            for (unsigned i=draw.offset;i<draw.offset+draw.count*10;++i)
                midpoint[i]=previous.vertices[i]*.5f+current.vertices[i]*.5f;
        }
        return true;
    }
    void finish() {
        if(recording){
            std::sort(current.keys.begin(),current.keys.end());
            std::swap(previous,current);current.reset();recording=false;
        }
    }
};
