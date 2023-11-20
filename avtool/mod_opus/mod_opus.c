//
//  mod_opus.c
//  avtool
//
//  Created by zhanwang-sky on 2023/11/20.
//

#include <stdlib.h>
#include <string.h>
#include "mod_opus.h"

av_opus_context_t* av_opus_init(bool decoding, bool encoding,
                                int sample_rate, int channels) {
  av_opus_context_t* context = NULL;
  int err = 0;

  // sanity check
  if (!decoding && !encoding) {
    return NULL;
  }

  if (!(context = malloc(sizeof(av_opus_context_t)))) {
    goto err_exit;
  }

  memset(context, 0, sizeof(av_opus_context_t));
  context->sample_rate = sample_rate;
  context->channels = channels;

  if (decoding) {
    context->decoder = opus_decoder_create(sample_rate, channels, &err);
    if (!context->decoder || err != OPUS_OK) {
      goto err_exit;
    }
  }

  if (encoding) {
    context->encoder = opus_encoder_create(sample_rate, channels, OPUS_APPLICATION_VOIP, &err);
    if (!context->encoder || err != OPUS_OK) {
      goto err_exit;
    }
  }

  return context;

err_exit:
  av_opus_destroy(context);
  return NULL;
}

void av_opus_destroy(av_opus_context_t* context) {
  if (context) {
    if (context->decoder) {
      opus_decoder_destroy(context->decoder);
      context->decoder = NULL;
    }
    if (context->encoder) {
      opus_encoder_destroy(context->encoder);
      context->encoder = NULL;
    }
    free(context);
  }
}

int av_opus_decode(av_opus_context_t* context,
                   const uint8_t* pkt, int pkt_len,
                   uint8_t* pcm, int samples) {
  int rc = 0;

  rc = opus_decode(context->decoder, pkt, pkt_len, (opus_int16*) pcm, samples, 0);

  return rc;
}
