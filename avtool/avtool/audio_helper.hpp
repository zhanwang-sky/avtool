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

class SamplesBuffer {
 public:
  SamplesBuffer(const SamplesBuffer&) = delete;
  SamplesBuffer& operator=(const SamplesBuffer&) = delete;

  SamplesBuffer(int channels, int samples, enum AVSampleFormat fmt) noexcept;

  SamplesBuffer(SamplesBuffer&& rhs) noexcept;

  SamplesBuffer& operator=(SamplesBuffer&& rhs) noexcept;

  virtual ~SamplesBuffer();

  bool operator!() const;

  uint8_t** get();

 protected:
  void clean();

  void reset();

 private:
  uint8_t** buf_ = NULL;
};

class Resampler {
 public:
  Resampler(const Resampler&) = delete;
  Resampler& operator=(const Resampler&) = delete;

  Resampler(enum AVSampleFormat in_sample_fmt,  const AVChannelLayout& in_chlayout,  int in_sample_rate,
            enum AVSampleFormat out_sample_fmt, const AVChannelLayout& out_chlayout, int out_sample_rate) noexcept;

  Resampler(Resampler&& rhs) noexcept;

  Resampler& operator=(Resampler&& rhs) noexcept;

  virtual ~Resampler();

  bool operator!() const;

  int resample(AVAudioFifo* af, const uint8_t* const* audio_data, int nb_samples);

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
