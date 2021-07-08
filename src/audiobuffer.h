#ifndef SCUEP_AUDIO_BUFFER_H
#define SCUEP_AUDIO_BUFFER_H

#include <stdint.h>
#include <threads.h>
#include <libavcodec/avcodec.h>
#include "database.h"

struct AudioBuffer {
	
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

};

struct AudioBuffer *
audiobuf_create( AVCodecParameters *param );

int 
audiobuf_write( 
	struct AudioBuffer *abuf, 
	AVFrame *frame, 
	int cut_front, 
	int total 
);

void 
audiobuf_free( struct AudioBuffer* );



#endif
