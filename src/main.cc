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
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

int main(int argc, char** argv) {
    RtpStreamConfig video_config;
  video_config.dst_address = "127.0.0.1";
  video_config.dst_port = 20002;
  video_config.src_port = 20000;
  video_config.payload_type = 101;
  video_config.ssrc = 1111;
  video_config.clock_rate = 90000;
  video_config.rtcp_mux = false;
  video_config.src_port_rtcp = 20001;
  video_config.dst_port_rtcp = 20003;

  RtpStreamConfig audio_config;
  audio_config.dst_address = "127.0.0.1";
  audio_config.dst_port = 20012;
  audio_config.src_port = 20010;
  audio_config.payload_type = 102;
  audio_config.ssrc = 2222;
  audio_config.clock_rate = 48000;
  audio_config.rtcp_mux = false;
  audio_config.src_port_rtcp = 20011;
  audio_config.dst_port_rtcp = 20013;

  if (argc > 0) {
    if (true) {

      std::string id = "channel1";
      std::string url = "http://localhost:4300/_data/sesame-streams/callcenter." + id;
      cpr::Response r = cpr::Get(cpr::Url{url});
      r.status_code;                  // 200
      r.header["content-type"];       // application/json; charset=utf-8
      std::cout << "response: " << r.text << std::endl;                         // JSON text string
      json ex1 = json::parse(r.text);
      const auto ch_id = ex1["id"].get<std::string>();
      const auto connection = ex1["connection"];
      const auto video = ex1["video"];
      const auto audio = ex1["audio"];
      video_config.dst_address = connection["host"].get<std::string>();
      video_config.dst_port = connection["videoPortDest"].get<uint16_t>();
      video_config.src_port = connection["videoPortSrc"].get<uint16_t>();
      video_config.payload_type = stoi(video["payloadType"].get<std::string>());
      video_config.ssrc = stoi(video["id"].get<std::string>());
      video_config.rtcp_mux = true;

      audio_config.dst_address = connection["host"].get<std::string>();
      audio_config.dst_port = connection["audioPortDest"].get<uint16_t>();
      audio_config.src_port = connection["audioPortSrc"].get<uint16_t>();
      audio_config.payload_type = stoi(audio["payloadType"].get<std::string>());
      audio_config.ssrc = stoi(audio["id"].get<std::string>());
      audio_config.rtcp_mux = true;
    }
  }
  uint32_t ts = 0;
  const auto frame_dur = 90000 / 25;
  const auto frame_dur_ms = 1000 / 25;

  const RtpSession rtp_session;
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
    std::cout << "dfiff: " << diff << std::endl;
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