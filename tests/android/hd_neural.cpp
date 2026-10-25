// Asset-free neural regression: independent upstream graph reference values,
// complete receptive-field overlap, arbitrary output scale and original alpha.
#include "platform/web/hd_neural.h"
#include <cassert>
#include <iostream>
int main() {
    std::vector<float> input(13*11),output;
    for(unsigned i=0;i<input.size();i++)input[i]=float((i*73)%251)/255;
    WebHDNeural::Infer(input,13,11,output);
    // OpenCV 4.12 inference of the original FSRCNN_x4.pb, before quantization.
    const unsigned at[]={0,1,51,52,257,1000,1500,2287};
    const float expected[]={.0796351433f,.0924395323f,.4840796590f,.0844599307f,.2789798081f,.7045735717f,.5993190408f,.3301464617f};
    for(unsigned i=0;i<8;i++)assert(std::abs(output[at[i]]-expected[i])<0.00001f);
    constexpr int w=53,h=51,halo=6,iw=w+halo*2,ih=h+halo*2;
    std::vector<unsigned char> rgba(w*h*4),up,base;
    for(int y=0;y<h;y++)for(int x=0;x<w;x++) {
        const unsigned p=(y*w+x)*4;
        for(int c=0;c<3;c++)rgba[p+c]=(x*13+y*7)%256;
        rgba[p+3]=(x+y)%9?255:0;
    }
    input.resize(iw*ih);
    for(int y=0;y<ih;y++)for(int x=0;x<iw;x++)input[y*iw+x]=rgba[(WebHDNeural::Reflect(y-halo,h)*w+WebHDNeural::Reflect(x-halo,w))*4]/255.0f;
    WebHDNeural::Infer(input,iw,ih,output);
    int ow,oh;assert(WebHDNeural::Upscale(rgba.data(),w,h,up,ow,oh));
    int bw,bh;WebHD::Upscale(rgba.data(),w,h,base,bw,bh);
    const auto wx=WebHD::Weights(w*4,ow),wy=WebHD::Weights(h*4,oh);
    unsigned changed=0;
    for(int y=0;y<oh;y++)for(int x=0;x<ow;x++) {
        const unsigned p=(y*ow+x)*4;
        const int sx=std::min(w-1,int((x+.5)*w/ow)),sy=std::min(h-1,int((y+.5)*h/oh));
        assert(up[p+3]==rgba[(sy*w+sx)*4+3]);
        if(!up[p+3])continue;
        float value=0;
        for(int ky=0;ky<4;ky++)for(int kx=0;kx<4;kx++)value+=output[(wy.index[y*4+ky]+halo*4)*iw*4+wx.index[x*4+kx]+halo*4]*255*wx.value[x*4+kx]*wy.value[y*4+ky];
        const float delta=std::max(-40.0f,std::min(40.0f,value-base[p]));
        assert(std::abs(int(up[p])-int(WebHD::Channel(base[p]+delta)))<=1);
        if(up[p]!=base[p])changed++;
    }
    assert(changed>up.size()/40);
    // A smooth edge becomes steeper without ringing past local extrema.
    std::vector<unsigned char> edge(16*8*4);
    const unsigned char ramp[]={20,20,20,20,25,40,70,120,180,220,235,240,240,240,240,240};
    for(int y=0;y<8;y++)for(int x=0;x<16;x++)for(int c=0;c<4;c++)edge[(y*16+x)*4+c]=c==3?255:ramp[x];
    const auto original=edge;WebHDNeural::Sharpen(edge,16,8,1.1f);
    assert(edge[(4*16+8)*4]-edge[(4*16+6)*4]>int(ramp[8])-ramp[6]);
    for(unsigned i=0;i<edge.size();i+=4){assert(edge[i]>=20&&edge[i]<=240);assert(edge[i+3]==255);}
    auto natural=original;WebHDNeural::Sharpen(natural,16,8,0);assert(natural==original);
    for(unsigned i=0;i<natural.size();i+=4){natural[i]=190;natural[i+1]=65;natural[i+2]=12;natural[i+3]=i%20?255:0;}
    const auto uniform=natural;WebHDNeural::Sharpen(natural,16,8,1.1f);assert(natural==uniform);
    auto crisp=up;WebHDNeural::Sharpen(crisp,ow,oh,.7f);
    for(unsigned i=3;i<up.size();i+=4)assert(crisp[i]==up[i]);
    std::cout<<"PASS: pretrained inference, seamless tiles, alpha coverage, stronger edge contrast, bounded sharpening\n";
}
