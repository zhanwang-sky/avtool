//
//  media_dumper.cpp
//  avtool
//
//  Created by zhanwang-sky on 2023/11/26.
//

#include <new>
#include <sstream>
#include <stdexcept>
#include "media_dumper.hpp"

#define check_exit(rc, ecode) \
do { \
  if (rc < 0) { \
    rc = ecode; \
    goto exit; \
  } \
} while (0)

using namespace AVTool;

AudioDumper::AudioDumper(const std::string& filename,
                         enum AVSampleFormat sample_fmt,
                         const AVChannelLayout& channel_layout,
                         int sample_rate)
    : filename_(filename),
      in_sample_fmt_(sample_fmt),
      in_channels_(channel_layout.nb_channels),
      in_sample_rate_(sample_rate) {
  std::ostringstream oss;
  int rc = 0;

  // allocate the output media context
  avformat_alloc_output_context2(&oc_, NULL, NULL, filename.c_str());
  if (!oc_) {
    oss << "Could not deduce output format from file extension";
    goto err_exit;
  }

  fmt_ = oc_->oformat;

  // find the encoder
  codec_ = avcodec_find_encoder(fmt_->audio_codec);
  if (!codec_) {
    oss << "Could not find encoder for '" << avcodec_get_name(fmt_->audio_codec) << "'";
    goto err_exit;
  }

  // alloc codec context
  c_ = avcodec_alloc_context3(codec_);
  if (!c_) {
    oss << "Could not alloc encoding context";
    goto err_exit;
  }
  c_->sample_fmt = codec_->sample_fmts ? codec_->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
  av_channel_layout_copy(&c_->ch_layout, &channel_layout);
  c_->sample_rate = sample_rate;
  if (fmt_->flags & AVFMT_GLOBALHEADER) {
    c_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
  }

  // open the codec
  rc = avcodec_open2(c_, codec_, NULL);
  if (rc < 0) {
    oss << "Could not open audio codec: " << av_err2str(rc);
    goto err_exit;
  }

  // add stream
  st_ = avformat_new_stream(oc_, NULL);
  if (!st_) {
    oss << "Could not allocate stream";
    goto err_exit;
  }
  st_->id = oc_->nb_streams - 1;
  st_->time_base = {1, c_->sample_rate};

  // copy the stream parameters to the muxer
  rc = avcodec_parameters_from_context(st_->codecpar, c_);
  if (rc < 0) {
    oss << "Could not copy the stream parameters";
    goto err_exit;
  }

  av_dump_format(oc_, 0, filename.c_str(), 1);

  // alloc frame
  frame_ = av_frame_alloc();
  if (!frame_) {
    oss << "Error allocating audio frame";
    goto err_exit;
  }
  frame_->format = c_->sample_fmt;
  av_channel_layout_copy(&frame_->ch_layout, &c_->ch_layout);
  frame_->sample_rate = c_->sample_rate;
  frame_->nb_samples = (codec_->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE)
                       ? max_frame_size : c_->frame_size;
  rc = av_frame_get_buffer(frame_, 0);
  if (rc < 0) {
    oss << "Could not allocate frame data";
    goto err_exit;
  }

  // alloc pkt
  pkt_ = av_packet_alloc();
  if (!pkt_) {
    oss << "Could not allocate AVPacket";
    goto err_exit;
  }

  frame_size_ = frame_->nb_samples;

  // Alloc AVFifo & Resampler
  if (!(codec_->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE)
      || (c_->sample_fmt != sample_fmt)) {
    af_ = av_audio_fifo_alloc(c_->sample_fmt, c_->ch_layout.nb_channels, max_frame_size);
    if (!af_) {
      oss << "Could not allocate FIFO";
      goto err_exit;
    }
    if (c_->sample_fmt != sample_fmt) {
      resampler_ = new(std::nothrow) Resampler(sample_fmt, channel_layout, sample_rate,
                                               c_->sample_fmt, c_->ch_layout, c_->sample_rate);
      if (!resampler_ || !(*resampler_)) {
        oss << "Could not create resampler";
        goto err_exit;
      }
    }
  }

  // open the output file, if needed
  if (!(fmt_->flags & AVFMT_NOFILE)) {
    rc = avio_open(&oc_->pb, filename.c_str(), AVIO_FLAG_WRITE);
    if (rc < 0) {
      oss << "Could not open '" << filename << "': " << av_err2str(rc);
      goto err_exit;
    }
    need_close_ = true;
  }

  rc = avformat_write_header(oc_, NULL);
  if (rc < 0) {
    oss << "Error occurred when opening output file: " << av_err2str(rc);
    goto err_exit;
  }
  need_trailer_ = true;

  return;

err_exit:
  clean();
  throw std::runtime_error(oss.str());
}

AudioDumper::AudioDumper(AudioDumper&& rhs)
  noexcept
    : filename_(rhs.filename_),
      in_sample_fmt_(rhs.in_sample_fmt_),
      in_channels_(rhs.in_channels_),
      in_sample_rate_(rhs.in_sample_rate_),
      fmt_(rhs.fmt_),
      codec_(rhs.codec_),
      oc_(rhs.oc_),
      c_(rhs.c_),
      st_(rhs.st_),
      frame_(rhs.frame_),
      pkt_(rhs.pkt_),
      frame_size_(rhs.frame_size_),
      af_(rhs.af_),
      resampler_(rhs.resampler_),
      need_close_(rhs.need_close_),
      need_trailer_(rhs.need_trailer_),
      samples_count_(rhs.samples_count_) {
  rhs.reset();
}

