//
//  audio_helper.cpp
//  avtool
//
//  Created by zhanwang-sky on 2023/11/24.
//

#include "audio_helper.hpp"

using namespace AVTool;

SamplesBuffer::SamplesBuffer(int channels, int samples,
                             enum AVSampleFormat fmt) noexcept {
  av_samples_alloc_array_and_samples(&buf_, NULL, channels, samples, fmt, 0);
}

SamplesBuffer::SamplesBuffer(SamplesBuffer&& rhs) noexcept
    : buf_(rhs.buf_) {
  rhs.reset();
}

SamplesBuffer& SamplesBuffer::operator=(SamplesBuffer&& rhs) noexcept {
  if (this != &rhs) {
    clean();

    buf_ = rhs.buf_;

    rhs.reset();
  }
  return *this;
}

SamplesBuffer::~SamplesBuffer() {
  clean();
}

bool SamplesBuffer::operator!() const {
  return !buf_;
}

uint8_t** SamplesBuffer::get() {
  return buf_;
}

void SamplesBuffer::clean() {
  if (buf_) {
    if (buf_[0]) {
      av_freep(&buf_[0]);
    }
    av_freep(&buf_);
  }
}

void SamplesBuffer::reset() {
  buf_ = NULL;
}

Resampler::Resampler(enum AVSampleFormat in_sample_fmt,  const AVChannelLayout& in_chlayout,  int in_sample_rate,
                     enum AVSampleFormat out_sample_fmt, const AVChannelLayout& out_chlayout, int out_sample_rate)
  noexcept
    : in_sample_fmt_(in_sample_fmt),
      in_channels_(in_chlayout.nb_channels),
      in_sample_rate_(in_sample_rate),
      out_sample_fmt_(out_sample_fmt),
      out_channels_(out_chlayout.nb_channels),
      out_sample_rate_(out_sample_rate) {
  // create resampler context
  swr_ = swr_alloc();
  if (!swr_) {
    goto err_exit;
  }

  // set options
  av_opt_set_sample_fmt(swr_, "in_sample_fmt", in_sample_fmt, 0);
  av_opt_set_chlayout(swr_, "in_chlayout", &in_chlayout, 0);
  av_opt_set_int(swr_, "in_sample_rate", in_sample_rate, 0);

  av_opt_set_sample_fmt(swr_, "out_sample_fmt", out_sample_fmt, 0);
  av_opt_set_chlayout(swr_, "out_chlayout", &out_chlayout, 0);
  av_opt_set_int(swr_, "out_sample_rate", out_sample_rate, 0);

  // initialize the resampling context
  if (swr_init(swr_) < 0) {
    goto err_exit;
  }

  return;

err_exit:
  clean();
}

Resampler::Resampler(Resampler&& rhs) noexcept
    : in_sample_fmt_(rhs.in_sample_fmt_),
      in_channels_(rhs.in_channels_),
      in_sample_rate_(rhs.in_sample_rate_),
      out_sample_fmt_(rhs.out_sample_fmt_),
      out_channels_(rhs.out_channels_),
      out_sample_rate_(rhs.out_sample_rate_),
      swr_(rhs.swr_),
      audio_data_(rhs.audio_data_),
      max_samples_(rhs.max_samples_) {
  rhs.reset();
}

Resampler& Resampler::operator=(Resampler&& rhs) noexcept {
  if (this != &rhs) {
    clean();

    in_sample_fmt_ = rhs.in_sample_fmt_;
    in_channels_ = rhs.in_channels_;
    in_sample_rate_ = rhs.in_sample_rate_;

    out_sample_fmt_ = rhs.out_sample_fmt_;
    out_channels_ = rhs.out_channels_;
    out_sample_rate_ = rhs.out_sample_rate_;

    swr_ = rhs.swr_;
    audio_data_ = rhs.audio_data_;
    max_samples_ = rhs.max_samples_;

    rhs.reset();
  }
  return *this;
}

Resampler::~Resampler() {
  clean();
}

bool Resampler::operator!() const {
  return !swr_;
}

int Resampler::resample(AVAudioFifo* af,
                        const uint8_t* const* audio_data, int nb_samples) {
  int max_samples = 0;
  int out_samples = 0;
  int rc = 0;

  // sanity check
  if (!af || nb_samples < 0) {
    return INT_MIN;
  }

  if (!(*this)) {
    return INT_MIN + 1;
  }

  // calculate max output samples
  max_samples = swr_get_out_samples(swr_, nb_samples);
  if (max_samples < 0) {
    return -1;
  } else if (max_samples == 0) {
    return 0;
  }

  // realloc buffer
  if (max_samples_ < max_samples) {
    if (audio_data_ && audio_data_[0]) {
      av_freep(&audio_data_[0]);
    }
  }

  if (!audio_data_ || !audio_data_[0]) {
    int linesize = 0;
    if (!audio_data_) {
      rc = av_samples_alloc_array_and_samples(&audio_data_, &linesize,
                                              out_channels_, max_samples,
                                              out_sample_fmt_, 0);
    } else {
      rc = av_samples_alloc(&audio_data_[0], &linesize,
                            out_channels_, max_samples, out_sample_fmt_, 0);
    }
    if (rc < 0) {
      return -2;
    }
    max_samples_ = max_samples;
  }

  // resample
  out_samples = swr_convert(swr_,
                            audio_data_, max_samples,
                            const_cast<const uint8_t**>(audio_data), nb_samples);
  if (out_samples < 0) {
    return -3;
  }

  // write to fifo
  if (out_samples > 0) {
    rc = av_audio_fifo_write(af, reinterpret_cast<void**>(audio_data_), out_samples);
    if (rc != out_samples) {
      return -4;
    }
  }

  return out_samples;
}

void Resampler::clean() {
  if (swr_) {
    swr_free(&swr_);
  }
  if (audio_data_) {
    if (audio_data_[0]) {
      av_freep(&audio_data_[0]);
    }
    av_freep(&audio_data_);
  }
}

void Resampler::reset() {
  swr_ = NULL;
  audio_data_ = NULL;
}
