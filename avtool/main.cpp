//
//  main.cpp
//  avtool
//
//  Created by zhanwang-sky on 2023/11/20.
//

#include <iostream>
#include "tlv_reader.hpp"

using std::cout;
using std::cerr;
using std::endl;

uint8_t pkt_buf[384];

int main(int argc, char* argv[]) {
  if (argc != 2) {
    cerr << "Usage: ./avtool {dump.tlv}\n";
    exit(EXIT_FAILURE);
  }

  avTLVReader tlv_reader{argv[1]};
  if (!tlv_reader.is_open()) {
    cerr << "Fail to open tlv file '" << argv[1] << "'\n";
    exit(EXIT_FAILURE);
  }

  uint8_t marker = 0;
  uint16_t seq = 0;
  uint32_t rtp_ts = 0;
  uint64_t cap_ts = 0;
  int tlv_len = 0;

  while ((tlv_len =
          tlv_reader.read(pkt_buf, sizeof(pkt_buf), marker, seq, rtp_ts, cap_ts)) > 0) {
    cout << (marker ? '*' : ' ')
         << " seq=" << seq
         << ", rtp_ts=" << rtp_ts
         << ", cap_ts=" << cap_ts
         << endl;
  }

  cout << "break, rc=" << tlv_len << endl;

  return 0;
}
