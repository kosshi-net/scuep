#ifndef SCUEP_PLAYER_H
#define SCUEP_PLAYER_H

#include "database.h"

#include <stdint.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <threads.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

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

	// Atomics accessed locklessly, make sure value changes mid-function are 
	// not an issue, eg sample and cache the values. Make sure to do full 
	// writes. 
	
	// Positions in ring buffer
	_Atomic uint32_t head; // decoder
	_Atomic uint32_t tail; // sndsvr

	// Totals
	_Atomic	uint64_t frames_played; // sndsvr
	_Atomic	uint64_t frames_decoded; // decoder

	uint64_t position_since; 
	uint64_t position_offset; 

	TrackId track_id;
	TrackId track_id_new;

	enum AVSampleFormat format;

	// Handle to close whatever sound server is running
	int (*sndsvr_close)(void);

	struct DecoderState av;

};


void player_init();

int player_load(TrackId);

// Absolute seek
int      player_seek(float);
uint64_t player_position(void);

int player_play();

int player_pause();

int player_stop();



#endif
