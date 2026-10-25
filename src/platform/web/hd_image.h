// Port-added, bounded still-image replacement helpers; no original game layout.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace WebHD {
constexpr unsigned MaxPixels=4u*1024*1024;
inline bool Dimensions(int w,int h,int& ow,int& oh) {
    if(w<1||h<1||unsigned(w)>MaxPixels/unsigned(h))return false;
    const double scale=std::min({4.5,2048.0/w,2048.0/h,std::sqrt(double(MaxPixels)/(double(w)*h))});
    ow=std::max(w,int(std::floor(w*scale)));oh=std::max(h,int(std::floor(h*scale)));
    return true;
}
inline void Key(const unsigned char* rgba,int w,int h,char (&key)[64]) {
    uint32_t a=2166136261u,b=5381u,c=0x9e3779b9u,d=0x85ebca6bu;
    for(unsigned i=0;i<unsigned(w)*unsigned(h)*4;i++) {
        const uint32_t v=rgba[i];a=(a^v)*16777619u;b=b*33u^v;
        c=(c^v)*0x5bd1e995u;d=(d+v)*0x27d4eb2du^(d>>15);
    }
    std::snprintf(key,sizeof(key),"%08x%08x%08x%08x_%dx%d",a,b,c,d,w,h);
}
struct Axis {std::vector<int> index;std::vector<float> value;};
inline Axis Weights(int src,int dst) {
    Axis result;result.index.resize(dst*4);result.value.resize(dst*4);
    constexpr double pi=3.14159265358979323846;
    for(int x=0;x<dst;x++) {
        const double at=(x+.5)*src/dst-.5;const int start=int(std::floor(at))-1;double sum=0;
        for(int k=0;k<4;k++) {
            const double distance=std::abs(at-start-k);
            const double value=distance<1e-8?1:distance>=2?0:std::sin(pi*distance)*std::sin(pi*distance/2)/(pi*pi*distance*distance/2);
            result.index[x*4+k]=std::max(0,std::min(src-1,start+k));result.value[x*4+k]=float(value);sum+=value;
        }
        for(int k=0;k<4;k++)result.value[x*4+k]=float(result.value[x*4+k]/sum);
    }
    return result;
}
inline unsigned char Channel(float value) {return (unsigned char)std::max(0,std::min(255,int(std::floor(value+.5f))));}
inline bool Upscale(const unsigned char* rgba,int w,int h,std::vector<unsigned char>& out,int& ow,int& oh) {
    if(!rgba||!Dimensions(w,h,ow,oh))return false;
    const Axis wx=Weights(w,ow),wy=Weights(h,oh);
    std::vector<float> temp(unsigned(ow)*h*4,0);out.assign(unsigned(ow)*oh*4,0);
    for(int y=0;y<h;y++)for(int x=0;x<ow;x++) {
        const unsigned to=(unsigned(y)*ow+x)*4;
        for(int k=0;k<4;k++) {
            const unsigned from=(unsigned(y)*w+wx.index[x*4+k])*4;
            const float value=wx.value[x*4+k],alpha=rgba[from+3]/255.0f;
            for(int c=0;c<3;c++)temp[to+c]+=rgba[from+c]*alpha*value;
            temp[to+3]+=alpha*value;
        }
    }
    for(int y=0;y<oh;y++)for(int x=0;x<ow;x++) {
        const unsigned to=(unsigned(y)*ow+x)*4;
        const unsigned nearest=(std::min(h-1,int((y+.5)*h/oh))*w+std::min(w-1,int((x+.5)*w/ow)))*4;
        float color[4]={};
        for(int k=0;k<4;k++) {
            const unsigned from=(unsigned(wy.index[y*4+k])*ow+x)*4;const float value=wy.value[y*4+k];
            for(int c=0;c<4;c++)color[c]+=temp[from+c]*value;
        }
        out[to+3]=rgba[nearest+3];
        if(color[3]>1e-6f&&out[to+3])for(int c=0;c<3;c++)out[to+c]=Channel(color[c]/color[3]);
    }
    return true;
}
} // namespace WebHD
