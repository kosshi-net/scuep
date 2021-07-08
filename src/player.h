#ifndef SCUEP_PLAYER_H
#define SCUEP_PLAYER_H

#include "database.h"

#include <stdint.h>
#include <threads.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>


struct DecoderState {
	thrd_t 			thread;
	struct ScuepTrack *track;

	AVFormatContext *format;
	AVStream        *stream;

	AVCodec         *codec;
	AVCodecContext  *codec_ctx;
	AVPacket        *packet;
	AVFrame         *frame;
};


struct PlayerState {
	
	mtx_t mutex; // Unusaed


	uint8_t *data;
	size_t size;

	uint32_t channels; 
	uint32_t period; 
	uint32_t frames; 

	uint32_t sizeof_frame;
	uint32_t sizeof_sample;
	
	// Positions in ring buffer
	uint32_t head; // Writer
	uint32_t tail; // Reader

	// Totals
	uint64_t frames_played;
	uint64_t frames_decoded;

	/* These are used to keep track of the real position in audio track.
	 *
	 * Decoder is allowed to begin decoding the next track before playback is
	 * actually finished; seamless transitions.
	 */
	uint64_t progress;      
	uint64_t progress_new;  
	uint64_t progress_swap; 

	TrackId track_id;
	TrackId track_id_new;

	enum AVSampleFormat format;

	// Handle to close whatever sound server is running
	int (*sndsvr_close)(void);

	struct DecoderState av;

};


void player_init();

int player_load(TrackId);

int player_play();

int player_pause();

int player_stop();



#endif
