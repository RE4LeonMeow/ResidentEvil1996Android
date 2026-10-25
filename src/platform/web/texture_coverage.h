// Conservative alpha coverage for port-owned render-state optimization.
#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

struct WebTextureCoverage {
    static constexpr int Tile=16;
    int width=0,height=0,columns=0;
    bool opaque=false,zeroAlpha=false,empty=false;
    std::vector<unsigned> prefix,occupied;

    void analyze(const unsigned char* rgba,int w,int h) {
        width=w;height=h;columns=(w+Tile-1)/Tile;
        const int rows=(h+Tile-1)/Tile,stride=columns+1;
        prefix.assign(size_t(stride)*(rows+1),0);occupied.assign(prefix.size(),0);
        opaque=true;zeroAlpha=false;empty=true;
        for(int y=0;y<h;y++)for(int x=0;x<w;x++){
            const unsigned alpha=rgba[(size_t(y)*w+x)*4+3];
            if(alpha!=255){opaque=false;prefix[(y/Tile+1)*stride+x/Tile+1]=1;}
            if(alpha){empty=false;occupied[(y/Tile+1)*stride+x/Tile+1]=1;}
            zeroAlpha|=alpha==0;
        }
        if(opaque)prefix.clear();
        if(!zeroAlpha||empty)occupied.clear();
        for(int y=1;y<=rows;y++)for(int x=1;x<=columns;x++){
            const int at=y*stride+x;
            if(!prefix.empty())prefix[at]+=prefix[at-1]+prefix[at-stride]-prefix[at-stride-1];
            if(!occupied.empty())occupied[at]+=occupied[at-1]+occupied[at-stride]-occupied[at-stride-1];
        }
    }

    bool coversOpaque(float u0,float v0,float u1,float v1) const {
        if(!validUV(u0,v0,u1,v1))return false;
        if(opaque)return true;
        return regionCount(prefix,u0,v0,u1,v1)==0;
    }

    bool coversEmpty(float u0,float v0,float u1,float v1) const {
        if(!validUV(u0,v0,u1,v1))return false;
        if(empty)return true;
        if(!zeroAlpha)return false;
        return regionCount(occupied,u0,v0,u1,v1)==0;
    }

private:
    bool validUV(float u0,float v0,float u1,float v1) const {
        return width>0&&height>0&&std::isfinite(u0)&&std::isfinite(v0)&&std::isfinite(u1)&&std::isfinite(v1);
    }
    unsigned regionCount(const std::vector<unsigned>& counts,float u0,float v0,float u1,float v1) const {
        // Include the filter footprint plus a conservative margin for GLES2
        // mediump UV rounding on HD sheets. Clamp matches CLAMP_TO_EDGE.
        const auto edge=[](float uv,int extent,int pad){
            const int margin=1+(extent+511)/512;
            return std::max(0,std::min(extent-1,int(std::floor(std::max(0.f,std::min(1.f,uv))*extent))+pad*margin))/Tile;
        };
        const int left=edge(std::min(u0,u1),width,-1),right=edge(std::max(u0,u1),width,1)+1;
        const int top=edge(std::min(v0,v1),height,-1),bottom=edge(std::max(v0,v1),height,1)+1;
        const int stride=columns+1;
        return counts[bottom*stride+right]+counts[top*stride+left]-
            counts[top*stride+right]-counts[bottom*stride+left];
    }
};
