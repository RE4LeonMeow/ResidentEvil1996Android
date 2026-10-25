// Port-added: exercise the actual texture upload, shaders, draw and readback.
extern "C" void re1_web_graphics_result(int success);
static bool WebGraphicsCheck(MarniDX* dx, MarniHandle* texture, bool verify)
{
    if (!dx) return false;
    if (*texture == MARNI_NULL_HANDLE) {
        const unsigned char rgba[16]={255,0,0,255, 0,255,0,255, 0,0,255,255, 255,255,0,255};
        *texture=dx->CreateTexture(2,2,32,rgba,NULL,NULL);
        if (*texture==MARNI_NULL_HANDLE) return false;
    }
    DWORD width=0,height=0;dx->GetBackBufferSize(&width,&height);
    dx->Clear(0,0,0,1);
    dx->DrawSprite(0,0,(float)width,(float)height,0,0,1,1,0xffffffff,*texture,MARNI_SAMPLER_POINT,MARNI_BLEND_DISABLE);
    if (verify) {
        void* buffer=NULL;DWORD w=0,h=0;
        if (!dx->CaptureBackbufferToRGBA(&buffer,&w,&h) || !buffer || w<4 || h<4) return false;
        const unsigned char* pixels=(const unsigned char*)buffer;
        unsigned colors=0;
        for (int y=0;y<2;y++) for(int x=0;x<2;x++) {
            const size_t at=((size_t)(h*(y?3:1)/4)*w+w*(x?3:1)/4)*4;
            const bool red=pixels[at]>250, green=pixels[at+1]>250, blue=pixels[at+2]>250;
            if(red&&!green&&!blue)colors|=1;
            if(!red&&green&&!blue)colors|=2;
            if(!red&&!green&&blue)colors|=4;
            if(red&&green&&!blue)colors|=8;
        }
        FILE* file=fopen("/persist/graphics-check.cap","wb");
        if(file){fprintf(file,"RE1CAP %u %u\n",(unsigned)w,(unsigned)h);fwrite(buffer,1,(size_t)w*h*4,file);fclose(file);}
        free(buffer);
        if(colors!=15)return false;
        re1_web_graphics_result(1);
        re1_web_stage("Graphics check passed; displaying test colors");
    }
    dx->Present();
    return true;
}
