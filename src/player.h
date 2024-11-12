#ifndef SCUEP_PLAYER_H
#define SCUEP_PLAYER_H

#include "database.h"

#include <stdint.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <threads.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>


/* Temporary debug stuff */

void debug_quit_decoder(void);

/* Structures */

struct DecoderState {
	thrd_t 			thread;
	int 			thread_run;

	struct ScuepTrack *track;

	AVFormatContext *format;
	AVStream        *stream;

	AVCodec         *codec;
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
	uint32_t frames;

	uint32_t sizeof_frame;
	uint32_t sizeof_sample;

	TrackId  track_id; /* TODO Will be removed? */

	// Atomics accessed locklessly, make sure value changes mid-function are
	// not an issue, eg sample and cache the values. Make sure to do full
	// writes.

	struct {
		TrackId  track_id;

		_Atomic uint64_t ring;          // Position in ring buffer
		_Atomic uint64_t total;         // Total frames decoded
		_Atomic bool     done;

		_Atomic uint64_t stream_changed;
		_Atomic uint64_t stream_offset;

	} head;

	struct {
		TrackId  track_id;

		_Atomic uint64_t ring;
		_Atomic uint64_t total;          // Total frames played
		_Atomic bool     done;

		_Atomic uint64_t stream_changed;
		_Atomic uint64_t stream_offset;
	} tail;

	enum AVSampleFormat format;

	// Handle to close whatever sound server is running
	int (*sndsvr_close)(void);

	struct DecoderState av;

};

// Not necessary to call.
void player_init(void);

int      player_seek(float);
int      player_seek_relative(float);

float    player_position_seconds(void);
float    player_duration_seconds(void);

int player_toggle(void);
int player_play(void);
int player_pause(void);
int player_stop(void);

// PlayerInfo is the public state.
struct PlayerInfo {
	// When unitialized (stopped), player is NULL. All values are invalid.
	// You may access the struct at your own risk for additional information.
	struct PlayerState *player;

	// Pause state
	bool   	            paused;

	// Currently playing track. Decoder may be decoding a different one.
	TrackId             track_id;

	// In seconds
	float               duration;
	float               progress;

	bool next_available; /* TODO Will be removed? */

};

/* Do not free or edit returned struct. Not thread safe. */
const struct PlayerInfo *player_get_info(void);

int player_load(TrackId);

// Debugging purposes
struct PlayerState *_get_playerstate();

#endif
