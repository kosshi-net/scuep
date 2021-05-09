#ifndef SCUEP_AUDIO_BUFFER_H
#define SCUEP_AUDIO_BUFFER_H

#include <stdint.h>
#include <threads.h>
#include <libavcodec/avcodec.h>

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

	uint64_t frames_played; // This is real position in audio stream
	uint64_t frames_decoded;

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
