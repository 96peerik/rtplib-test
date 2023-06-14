#pragma once

#define FRAME_SIZE 960
#define SAMPLE_RATE 48000
#define CHANNELS 2
#define APPLICATION OPUS_APPLICATION_AUDIO
#define BITRATE 64000
#define OUTPUT_SAMPLE_FORMAT AV_SAMPLE_FMT_S16
#define MAX_FRAME_SIZE 6*1024
#define MAX_PACKET_SIZE (3*1276)

#include <functional>
#include <opus.h>
#include "ffmpeg-reader.h"

#pragma warning(push, 0)
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/dict.h>
#include <libavutil/opt.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/pixdesc.h>
#include <libavutil/log.h>
}
#pragma warning(pop)

class OpusEncoder {
 private:
  SwrContext *swr_ctx = nullptr;
  AVAudioFifo *fifo = nullptr;

  OpusEncoder* opus_encoder = nullptr;
  unsigned char cbits[MAX_PACKET_SIZE];
  opus_int16 in[FRAME_SIZE*CHANNELS];

  int buffer_line_size = 0;
  uint8_t** audio_buffer = nullptr;
  uint32_t pts = 0;

  static int init_fifo(AVAudioFifo **fifo) {
    if (!(*fifo = av_audio_fifo_alloc(OUTPUT_SAMPLE_FORMAT, CHANNELS, 1))) {
      fprintf(stderr, "Could not allocate FIFO\n");
      return AVERROR(ENOMEM);
    }
    return 0;
  }

  static int add_samples_to_fifo(AVAudioFifo *fifo, uint8_t **converted_input_samples, const int frame_size) {
    int error;

    if ((error = av_audio_fifo_realloc(fifo, av_audio_fifo_size(fifo) + frame_size)) < 0) {
      fprintf(stderr, "Could not reallocate FIFO\n");
      return error;
    }

    if (av_audio_fifo_write(fifo, (void **)converted_input_samples, frame_size) < frame_size) {
      fprintf(stderr, "Could not write data to FIFO\n");
      return AVERROR_EXIT;
    }
    return 0;
 }

 public:
  std::function<void(uint8_t*, uint32_t, uint32_t)> on_audio_packet_encoded;

  ~OpusEncoder() {
    av_freep(&audio_buffer[0]);
    av_freep(&audio_buffer);
    swr_free(&swr_ctx);
    av_audio_fifo_free(fifo);
    opus_encoder_destroy(opus_encoder);
  }

  void encode_audio(AVFrame *frame) {
    int ret;

    int got_samples = swr_convert(swr_ctx, audio_buffer, FRAME_SIZE, (const uint8_t **)frame->extended_data, frame->nb_samples);
    if (got_samples < 0) {
      fprintf(stderr, "error: swr_convert()\n");
      exit(1);
    }

    while (got_samples > 0) {
      add_samples_to_fifo(fifo, audio_buffer, got_samples);

      while (av_audio_fifo_size(fifo) >= FRAME_SIZE) {
        void* data;
        if ((ret = av_audio_fifo_read(fifo, (void**)audio_buffer, FRAME_SIZE)) < 0) {
          fprintf(stderr, "Could not read data from FIFO\n");
          exit(1);
        }
        for (int i=0;i < CHANNELS * FRAME_SIZE; i++) {
          in[i] = (unsigned char)audio_buffer[0][2*i+1] << 8 | (unsigned char)audio_buffer[0][2*i];      
        }
            /* Encode the frame. */
        int nbBytes = opus_encode(opus_encoder, in, FRAME_SIZE, cbits, MAX_PACKET_SIZE);
        if (nbBytes<0)
        {
          fprintf(stderr, "encode failed: %s\n", opus_strerror(nbBytes));
          exit(1);
        }

        on_audio_packet_encoded((uint8_t*)&cbits[0], nbBytes, pts);
        pts += got_samples;
      }

      got_samples = swr_convert(swr_ctx, audio_buffer, FRAME_SIZE, NULL, 0);
      if (got_samples < 0) {
        fprintf(stderr, "error: swr_convert()\n");
        exit(1);
      }
    }
  }

  void init(AVCodecContext* src_audio_codec_ctx) {
    int err;
    opus_encoder = opus_encoder_create(SAMPLE_RATE, CHANNELS, APPLICATION, &err);
    if (err < 0) {
      fprintf(stderr, "failed to create an encoder: %s\n", opus_strerror(err));
      exit(1);
    }
    err = opus_encoder_ctl(opus_encoder, OPUS_SET_BITRATE(BITRATE));

    swr_ctx = swr_alloc_set_opts(NULL,
                av_get_default_channel_layout(CHANNELS), // out_ch_layout
                OUTPUT_SAMPLE_FORMAT,                    // out_sample_fmt
                SAMPLE_RATE,                             // out_sample_rate
                av_get_default_channel_layout(src_audio_codec_ctx->channels),   // in_ch_layout
                src_audio_codec_ctx->sample_fmt,         // in_sample_fmt
                src_audio_codec_ctx->sample_rate,        // in_sample_rate
                0,                                       // log_offset
                NULL);                                   // log_ctx

    ck(swr_init(swr_ctx));
    
    av_samples_alloc_array_and_samples(&audio_buffer, &buffer_line_size, 2, MAX_FRAME_SIZE, AV_SAMPLE_FMT_S16, 0);
    init_fifo(&fifo);
  }

};