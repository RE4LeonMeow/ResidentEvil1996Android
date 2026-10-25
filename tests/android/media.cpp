// Decode a generated Cinepak + PCM AVI using exactly the wasm movie libraries.
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}
#include <assert.h>
#include <stdio.h>
int main(){
 AVFormatContext* file=nullptr;assert(avformat_open_input(&file,"/test.avi",nullptr,nullptr)==0);
 assert(avformat_find_stream_info(file,nullptr)>=0);
 AVCodecContext* contexts[2]={};int streams[2]={-1,-1};
 for(unsigned i=0;i<file->nb_streams;i++){
  auto* par=file->streams[i]->codecpar;
  int type=par->codec_type==AVMEDIA_TYPE_VIDEO?0:par->codec_type==AVMEDIA_TYPE_AUDIO?1:-1;
  if(type<0)continue;
  const AVCodec* codec=avcodec_find_decoder(par->codec_id);assert(codec);
  contexts[type]=avcodec_alloc_context3(codec);assert(avcodec_parameters_to_context(contexts[type],par)==0);
  assert(avcodec_open2(contexts[type],codec,nullptr)==0);streams[type]=i;
 }
 assert(contexts[0]&&contexts[1]);
 AVPacket* packet=av_packet_alloc();AVFrame* frame=av_frame_alloc();int pictures=0,samples=0;
 while(av_read_frame(file,packet)>=0){
  for(int i=0;i<2;i++)if(packet->stream_index==streams[i]){
   assert(avcodec_send_packet(contexts[i],packet)==0);
   while(avcodec_receive_frame(contexts[i],frame)==0){
    if(i==0){assert(frame->width==32&&frame->height==32&&frame->data[0]);pictures++;}
    else samples+=frame->nb_samples;
    av_frame_unref(frame);
   }
  }
  av_packet_unref(packet);
 }
 assert(pictures==5 && samples==11025);
 av_packet_free(&packet);av_frame_free(&frame);for(auto*& context:contexts)avcodec_free_context(&context);avformat_close_input(&file);
 printf("PASS: wasm decoded %d Cinepak frames and %d PCM audio samples\n",pictures,samples);
}
