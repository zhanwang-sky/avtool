//
//  media_dumper.hpp
//  avtool
//
//  Created by zhanwang-sky on 2023/11/26.
//

#ifndef media_dumper_hpp
#define media_dumper_hpp

#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/opt.h>
#include <libavutil/timestamp.h>
}

#include "audio_helper.hpp"

namespace AVTool {

class AudioDumper {
 public:
  static constexpr int max_frame_size = 16384;

  AudioDumper(const AudioDumper&) = delete;
  AudioDumper& operator=(const AudioDumper&) = delete;

  AudioDumper(const std::string& filename,
              enum AVSampleFormat sample_fmt,
              const AVChannelLayout& channel_layout,
              int sample_rate);

  AudioDumper(AudioDumper&&) noexcept;
  AudioDumper& operator=(AudioDumper&&) noexcept;

  virtual ~AudioDumper();

  int dump(const uint8_t* const* audio_data, int nb_samples);

 protected:
  void clean();

  void reset();

 private:
  int receive_n_write_packet();

  std::string filename_;
  enum AVSampleFormat in_sample_fmt_;
  int in_channels_;
  int in_sample_rate_;

  const AVOutputFormat* fmt_ = NULL;
  const AVCodec* codec_ = NULL;
  AVFormatContext* oc_ = NULL;
  AVCodecContext* c_ = NULL;
  AVStream* st_ = NULL;

  AVFrame* frame_ = NULL;
  AVPacket* pkt_ = NULL;
  int frame_size_ = 0;

  AVAudioFifo* af_ = NULL;
  Resampler* resampler_ = NULL;

  bool need_close_ = false;
  bool need_trailer_ = false;

  uint64_t samples_count_ = 0;
};

}

#endif /* media_dumper_hpp */
