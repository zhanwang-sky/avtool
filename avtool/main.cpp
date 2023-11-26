//
//  main.cpp
//  avtool
//
//  Created by zhanwang-sky on 2023/11/20.
//

#include <fstream>
#include <iostream>
#include <memory>
#include "avtool/audio_helper.hpp"
#include "mod_opus/mod_opus.h"
#include "tlv_reader.hpp"

#define SAMPLE_RATE 16000
#define NR_CHANNELS 1
#define SAMPLES_PER_FRAME 320

using std::cout;
using std::cerr;
using std::endl;

using opus_ctx_ptr = std::unique_ptr<av_opus_context_t, decltype(&av_opus_destroy)>;

uint8_t pkt_buf[384];
uint8_t frame_buf[SAMPLES_PER_FRAME * NR_CHANNELS * 2];

int main(int argc, char* argv[]) {
  if (argc != 3) {
    cerr << "Usage: ./avtool {dump.tlv} {dump.pcm}\n";
    exit(EXIT_FAILURE);
  }

  avTLVReader tlv_reader{argv[1]};
  if (!tlv_reader.is_open()) {
    cerr << "Fail to open tlv file '" << argv[1] << "'\n";
    exit(EXIT_FAILURE);
  }

  std::ofstream pcm_dumper{argv[2], std::ofstream::binary | std::ofstream::trunc};
  if (!pcm_dumper) {
    cerr << "Fail to open pcm file '" << argv[2] << "'\n";
    exit(EXIT_FAILURE);
  }

  opus_ctx_ptr opus_ctx{av_opus_init(true, false, SAMPLE_RATE, NR_CHANNELS), &av_opus_destroy};
  if (!opus_ctx) {
    cerr << "Fail to init opus context\n";
    exit(EXIT_FAILURE);
  }

  uint8_t marker = 0;
  uint16_t seq = 0;
  uint32_t rtp_ts = 0;
  uint64_t cap_ts = 0;
  int tlv_len = 0;
  int samples_decoded = 0;
  while ((tlv_len =
          tlv_reader.read(pkt_buf, sizeof(pkt_buf), marker, seq, rtp_ts, cap_ts)) > 0) {
    cout << (marker ? '*' : ' ')
         << " seq=" << seq
         << ", rtp_ts=" << rtp_ts
         << ", cap_ts=" << cap_ts
         << endl;
    samples_decoded = av_opus_decode(opus_ctx.get(),
                                     pkt_buf + tlv_reader.header_len,
                                     tlv_len - tlv_reader.header_len,
                                     frame_buf, SAMPLES_PER_FRAME);
    cout << samples_decoded << " samples decoded\n";
    if (samples_decoded > 0) {
      pcm_dumper.write((const char*) frame_buf, samples_decoded * NR_CHANNELS * 2);
    }
  }

  cout << "break, rc=" << tlv_len << endl;

  return 0;
}
