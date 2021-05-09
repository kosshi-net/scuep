#include "audiobuffer.h"
#include "util.h"
#include <stdint.h>
#include <threads.h>
#include <libavcodec/avcodec.h>

struct AudioBuffer *audiobuf_create( AVCodecParameters *param )
{
	struct AudioBuffer *abuf = calloc( sizeof (struct AudioBuffer), 1 );


	abuf->period = 1024;
	abuf->channels = param->channels;
	abuf->format  = param->format;
	abuf->frames  = abuf->period * 10;

	abuf->sizeof_sample = av_get_bytes_per_sample(param->format);
	abuf->sizeof_frame  = abuf->sizeof_sample * abuf->channels;

	abuf->size = abuf->frames * abuf->sizeof_frame;
	
	printf("Buffer frames: %i\n", abuf->frames);

	abuf->data    = malloc(abuf->size);

	return abuf;
}

int audiobuf_write( 
	struct AudioBuffer *this, 
	AVFrame *packet, 
	int cut_front, 
	int total 
){
	int interleaved = !av_sample_fmt_is_planar(packet->format);
	
	int left = total;
	int packet_written = cut_front;

	// Loop until the whole packet has been written out
	while( left != 0 ){

		int available = this->frames - 
			           (this->frames_decoded - 
		                this->frames_played);
		
		available = MIN( available, this->frames - this->head );
		available = MIN( available, left );


		if(available == 0){
			sleep_ms(5);
			continue;
		}

		if(interleaved){
			memcpy( 
				this->data      + (this->head     * this->sizeof_frame), 
				packet->data[0] + (packet_written * this->sizeof_frame),  
				available * this->sizeof_frame
			);
		} else {
			// Nobody should use MP3 anyway
			int i = this->head * this->sizeof_frame;
			for( int f = 0; f < available; f++ )           // Frame
			for( int c = 0; c < packet->channels; c++ )    // Channel
			for( int b = 0; b < this->sizeof_sample; b++ ) // Byte
			{
				this->data[i++] = packet->data[c][ 
					(f+packet_written) * this->sizeof_sample + b
				];
			}
		} 

		this->head += available;
		this->head %= this->frames;
		this->frames_decoded += available;

		packet_written += available;
		left -= available;
	}
	return 0;
}

void audiobuf_free( struct AudioBuffer *this )
{
	free(this->data);
	free(this);
}
