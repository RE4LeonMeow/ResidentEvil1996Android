// Port-owned, install-time FSRCNN x4 inference. The game thread never runs it.
// Architecture and pretrained weights: Saafke/FSRCNN_Tensorflow (Apache-2.0).
#pragma once
#include "hd_image.h"
#include "fsrcnn_weights.h"
#ifdef __wasm_simd128__
#include <wasm_simd128.h>
#endif

namespace WebHDNeural {
inline void Conv(const std::vector<float>& input,int w,int h,int in,int out,int kernel,
                 const float* weights,const float* bias,const float* alpha,std::vector<float>& output) {
    output.resize(unsigned(w)*h*out);
    const int pad=kernel/2;
    for(int y=0;y<h;y++)for(int x=0;x<w;x++)for(int o=0;o<out;o+=4) {
#ifdef __wasm_simd128__
        auto sum=wasm_v128_load(bias+o);
#else
        float sum[4];for(int n=0;n<4;n++)sum[n]=bias[o+n];
#endif
        for(int ky=0;ky<kernel;ky++)for(int kx=0;kx<kernel;kx++) {
            const int sx=x+kx-pad,sy=y+ky-pad;
            if(sx<0||sy<0||sx>=w||sy>=h)continue;
            const float* pixel=input.data()+(unsigned(sy)*w+sx)*in;
            const float* weight=weights+((ky*kernel+kx)*in)*out+o;
            for(int c=0;c<in;c++,weight+=out) {
#ifdef __wasm_simd128__
                sum=wasm_f32x4_add(sum,wasm_f32x4_mul(wasm_f32x4_splat(pixel[c]),wasm_v128_load(weight)));
#else
                for(int n=0;n<4;n++)sum[n]+=pixel[c]*weight[n];
#endif
            }
        }
        float* dest=output.data()+(unsigned(y)*w+x)*out+o;
#ifdef __wasm_simd128__
        if(alpha){const auto zero=wasm_f32x4_splat(0);sum=wasm_f32x4_add(wasm_f32x4_max(sum,zero),wasm_f32x4_mul(wasm_f32x4_min(sum,zero),wasm_v128_load(alpha+o)));}
        wasm_v128_store(dest,sum);
#else
        for(int n=0;n<4;n++)dest[n]=alpha&&sum[n]<0?sum[n]*alpha[o+n]:sum[n];
#endif
    }
}
// Exposed separately for numerical comparison against the upstream graph.
inline void Infer(const std::vector<float>& y,int w,int h,std::vector<float>& out) {
    using namespace FSRCNNWeights;
    std::vector<float> a,b;
    Conv(y,w,h,1,56,5,f1,b1,alpha1,a);
    Conv(a,w,h,56,12,1,f2,b2,alpha2,b);
    Conv(b,w,h,12,12,3,f3,b3,alpha3,a);
    Conv(a,w,h,12,12,3,f4,b4,alpha4,b);
    Conv(b,w,h,12,12,3,f5,b5,alpha5,a);
    Conv(a,w,h,12,12,3,f6,b6,alpha6,b);
    Conv(b,w,h,12,56,1,f7,b7,alpha7,a);
    const float zero[16]={};Conv(a,w,h,56,16,1,f8,zero,nullptr,b);
    out.resize(unsigned(w)*h*16);
    for(int iy=0;iy<h;iy++)for(int ix=0;ix<w;ix++)for(int dy=0;dy<4;dy++)for(int dx=0;dx<4;dx++)
        out[(unsigned(iy*4+dy)*w*4+ix*4+dx)]=b[(unsigned(iy)*w+ix)*16+dy*4+dx]+b8[0];
}
inline int Reflect(int value,int length) {
    if(length==1)return 0;
    while(value<0||value>=length)value=value<0?-value:2*length-2-value;
    return value;
}
inline void Sharpen(std::vector<unsigned char>& rgba,int w,int h,float amount) {
    if(!std::isfinite(amount)||amount<=0)return;
    amount=std::min(amount,1.1f);
    const int weights[5]={1,4,6,4,1};
    std::vector<float> horizontal(unsigned(w)*h);
    const auto luma=[&](unsigned at){return .299f*rgba[at]+.587f*rgba[at+1]+.114f*rgba[at+2];};
    for(int y=0;y<h;y++)for(int x=0;x<w;x++) {
        float sum=0,weight=0;
        for(int k=0;k<5;k++) {
            const unsigned at=(unsigned(y)*w+std::max(0,std::min(w-1,x+k-2)))*4;
            if(rgba[at+3]){sum+=luma(at)*weights[k];weight+=weights[k];}
        }
        horizontal[unsigned(y)*w+x]=weight?sum/weight:0;
    }
    // Read original pixels throughout: in-place neighbour feedback would make
    // sharpening depend on scan direction and contaminate transparent edges.
    std::vector<float> delta(unsigned(w)*h,0);
    for(int y=0;y<h;y++)for(int x=0;x<w;x++) {
        const unsigned pixel=unsigned(y)*w+x,at=pixel*4;
        if(!rgba[at+3])continue;
        float sum=0,weight=0,low=luma(at),high=low;
        for(int k=0;k<5;k++) {
            const unsigned sample=unsigned(std::max(0,std::min(h-1,y+k-2)))*w+x;
            if(rgba[sample*4+3]){sum+=horizontal[sample]*weights[k];weight+=weights[k];}
        }
        for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++) {
            const unsigned sample=(unsigned(std::max(0,std::min(h-1,y+dy)))*w+std::max(0,std::min(w-1,x+dx)))*4;
            if(rgba[sample+3]){const float value=luma(sample);low=std::min(low,value);high=std::max(high,value);}
        }
        const float base=luma(at),detail=weight?base-sum/weight:0;
        // Ignore sub-level noise and bound overshoot to local luminance extrema.
        const float gain=std::copysign(std::max(0.0f,std::abs(detail)-.75f),detail)*amount;
        delta[pixel]=std::max(low,std::min(high,base+std::max(-16.0f,std::min(16.0f,gain))))-base;
    }
    for(unsigned i=0;i<unsigned(w)*h;i++)if(rgba[i*4+3])for(int c=0;c<3;c++)rgba[i*4+c]=WebHD::Channel(rgba[i*4+c]+delta[i]);
}
inline bool Upscale(const unsigned char* rgba,int w,int h,std::vector<unsigned char>& result,int& ow,int& oh,float sharpness=0) {
    if(!WebHD::Upscale(rgba,w,h,result,ow,oh))return false;
    // Use neural luminance with alpha-safe Lanczos colour, matching FSRCNN's
    // Y-only training. Do not invent opaque pixels around masks or sprites.
    // Bounded tiles overlap by the complete six-pixel receptive-field radius.
    // That makes tile interiors equivalent to whole-image inference.
    constexpr int tile=48,halo=6;
    std::vector<float> input,neural;
    const auto wx=WebHD::Weights(w*4,ow),wy=WebHD::Weights(h*4,oh);
    for(int ty=0;ty<h;ty+=tile)for(int tx=0;tx<w;tx+=tile) {
        const int cw=std::min(tile,w-tx),ch=std::min(tile,h-ty),iw=cw+halo*2,ih=ch+halo*2;
        input.resize(unsigned(iw)*ih);
        for(int y=0;y<ih;y++)for(int x=0;x<iw;x++) {
            const unsigned at=(unsigned(Reflect(ty+y-halo,h))*w+Reflect(tx+x-halo,w))*4;
            input[unsigned(y)*iw+x]=(.299f*rgba[at]+.587f*rgba[at+1]+.114f*rgba[at+2])/255.0f;
        }
        Infer(input,iw,ih,neural);
        const int left=(tx*ow+w-1)/w,right=((tx+cw)*ow+w-1)/w;
        const int top=(ty*oh+h-1)/h,bottom=((ty+ch)*oh+h-1)/h;
        for(int y=top;y<bottom;y++)for(int x=left;x<right;x++) {
            const unsigned at=(unsigned(y)*ow+x)*4;
            if(!result[at+3])continue;
            float value=0;
            for(int ky=0;ky<4;ky++)for(int kx=0;kx<4;kx++) {
                const int nx=wx.index[x*4+kx]-tx*4+halo*4,ny=wy.index[y*4+ky]-ty*4+halo*4;
                value+=neural[unsigned(ny)*iw*4+nx]*255*wx.value[x*4+kx]*wy.value[y*4+ky];
            }
            const float base=.299f*result[at]+.587f*result[at+1]+.114f*result[at+2];
            // Limit ringing near palette edges; retain the source's colour/alpha.
            const float detail=std::max(-40.0f,std::min(40.0f,value-base));
            for(int c=0;c<3;c++)result[at+c]=WebHD::Channel(result[at+c]+detail);
        }
    }
    Sharpen(result,ow,oh,sharpness);return true;
}
} // namespace WebHDNeural
