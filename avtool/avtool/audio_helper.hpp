//
//  audio_helper.hpp
//  avtool
//
//  Created by zhanwang-sky on 2023/11/24.
//

#ifndef audio_helper_hpp
#define audio_helper_hpp

extern "C" {
#include <libavutil/audio_fifo.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
}

namespace AVTool {

AVFrame* alloc_audio_frame(enum AVSampleFormat sample_fmt,
                           const AVChannelLayout* channel_layout,
                           int sample_rate, int nb_samples);

class Resampler {
 public:
  Resampler(const Resampler&) = delete;
  Resampler& operator=(const Resampler&) = delete;

  Resampler(enum AVSampleFormat in_sample_fmt,  const AVChannelLayout& in_chlayout,  int in_sample_rate,
            enum AVSampleFormat out_sample_fmt, const AVChannelLayout& out_chlayout, int out_sample_rate);

  Resampler(Resampler&& rhs) noexcept;

  Resampler& operator=(Resampler&& rhs) noexcept;

  virtual ~Resampler();

  int resample(AVAudioFifo* af, const uint8_t** audio_data, int nb_samples);

 protected:
  void clean();

  void reset();

 private:
  enum AVSampleFormat in_sample_fmt_;
  int in_channels_;
  int in_sample_rate_;

  enum AVSampleFormat out_sample_fmt_;
  int out_channels_;
  int out_sample_rate_;

  struct SwrContext* swr_ = NULL;
  uint8_t** audio_data_ = NULL;
  int max_samples_ = 0;
};

}

#endif /* audio_helper_hpp */
