#pragma once

#include <functional>
#include <iostream>
#include "ffmpeg-reader.h"

#pragma warning(push, 0)
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/dict.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/pixdesc.h>
#include <libavutil/log.h>
}
#pragma warning(pop)

class FFmpegEncoder {
 private:
  AVCodec* video_codec;
  AVCodecContext* video_codec_ctx;
  AVPacket* pkt;
  AVFrame* frame;
  AVRational src_timebase;
 public:
  std::function<void(AVPacket*, uint32_t)> on_video_packet_encoded;

  void encode(AVFrame *frame) {
    int ret;
 
    ret = avcodec_send_frame(video_codec_ctx, frame);
    if (ret < 0) {
      std::cout << "Error sending a frame for encoding" << std::endl;
      exit(1);
    }
 
    while (ret >= 0) {
      ret = avcodec_receive_packet(video_codec_ctx, pkt);
      if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        return;
      } else if (ret < 0) {
        std::cout << "Error during encoding";
        exit(1);
      }
      const auto pts = av_rescale_q(pkt->pts, src_timebase, video_codec_ctx->time_base);

      on_video_packet_encoded(pkt, pts);
      av_packet_unref(pkt);
    }
  }

  void init(AVCodecContext* ctx) {
    src_timebase = ctx->time_base;
    video_codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    video_codec_ctx = avcodec_alloc_context3(video_codec);

    pkt = av_packet_alloc();

    video_codec_ctx->width = 1920;
    video_codec_ctx->height = 1080;

    video_codec_ctx->time_base.num = 1;
    video_codec_ctx->time_base.den = 90000;

    video_codec_ctx->framerate.num = 25;
    video_codec_ctx->framerate.den = 1;

    video_codec_ctx->gop_size = 10;
    video_codec_ctx->max_b_frames = 1;
    video_codec_ctx->level = 40;
    video_codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;

    av_opt_set(video_codec_ctx->priv_data, "profile", "baseline", 0);
    av_opt_set(video_codec_ctx->priv_data, "preset", "ultrafast", 0);
    av_opt_set(video_codec_ctx->priv_data, "tune", "zerolatency", 0);
    av_opt_set(video_codec_ctx->priv_data, "x264opts", "bitrate=60000:aud=1", 0);
    
    ck(avcodec_open2(video_codec_ctx, video_codec, NULL));
  }

  FFmpegEncoder() {
  } 

};