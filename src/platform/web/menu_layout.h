// Port-only inventory layout. Positions use the original 320x240 UI space.
// Glyphs, icons, models and HUD widgets keep their aspect ratio. Only long
// decorative frame strips and flat backgrounds expand into the added space.
#pragma once
#include <algorithm>
#include <cstring>

inline void WebReflowMenuDraw(const float* src,float* dst,int count,int width,int height,
                             float extra,bool perspective,bool flatFill,int layout=0)
{
    std::memcpy(dst,src,size_t(count)*10*sizeof(float));
    if(extra<=0||count<=0)return;
    const float sx=width/320.0f,sy=height/240.0f,ratio=320.0f/(320.0f+extra);
    // A model always translates as one object. Its thousands of triangle
    // vertices do not need the sprite bounds/classification pass below.
    if(perspective){
        for(int i=0;i<count;i++)dst[i*10]=(src[i*10]/sx+extra*.5f)*sx*ratio;
        return;
    }
    float left=src[0]/sx,right=left,top=src[1]/sy,bottom=top;
    for(int i=1;i<count;i++){
        left=std::min(left,src[i*10]/sx);right=std::max(right,src[i*10]/sx);
        top=std::min(top,src[i*10+1]/sy);bottom=std::max(bottom,src[i*10+1]/sy);
    }
    const float w=right-left,h=bottom-top;
    if(layout==2){
        // Full map/document pages have one coordinate system. Center every
        // graphic, marker and text line together. Only full-width flat fills
        // and decorative horizontal rules expand into the added space.
        const bool full=(flatFill&&w>=319)||(w>=319&&h<=16);
        for(int i=0;i<count;i++){
            const float x=src[i*10]/sx;
            dst[i*10]=(full?x/ratio:x+extra*.5f)*sx*ratio;
        }
        return;
    }
    enum { Content,FullWidth,PreviewStrip,LeftEdge,RightEdge,Sidebar,EquipmentFrame,EquipmentIcon } placement=Content;
    if(!perspective){
        if((flatFill&&(w>=319||(top>=180&&w>=240))) || (top>=180&&w>=64&&h<=16))placement=FullWidth;
        else if(top>=180)placement=Content; // description, Yes/No and cursor
        // The item action sprites (including their opening animation and
        // selection highlight) belong beside the inventory, not at the
        // center of the expanded model preview.
        else if(!layout&&!flatFill&&left>=143.5f&&right<=201.5f&&top>=56.5f&&bottom<=129.5f)placement=Sidebar;
        // Original masks draw over the actions to blank the area outside
        // the model frame. Anchor their inner edges to that frame too.
        else if(flatFill&&top>=11&&bottom<=141&&left>=198)placement=Sidebar;
        else if(flatFill&&top>=11&&bottom<=141&&right<=10)placement=LeftEdge;
        else if(left>=147&&right<=209&&top>=141&&bottom<=181&&w>=56&&h>=36)placement=EquipmentFrame;
        else if(left>=159&&right<=201&&top>=145&&bottom<=177)placement=EquipmentIcon;
        else if(left>=202)placement=Sidebar;
        else if(top>=11&&bottom<=141){
            if(left>=31&&right<=193&&
               (bottom<=24||top>=127||(flatFill&&w>=127)))placement=PreviewStrip;
            else if(right<=33&&left>=7)placement=LeftEdge;
            else if(left>=191&&right<=201)placement=RightEdge;
        }
    }
    for(int i=0;i<count;i++){
        float x=src[i*10]/sx;
        switch(placement){
        case FullWidth:x/=ratio;break;
        case PreviewStrip:x+=extra*std::clamp((x-32)/160.0f,0.0f,1.0f);break;
        case LeftEdge:break;
        case RightEdge:case Sidebar:x+=extra;break;
        case EquipmentFrame:x+=extra*(.5f+.5f*std::clamp((x-148)/60.0f,0.0f,1.0f));break;
        case EquipmentIcon:x+=extra*.75f;break;
        default:x+=extra*.5f;break;
        }
        dst[i*10]=x*sx*ratio;
    }
}