AudioDumper& AudioDumper::operator=(AudioDumper&& rhs) noexcept {
  if (this != &rhs) {
    clean();

    filename_ = rhs.filename_;
    in_sample_fmt_ = rhs.in_sample_fmt_;
    in_channels_ = rhs.in_channels_;
    in_sample_rate_ = rhs.in_sample_rate_;

    fmt_ = rhs.fmt_;
    codec_ = rhs.codec_;
    oc_ = rhs.oc_;
    c_ = rhs.c_;
    st_ = rhs.st_;

    frame_ = rhs.frame_;
    pkt_ = rhs.pkt_;
    frame_size_ = rhs.frame_size_;

    af_ = rhs.af_;
    resampler_ = rhs.resampler_;

    need_close_ = rhs.need_close_;
    need_trailer_ = rhs.need_trailer_;

    samples_count_ = rhs.samples_count_;

    rhs.reset();
  }
  return *this;
}

AudioDumper::~AudioDumper() {
  clean();
}

int AudioDumper::dump(const uint8_t* const* audio_data, int nb_samples) {
  int rc = 0;

  // sanity check
  if (nb_samples < 0 || nb_samples > max_frame_size) {
    return INT_MIN;
  }

  if (af_) {
    // 1. caching
    if (resampler_) {
      rc = resampler_->resample(af_, audio_data, nb_samples);
      check_exit(rc, -1);
    } else {
      if (audio_data) {
        rc = av_audio_fifo_write(af_,
                                 const_cast<void* const*>(reinterpret_cast<const void* const*>(audio_data)),
                                 nb_samples);
        check_exit(rc, -2);
      }
    }

    // 2. framing (use codec default frame size)
    while (true) {
      int fifo_sz = av_audio_fifo_size(af_);
      int min_frame_sz = !(codec_->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE) ? c_->frame_size : 1;
      if (fifo_sz < min_frame_sz) {
        break;
      }
      rc = av_frame_make_writable(frame_);
      check_exit(rc, -3);
      rc = av_audio_fifo_read(af_, reinterpret_cast<void**>(frame_->extended_data), frame_size_);
      check_exit(rc, -4);
      frame_->nb_samples = rc;
      frame_->pts = samples_count_;
      samples_count_ += rc;
      rc = avcodec_send_frame(c_, frame_);
      check_exit(rc, -5);
      rc = receive_n_write_packet();
      check_exit(rc, -6);
    }

    // 3. flushing
    if (!audio_data) {
      while (av_audio_fifo_size(af_) > 0) {
        rc = av_frame_make_writable(frame_);
        check_exit(rc, -7);
        rc = av_audio_fifo_read(af_, reinterpret_cast<void**>(frame_->extended_data), frame_size_);
        check_exit(rc, -8);
        frame_->nb_samples = rc;
        frame_->pts = samples_count_;
        samples_count_ += rc;
        rc = avcodec_send_frame(c_, frame_);
        check_exit(rc, -9);
        rc = receive_n_write_packet();
        check_exit(rc, -10);
      }
      rc = avcodec_send_frame(c_, NULL);
      check_exit(rc, -11);
      rc = receive_n_write_packet();
      check_exit(rc, -12);
    }
  } else {
    AVFrame* pframe = NULL;
    // caching & framing & flushing
    if (audio_data) {
      rc = av_frame_make_writable(frame_);
      check_exit(rc, -13);
      rc = av_samples_copy(frame_->extended_data, const_cast<uint8_t* const*>(audio_data),
                           0, 0, nb_samples, in_channels_, in_sample_fmt_);
      check_exit(rc, -14);
      frame_->nb_samples = nb_samples;
      frame_->pts = samples_count_;
      samples_count_ += nb_samples;
      pframe = frame_;
    }
    rc = avcodec_send_frame(c_, pframe);
    check_exit(rc, -15);
    rc = receive_n_write_packet();
    check_exit(rc, -16);
  }

  return 0;

exit:
  return rc;
}

void AudioDumper::clean() {
  if (need_trailer_) {
    av_write_trailer(oc_);
    need_trailer_ = false;
  }

  if (c_) {
    avcodec_free_context(&c_);
  }

  if (frame_) {
    av_frame_free(&frame_);
  }

  if (pkt_) {
    av_packet_free(&pkt_);
  }

  if (resampler_) {
    delete resampler_;
    resampler_ = NULL;
  }

  if (af_) {
    av_audio_fifo_free(af_);
    af_ = NULL;
  }

  if (need_close_) {
    avio_closep(&oc_->pb);
    need_close_ = false;
  }

  if (oc_) {
    avformat_free_context(oc_);
    oc_ = NULL;
  }

  reset();
}

void AudioDumper::reset() {
  fmt_ = NULL;
  codec_ = NULL;
  oc_ = NULL;
  c_ = NULL;
  st_ = NULL;

  frame_ = NULL;
  pkt_ = NULL;
  frame_size_ = 0;

  af_ = NULL;
  resampler_ = NULL;

  need_close_ = false;
  need_trailer_ = false;

  samples_count_ = 0;
}

int AudioDumper::receive_n_write_packet() {
  int rc = 0;

  do {
    // receive packet
    rc = avcodec_receive_packet(c_, pkt_);
    if (rc == AVERROR(EAGAIN) || rc == AVERROR_EOF) {
      rc = 0;
      break;
    } else if (rc < 0) {
      rc = -1;
      break;
    }
    // write packet
    rc = av_interleaved_write_frame(oc_, pkt_);
    if (rc < 0) {
      rc = -2;
      break;
    }
  } while (true);

  return rc;
}
