#include "platform/web/hd_image.h"
#include <cassert>
#include <cstring>
#include <iostream>
int main() {
    int w,h;assert(WebHD::Dimensions(320,240,w,h)&&w==1440&&h==1080);
    assert(!WebHD::Dimensions(-1,240,w,h));assert(!WebHD::Dimensions(100000,100000,w,h));
    assert(WebHD::ModDimensions(320,240,1280,960));assert(WebHD::ModDimensions(256,256,4096,4096));
    assert(!WebHD::ModDimensions(320,240,1920,1080));assert(!WebHD::ModDimensions(0,240,1280,960));
    assert(!WebHD::ModDimensions(256,256,8192,8192));assert(!WebHD::ModDimensions(320,240,160,120));
    assert(WebHD::ModPageDimensions(128,256,1024,1024));assert(!WebHD::ModDimensions(128,256,1024,1024));
    assert(!WebHD::ModPageDimensions(128,256,64,1024));
    unsigned char source[64];const unsigned char colors[][4]={{0,0,0,255},{255,0,0,255},{0,255,0,255},{0,0,255,255}};
    for(int i=0;i<16;i++)memcpy(source+i*4,colors[i%4],4);
    char key[64];WebHD::Key(source,4,4,key);std::cout<<key<<'\n';
    assert(strcmp(key,"3338aa45a0a44d056b166c3958f0cbdb_4x4")==0);
    std::vector<unsigned char> out;assert(WebHD::Upscale(source,4,4,out,w,h)&&w==18&&h==18);
    for(unsigned i=3;i<out.size();i+=4)assert(out[i]==255);
    source[3]=0;assert(WebHD::Upscale(source,4,4,out,w,h));
    for(int y=0;y<h;y++)for(int x=0;x<w;x++)assert(out[(y*w+x)*4+3]==(int((x+.5)*4/w)==0&&int((y+.5)*4/h)==0?0:255));
    for(int i=0;i<16;i++){source[i*4]=190;source[i*4+1]=65;source[i*4+2]=12;source[i*4+3]=255;}
    assert(WebHD::Upscale(source,4,4,out,w,h));
    for(unsigned i=0;i<out.size();i+=4)assert(out[i]==190&&out[i+1]==65&&out[i+2]==12&&out[i+3]==255);
    std::cout<<"PASS: bounded HD dimensions, alpha masks, uniform colors\n";
}
