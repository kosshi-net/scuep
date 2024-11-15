#ifndef SCUEP_PLAYER_H
#define SCUEP_PLAYER_H

#include "database.h"

#include <stdint.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <threads.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

/*
 * Backend design notes
 *
 *  1. Tracks must be played raw to the audio server, no resampling allowed
 *  2. Tracks must be played gaplessly, when 1st condition allows
 *  Gapless here means that there are no pauses or hitches when tracks change.
 *
 * */


/* Temporary debug stuff */

void debug_quit_decoder(void);

/* Structures */

struct DecoderState {
	thrd_t          thread;
	int             thread_run;

	struct ScuepTrack *track;

	AVFormatContext *format;
	AVStream        *stream;

	const AVCodec   *codec;
	AVCodecContext  *codec_ctx;
	AVPacket        *packet;
	AVFrame         *frame;
};


struct PlayerState {

	_Atomic bool pause;

	uint8_t *data;
	size_t size;

	uint32_t channels;
	uint32_t sample_rate;
	uint32_t period;
	uint32_t frames; /* Frame lenght of ring buffer */

	uint32_t sizeof_frame;
	uint32_t sizeof_sample;

	bool preload_failed;

	/* Atomics accessed locklessly, make sure value changes mid-function are
	   not an issue, eg sample and cache the values. Make sure to do full
	   writes.

	   TODO investigate where atomics are actually necessary
	*/


	/* Head - the currently decoding track, can differ from tail */
	struct {
		TrackId  track_id;
		/* state_key is tracking data for the frontend */
		uint32_t state_key;

		_Atomic uint64_t ring;          // Position in ring buffer
		_Atomic uint64_t total;         // Total frames decoded
		_Atomic bool     done;

		_Atomic uint64_t stream_changed;
		_Atomic uint64_t stream_offset;
		_Atomic uint64_t stream_length;

	} head;

	/* Tail - the currently playing track */
	struct {
		TrackId  track_id;
		uint32_t state_key;

		_Atomic uint64_t ring;
		_Atomic uint64_t total;          // Total frames played
		_Atomic bool     done;

		_Atomic uint64_t stream_changed;
		_Atomic uint64_t stream_offset;
		_Atomic uint64_t stream_length;
	} tail;

	enum AVSampleFormat format;

	/* Handle to close whatever sound server is running */
	int (*sndsvr_close)(void);

	struct DecoderState av;

};

/* Not necessary to call. */
void player_init(void);

int      player_seek(float);
int      player_seek_relative(float);

float    player_position_seconds(void);
float    player_duration_seconds(void);

uint32_t player_state_key(void);

int player_toggle(void);
int player_play(void);
int player_pause(void);
int player_stop(void);

int player_load(TrackId, uint32_t state_key, bool preload);

struct PlayerState *_get_playerstate(void);

#endif
