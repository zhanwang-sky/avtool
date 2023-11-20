//
//  mod_opus.h
//  avtool
//
//  Created by zhanwang-sky on 2023/11/20.
//

#ifndef mod_opus_h
#define mod_opus_h

#include <stdbool.h>
#include <opus/opus.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  int sample_rate;
  int channels;
  OpusDecoder* decoder;
  OpusEncoder* encoder;
} av_opus_context_t;

av_opus_context_t* av_opus_init(bool decoding, bool encoding,
                                int sample_rate, int channels);

void av_opus_destroy(av_opus_context_t* context);

int av_opus_decode(av_opus_context_t* context,
                   const uint8_t* pkt, int pkt_len,
                   uint8_t* pcm, int samples);

#ifdef __cplusplus
}
#endif

#endif /* mod_opus_h */
