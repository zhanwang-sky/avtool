//
//  main.cpp
//  avtool
//
//  Created by zhanwang-sky on 2023/11/20.
//

#include <iostream>
#include <memory>
#include <stdexcept>
#include "avtool/audio_helper.hpp"
#include "avtool/media_dumper.hpp"
#include "mod_opus/mod_opus.h"
#include "tlv_reader.hpp"

#define SAMPLE_RATE 16000
#define NR_CHANNELS 1
#define SAMPLES_PER_FRAME 320
#define MAX_SAMPLES_CACHE 16384

using std::cout;
using std::cerr;
using std::endl;

using audio_fifo_ptr = std::unique_ptr<AVAudioFifo, decltype(&av_audio_fifo_free)>;
using opus_ctx_ptr = std::unique_ptr<av_opus_context_t, decltype(&av_opus_destroy)>;

uint8_t pkt_buf[512];

int main(int argc, char* argv[]) {
  if (argc != 3) {
    cerr << "Usage: ./avtool {dump.tlv} {dump.wav}\n";
    exit(EXIT_FAILURE);
  }

  AVChannelLayout ch_layout = (NR_CHANNELS > 1)
                              ? AVChannelLayout(AV_CHANNEL_LAYOUT_STEREO)
                              : AVChannelLayout(AV_CHANNEL_LAYOUT_MONO);

  AVTool::Resampler resampler(AV_SAMPLE_FMT_S16, ch_layout, SAMPLE_RATE,
                              AV_SAMPLE_FMT_FLTP, ch_layout, SAMPLE_RATE);
  if (!resampler) {
    cerr << "Fail to create resampler\n";
    exit(EXIT_FAILURE);
  }

  audio_fifo_ptr audio_fifo(av_audio_fifo_alloc(AV_SAMPLE_FMT_FLTP, NR_CHANNELS, MAX_SAMPLES_CACHE),
                            &av_audio_fifo_free);
  if (!audio_fifo) {
    cerr << "Fail to create audio fifo\n";
    exit(EXIT_FAILURE);
  }

  opus_ctx_ptr opus_ctx(av_opus_init(true, false, SAMPLE_RATE, NR_CHANNELS),
                        &av_opus_destroy);
  if (!opus_ctx) {
    cerr << "Fail to init opus context\n";
    exit(EXIT_FAILURE);
  }

  avTLVReader tlv_reader(argv[1]);
  if (!tlv_reader.is_open()) {
    cerr << "Fail to open tlv file '" << argv[1] << "'\n";
    exit(EXIT_FAILURE);
  }

  uint8_t** s16_buf = NULL;
  uint8_t** fltp_buf = NULL;

  try {
    int rc = 0;
    int linesize = 0;

    rc = av_samples_alloc_array_and_samples(&s16_buf, &linesize, NR_CHANNELS, MAX_SAMPLES_CACHE, AV_SAMPLE_FMT_S16, 0);
    if (rc < 0) {
      throw std::runtime_error("Fail to alloc s16 buf");
    }

    rc = av_samples_alloc_array_and_samples(&fltp_buf, &linesize, NR_CHANNELS, MAX_SAMPLES_CACHE, AV_SAMPLE_FMT_FLTP, 0);
    if (rc < 0) {
      throw std::runtime_error("Fail to alloc fltp buf");
    }

    AVTool::AudioDumper audio_dumper(argv[2], AV_SAMPLE_FMT_FLTP, ch_layout, SAMPLE_RATE);

    uint8_t marker = 0;
    uint16_t seq = 0;
    uint32_t rtp_ts = 0;
    uint64_t cap_ts = 0;
    int tlv_len = 0;
    int samples_decoded = 0;

    while ((tlv_len = tlv_reader.read(pkt_buf, sizeof(pkt_buf), marker, seq, rtp_ts, cap_ts)) > 0) {
      cout << "seq=" << seq
           << (marker ? "#" : "")
           << ", rtp_ts=" << rtp_ts
           << ", cap_ts=" << cap_ts
           << endl;

      samples_decoded = av_opus_decode(opus_ctx.get(),
                                       pkt_buf + tlv_reader.header_len,
                                       tlv_len - tlv_reader.header_len,
                                       s16_buf[0], SAMPLES_PER_FRAME);
      cout << samples_decoded << " samples decoded\n";
      if (samples_decoded <= 0) {
        cout << "decode error, skip\n";
        continue;
      }

      rc = resampler.resample(audio_fifo.get(), s16_buf, samples_decoded);
      cout << rc << " samples converted\n";
      if (rc <= 0) {
        cout << "resample error, skip\n";
        continue;
      }

      rc = av_audio_fifo_read(audio_fifo.get(), reinterpret_cast<void**>(fltp_buf), MAX_SAMPLES_CACHE);
      cout << rc << " samples read\n";
      if (rc <= 0) {
        cout << "read samples error, skip\n";
        continue;
      }

      audio_dumper.dump(fltp_buf, rc);
      cout << rc << " samples write\n";
    }

    // flush
    audio_dumper.dump(NULL, 0);

    cout << "break, rc=" << tlv_len << endl;

  } catch (std::exception &e) {
    cerr << "Error: " << e.what() << endl;

    if (s16_buf) {
      if (s16_buf[0]) {
        av_freep(&s16_buf[0]);
      }
      av_freep(&s16_buf);
    }

    if (fltp_buf) {
      if (fltp_buf[0]) {
        av_freep(&fltp_buf[0]);
      }
      av_freep(&fltp_buf);
    }

    exit(EXIT_FAILURE);

  }

  return 0;
}
