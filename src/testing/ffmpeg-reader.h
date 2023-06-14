#pragma once

#include <string>
#include <iostream>
#include <functional>
#include <thread>
#include <chrono>

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
}

#pragma warning(pop)

inline bool check(int e, int iLine, const char *szFile) {
  if (e < 0) {
    std::cout << "General error " << e << " at line " << iLine << " in file " << szFile;
    return false;
  }
  return true;
}

#define ck(call) check(call, __LINE__, __FILE__)

class FFmpegReader {
 private:
  AVFormatContext* ctx = nullptr;
  int video_stream = -1;
  int audio_stream = -1;
  AVCodec* video_codec;
  AVCodec* audio_codec;
  AVCodecContext* video_codec_ctx = nullptr;
  AVCodecContext* audio_codec_ctx = nullptr;
  size_t video_dst_bufsize = 0;
  AVFrame *frame = NULL;
  AVPacket packet;
  int video_frame_count = 0;
  int audio_frame_count = 0; 
  
  uint8_t *video_dst_data[4] = {NULL};
  int video_dst_linesize[4];

  AVFormatContext* create_format_context(const std::string filename) {
    avformat_network_init();

    ctx = avformat_alloc_context();
    AVDictionary* opts = NULL;
    AVInputFormat * ifmt = NULL;

    av_dict_set_int(&opts, "buffer_size", 2 * 1024 * 1024, 0);

    const auto res = avformat_open_input(&ctx, filename.c_str(), ifmt, &opts);
    if (res != 0) throw new std::exception("Failed to open file");
    return ctx;
  }

  int decode_packet(int *got_frame, int cached) {
    int ret = 0;
    int decoded = packet.size;
    *got_frame = 0;
    if (packet.stream_index == video_stream) {
        /* decode video frame */
        ret = avcodec_decode_video2(video_codec_ctx, frame, got_frame, &packet);
        if (ret < 0) {
            std::cout << "Error decoding video frame" << std::endl;
            return ret;
        }
        if (*got_frame) {
            /* copy decoded frame to destination buffer:
             * this is required since rawvideo expects non aligned data */
            av_image_copy(video_dst_data, video_dst_linesize,
                          (const uint8_t **)(frame->data), frame->linesize,
                          video_codec_ctx->pix_fmt, video_codec_ctx->width, video_codec_ctx->height);
                          
            on_video_frame(frame, video_dst_data[0], video_dst_bufsize);

            /* write to rawvideo file */
           // fwrite(video_dst_data[0], 1, video_dst_bufsize, video_dst_file);
        }
    } else if (packet.stream_index == audio_stream) {
        /* decode audio frame */
        ret = avcodec_decode_audio4(audio_codec_ctx, frame, got_frame, &packet);
        if (ret < 0) {
            return ret;
        }
        /* Some audio decoders decode only part of the packet, and have to be
         * called again with the remainder of the packet data.
         * Sample: fate-suite/lossless-audio/luckynight-partial.shn
         * Also, some decoders might over-read the packet. */
        decoded = FFMIN(ret, packet.size);
        if (*got_frame) {
            size_t unpadded_linesize = frame->nb_samples * av_get_bytes_per_sample((AVSampleFormat)frame->format);
            /* Write the raw audio data samples of the first plane. This works
             * fine for packed formats (e.g. AV_SAMPLE_FMT_S16). However,
             * most audio decoders output planar audio, which uses a separate
             * plane of audio samples for each channel (e.g. AV_SAMPLE_FMT_S16P).
             * In other words, this code will write only the first audio channel
             * in these cases.
             * You should use libswresample or libavfilter to convert the frame
             * to packed data. */

           // fwrite(frame->extended_data[0], 1, unpadded_linesize, audio_dst_file);
            on_audio_frame(frame);
        }
    }
    /* If we use the new API with reference counting, we own the data and need
     * to de-reference it when we don't use it anymore */
    if (*got_frame) av_frame_unref(frame);
    return decoded;
}

 public:
  std::function<void(AVFrame*, uint8_t*, size_t)> on_video_frame;
  std::function<void(AVFrame*)> on_audio_frame;

  AVCodecContext* get_audio_codec_context() {
    return this->audio_codec_ctx;
  }

  AVCodecContext* get_video_codec_context() {
    return this->video_codec_ctx;
  }

  FFmpegReader(const std::string filename) {
    const auto ctx = create_format_context(filename);
    ck(avformat_find_stream_info(ctx, nullptr));

    video_stream = av_find_best_stream(ctx, AVMediaType::AVMEDIA_TYPE_VIDEO, -1, -1, &video_codec, 0);
    if (video_stream < 0) throw std::exception("No video stream found");
    const auto st = ctx->streams[video_stream];
    video_codec_ctx = st->codec;

    ck(avcodec_open2(video_codec_ctx, video_codec, nullptr));
    video_codec_ctx->time_base = st->time_base;

    audio_stream = av_find_best_stream(ctx, AVMediaType::AVMEDIA_TYPE_AUDIO, -1, -1, &audio_codec, 0);
    if (audio_stream < 0) {
      std::cout << "No audio" << std::endl;
    } else {
      const auto st = ctx->streams[audio_stream];
      audio_codec_ctx = st->codec;
      ck(avcodec_open2(audio_codec_ctx, audio_codec, nullptr));
    }

    const auto ret = av_image_alloc(video_dst_data, video_dst_linesize, video_codec_ctx->width, video_codec_ctx->height, video_codec_ctx->pix_fmt, 1);
    if (ret < 0) {
      throw std::exception("Could not allocate video buffer");
    }
    video_dst_bufsize = ret;

    frame = av_frame_alloc();
    av_init_packet(&packet);
    packet.data = nullptr;
    packet.size = 0;
  }

  void decode() {
    while (av_read_frame(ctx, &packet) >= 0) {
      AVPacket orig_pkt = packet;
      int got_frame = 0;
      do {
        const auto ret = decode_packet(&got_frame, 0);
        if (ret < 0) {
          break;
        }

        packet.data += ret;
        packet.size -= ret;

      } while (packet.size > 0);
      av_free_packet(&orig_pkt);
    }
  }
};