#include "platform/web/interpolation.h"
#include <cassert>
#include <cstdio>

int main(){
 WebRenderInterpolation f;const float clear[]={0,0,0,1};
 float v[]={10,20,.5f,10,0,0,1,1,1,1, 20,20,.5f,10,1,0,1,1,1,1, 10,30,.5f,10,0,1,1,1,1,1};
 f.begin(1,640,480,clear);assert(f.append(v,3,1,0,0,true,true,true));assert(!f.makeMidpoint());f.finish();
 for(int i=0;i<3;i++)v[i*10]+=4;
 f.begin(1,640,480,clear);assert(f.append(v,3,1,0,0,true,true,true));
 assert(f.makeMidpoint());assert(f.midpoint[0]==12&&f.midpoint[10]==22);
 assert(f.current.vertices[0]==14&&f.previous.vertices[0]==10); // original endpoints intact
 f.current.vertices[4]=.1f;assert(!f.makeMidpoint());f.current.vertices[4]=0;
 f.current.vertices[0]+=100;assert(!f.makeMidpoint());f.current.vertices[0]-=100;
 f.current.scene=2;assert(!f.makeMidpoint());f.current.scene=1;
 f.current.width=800;assert(!f.makeMidpoint());f.current.width=640;
 f.current.draws[0].blend=1;assert(!f.makeMidpoint());f.current.draws[0].blend=0;
 f.current.vertices[0]=NAN;assert(!f.makeMidpoint());
 f.invalidate();assert(!f.recording&&f.previous.scene==0);
 f.begin(1,640,480,clear);assert(!f.append(v,WebRenderInterpolation::MaxVertices+1,1,0,0,true,true,true));
 f.begin(0,640,480,clear);assert(!f.recording&&!f.makeMidpoint());
 // Matching must follow primitive identity when the renderer changes order.
 float a[60]={};
 for(unsigned i=0;i<6;++i){a[i*10]=i<3?10:30;a[i*10+1]=20;a[i*10+2]=.5f;a[i*10+3]=10;a[i*10+4]=float(i%3);}
 const MarniTriangleKey keys[]={{1,2,0},{1,2,1}},reversed[]={{1,2,1},{1,2,0}};
 f.begin(1,640,480,clear);assert(f.append(a,6,1,0,0,true,true,true,keys));f.finish();
 float b[60];std::memcpy(b,a+30,30*sizeof(float));std::memcpy(b+30,a,30*sizeof(float));
 for(unsigned i=0;i<6;++i)b[i*10]+=4;
 f.begin(1,640,480,clear);assert(f.append(b,6,1,0,0,true,true,true,reversed));
 assert(f.makeMidpoint());assert(f.midpoint[0]==32&&f.midpoint[30]==12);
 f.current.keys[0].key.object=99;assert(!f.makeMidpoint());
 puts("PASS: exact interpolation endpoints; midpoint; UV/topology, jump, scene, resize, state, NaN and memory-limit fallback");
}
