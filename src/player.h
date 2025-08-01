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

	uint8_t *data;  /* Ring buffer */
	size_t   size;  /* Total buffer size in bytes */

	uint32_t channels;
	uint32_t sample_rate;
	uint32_t frames; /* Frame length of ring buffer */

	uint32_t sizeof_frame;
	uint32_t sizeof_sample;

	bool preload_failed;

	/* 
	 * Atomics accessed locklessly, make sure writes don't have races
	 * TODO investigate where atomics are actually necessary
	 *
	 * Head - Decoder
	 *   - Written to by decoder, read by audio thread
	 * Tail - Playing audio
	 *   - Written to and read only by audio thread
	 *
	 * Position etc information on frontend should be displayed from tail's
	 * status. Head can be decoding an entirely different track, a lot of
	 * metadata stored here to keep track of that.
	 *
	 * Ring buffer
	 * ring  - Frame position in ring buffer
	 * total - Total frames decoded,
	 *
	 * head.stream_changed - When stream last changed, in total frame position
	 * tail.stream_changed - When stream will change, in total frame position
	 *
	 * When tail.total reaches head.stream_changed, head metadata is copied
	 * to tail.
	 *
	 * state_key     - Arbitrary "txid" value for frontend tracking.
	 * stream_offset - Used to store the seek offset to calculate true progress
	 * stream_lenght - Lenght of current stream in frames
	 */

	struct {
		_Atomic uint64_t ring;
		_Atomic uint64_t total;
		_Atomic bool     done;

		_Atomic TrackId  track_id;
		_Atomic uint32_t state_key;

		_Atomic uint64_t stream_changed;
		_Atomic uint64_t stream_offset;
		_Atomic uint64_t stream_length;

	} head;

	struct {
		_Atomic uint64_t ring;
		_Atomic uint64_t total;
		_Atomic bool     done;

		_Atomic TrackId  track_id;
		_Atomic uint32_t state_key;

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
