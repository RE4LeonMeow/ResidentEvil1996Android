#include "platform/web/texture_coverage.h"
#include <cassert>
#include <cstdio>
#include <limits>

int main(){
    WebTextureCoverage coverage;
    assert(!coverage.coversOpaque(0,0,1,1));
    assert(!coverage.coversEmpty(0,0,1,1));
    const int w=73,h=45;
    std::vector<unsigned char> pixels(w*h*4,255);
    coverage.analyze(pixels.data(),w,h);
    assert(coverage.opaque&&!coverage.zeroAlpha&&coverage.coversOpaque(-10,-10,10,10));
    assert(!coverage.coversEmpty(0,0,1,1));
    assert(!coverage.coversOpaque(std::numeric_limits<float>::quiet_NaN(),0,1,1));
    pixels[(20*w+20)*4+3]=0;pixels[(44*w+72)*4+3]=254;
    coverage.analyze(pixels.data(),w,h);
    assert(!coverage.opaque&&coverage.zeroAlpha);
    assert(!coverage.coversOpaque(0,0,1,1));
    assert(!coverage.coversOpaque(20.f/w,20.f/h,20.f/w,20.f/h));
    assert(!coverage.coversOpaque(10,10,10,10)); // clamped partial edge
    assert(coverage.coversOpaque(-10,-10,-1,-1));
    assert(coverage.coversOpaque(.6f,0,.8f,.2f));
    // Random UV rectangles: every accepted rectangle must be opaque even
    // after expanding the sampling footprint for linear filtering/rounding.
    unsigned random=12345;
    const auto next=[&](){random=random*1664525u+1013904223u;return float(random%3000)/1000.f-1;};
    unsigned accepted=0,rejected=0;
    for(int i=0;i<5000;i++){
        const float u0=next(),u1=next(),v0=next(),v1=next();
        if(!coverage.coversOpaque(u0,v0,u1,v1)){rejected++;continue;}
        accepted++;
        const auto edge=[](float uv,int extent,int pad){return std::max(0,std::min(extent-1,int(std::floor(std::max(0.f,std::min(1.f,uv))*extent))+pad));};
        for(int y=edge(std::min(v0,v1),h,-1);y<=edge(std::max(v0,v1),h,1);y++)
            for(int x=edge(std::min(u0,u1),w,-1);x<=edge(std::max(u0,u1),w,1);x++)assert(pixels[(y*w+x)*4+3]==255);
    }
    assert(accepted&&rejected);
    pixels.assign(w*h*4,255);coverage.analyze(pixels.data(),w,h);
    assert(coverage.opaque&&!coverage.zeroAlpha&&coverage.coversOpaque(0,0,1,1));
    pixels.assign(w*h*4,0);coverage.analyze(pixels.data(),w,h);
    assert(coverage.empty&&coverage.coversEmpty(0,0,1,1));
    pixels[(20*w+20)*4+3]=1;coverage.analyze(pixels.data(),w,h);
    assert(!coverage.empty&&!coverage.coversEmpty(20.f/w,20.f/h,20.f/w,20.f/h));
    assert(coverage.coversEmpty(.7f,.8f,1,1)); // empty subrectangle of a live atlas
    assert(!coverage.coversEmpty(std::numeric_limits<float>::quiet_NaN(),0,1,1));
    puts("PASS: conservative opaque coverage; zero/partial alpha; clamp, filtering margin, odd dimensions and texture updates");
}
