//
//  main.cpp
//  avtool
//
//  Created by zhanwang-sky on 2023/11/20.
//

#include <iostream>
#include <memory>
#include <stdexcept>

#include <rubberband/RubberBandStretcher.h>
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

  RubberBand::RubberBandStretcher stretcher(SAMPLE_RATE, NR_CHANNELS,
                                            RubberBand::RubberBandStretcher::OptionProcessRealTime
                                            | RubberBand::RubberBandStretcher::OptionEngineFiner);
  stretcher.setPitchScale(1.35);

  avTLVReader tlv_reader(argv[1]);
  if (!tlv_reader.is_open()) {
    cerr << "Fail to open tlv file '" << argv[1] << "'\n";
    exit(EXIT_FAILURE);
  }

  AVTool::SamplesBuffer s16_buf(NR_CHANNELS, MAX_SAMPLES_CACHE, AV_SAMPLE_FMT_S16);
  if (!s16_buf) {
    cerr << "Fail to alloc s16 buf\n";
    exit(EXIT_FAILURE);
  }

  AVTool::SamplesBuffer fltp_buf(NR_CHANNELS, MAX_SAMPLES_CACHE, AV_SAMPLE_FMT_FLTP);
  if (!fltp_buf) {
    cerr << "Fail to alloc fltp buf\n";
    exit(EXIT_FAILURE);
  }

  AVTool::SamplesBuffer stretched_buf(NR_CHANNELS, MAX_SAMPLES_CACHE, AV_SAMPLE_FMT_FLTP);
  if (!stretched_buf) {
    cerr << "Fail to alloc stretched buf\n";
    exit(EXIT_FAILURE);
  }

  try {
    AVTool::AudioDumper audio_dumper(argv[2], AV_SAMPLE_FMT_FLTP, ch_layout, SAMPLE_RATE);

    uint8_t marker = 0;
    uint16_t seq = 0;
    uint32_t rtp_ts = 0;
    uint64_t cap_ts = 0;
    int tlv_len = 0;
    int samples = 0;

    while ((tlv_len = tlv_reader.read(pkt_buf, sizeof(pkt_buf), marker, seq, rtp_ts, cap_ts)) > 0) {
      cout << "seq=" << seq
           << (marker ? "#" : "")
           << ", rtp_ts=" << rtp_ts
           << ", cap_ts=" << cap_ts
           << endl;

      samples = av_opus_decode(opus_ctx.get(),
                               pkt_buf + tlv_reader.header_len,
                               tlv_len - tlv_reader.header_len,
                               s16_buf.get()[0], SAMPLES_PER_FRAME);
      cout << samples << " samples decoded\n";
      if (samples <= 0) {
        cout << "decode error(" << samples << ")\n";
        continue;
      }

      samples = resampler.resample(audio_fifo.get(), s16_buf.get(), samples);
      cout << samples << " samples converted\n";
      if (samples <= 0) {
        cout << "resample error(" << samples << ")\n";
        continue;
      }

      samples = av_audio_fifo_read(audio_fifo.get(), reinterpret_cast<void**>(fltp_buf.get()), MAX_SAMPLES_CACHE);
      cout << samples << " samples read\n";
      if (samples <= 0) {
        cout << "fifo error(" << samples << ")\n";
        continue;
      }

      stretcher.process(reinterpret_cast<float**>(fltp_buf.get()), samples, false);

      samples = stretcher.available();
      cout << samples << " samples stretched\n";
      if (samples <= 0) {
        cout << "not available, skip\n";
        continue;
      }

      stretcher.retrieve(reinterpret_cast<float**>(stretched_buf.get()), samples);

      audio_dumper.dump(stretched_buf.get(), samples);
      cout << samples << " samples write\n";
    }

    // flush
    audio_dumper.dump(NULL, 0);

    cout << "break " << tlv_len << endl;

  } catch (std::exception &e) {
    cerr << "Error: " << e.what() << endl;
    exit(EXIT_FAILURE);
  }

  return 0;
}
