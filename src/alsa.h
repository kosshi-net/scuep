#ifndef SCUEP_ALSA_H
#define SCUEP_ALSA_H

#include <libavcodec/avcodec.h>

#include "audiobuffer.h"

int  alsa_open( AVCodecParameters*, struct AudioBuffer* );
void alsa_close(void);

#endif
