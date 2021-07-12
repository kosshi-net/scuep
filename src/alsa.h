#ifndef SCUEP_ALSA_H
#define SCUEP_ALSA_H

#include <libavcodec/avcodec.h>

#include "player.h"

int alsa_open( struct PlayerState* );
int alsa_close(void);

#endif
