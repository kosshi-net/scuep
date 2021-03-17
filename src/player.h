#ifndef SCUEP_PLAYER_H
#define SCUEP_PLAYER_H

#include "database.h"

/*
 * Decodes and plays the tracks. 
 * 
 */

enum {
	PLAYER_PLAY,
	PLAYER_PAUSE,
	PLAYER_STOP,
	PLAYER_FINISH,
	PLAYER_ERROR
};


void player_init();

int player_load(TrackId);

void player_play(TrackId optional);

int player_get_state( TrackId, uint32_t *progress );

/*
 * Stop button would act as a soft deinit. Disconnect from sound servers etc
 */
void player_stop();



#endif
