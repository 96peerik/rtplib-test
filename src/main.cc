#include <iostream>
#include <fstream>
#include <vector>
#include <memory>
#include <chrono>
#include <thread>

#include <rtplib/rtp-session.h>
#include "utils.h"

#include "testing/ffmpeg-reader.h"
#include "testing/ffmpeg-encoder.h"
#include "testing/opus-encoder.h"

int main() {
  uint32_t ts = 0;
  const auto frame_dur = 90000 / 25;
  const auto frame_dur_ms = 1000 / 25;

  const RtpSession rtp_session;
  RtpStreamConfig video_config;
  video_config.dst_address = "127.0.0.1";
  video_config.dst_port = 20002;
  video_config.src_port = 20000;
  video_config.payload_type = 101;
  video_config.ssrc = 1111;
  video_config.clock_rate = 90000;

  RtpStreamConfig audio_config;
  audio_config.dst_address = "127.0.0.1";
  audio_config.dst_port = 20012;
  audio_config.src_port = 20010;
  audio_config.payload_type = 102;
  audio_config.ssrc = 2222;
  audio_config.clock_rate = 48000;

  const std::shared_ptr<RtpStream> rtp_stream_video = std::make_shared<RtpStream>(video_config);
  const std::shared_ptr<RtpStream> rtp_stream_audio = std::make_shared<RtpStream>(audio_config);

  FFmpegEncoder encoder;
    int64_t last_frame_ts = 0;

  encoder.on_video_packet_encoded = [&](AVPacket* pkt, uint32_t pts) {
    std::cout << "video packet: " << pkt->size << " pts: " << pts << std::endl;
    rtp_stream_video->send_h264(pkt->data, pkt->size, pts);
      const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
      .count();
    const auto diff = now - last_frame_ts;
    last_frame_ts = now;
    if (diff < frame_dur_ms) {
      const auto sleep_time = frame_dur_ms - diff;
      std::cout << "diff: " << diff << " sleeping: " << sleep_time << std::endl;
      std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time));
    }

  };

  OpusEncoder opus_encoder;
  opus_encoder.on_audio_packet_encoded = [&](uint8_t* data, uint32_t size, uint32_t pts) {
    std::cout << "audio packet: " << size << " pts: " << pts << std::endl;
    rtp_stream_audio->send(data, size, pts);
  };

  FFmpegReader reader("c:\\ost\\2022_07_05_Malmo_Reykjavik_1080i_4ch.mov");
  const auto audio_ctx = reader.get_audio_codec_context();
  if (audio_ctx != nullptr) {
    opus_encoder.init(audio_ctx);
  }

  encoder.init(reader.get_video_codec_context());
  
  reader.on_video_frame = [&](AVFrame* frame, uint8_t* data, size_t size) {
    encoder.encode(frame);
  };

  reader.on_audio_frame = [&](AVFrame* frame) {
    opus_encoder.encode_audio(frame);
  };

  reader.decode(); 

  return 0;
}