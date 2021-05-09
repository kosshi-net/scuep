#include "alsa.h"
#include "log.h"
#include "audiobuffer.h"

#include <alsa/asoundlib.h>
#include <libavcodec/avcodec.h>

#include <stdint.h>
#include <stdbool.h>

#include <threads.h>

char *device = "default";
snd_output_t *output = NULL;

snd_pcm_t         *handle;

struct AudioBuffer *abuf = NULL;

thrd_t thread;

// A period of silence
uint8_t *silence = NULL; 


snd_pcm_format_t format_av2alsa( enum AVSampleFormat f ){

	switch(f){
		case AV_SAMPLE_FMT_FLT:
		case AV_SAMPLE_FMT_FLTP:
			return SND_PCM_FORMAT_FLOAT_LE;
		
		case AV_SAMPLE_FMT_S16:
		case AV_SAMPLE_FMT_S16P:
			return SND_PCM_FORMAT_S16_LE;

		case AV_SAMPLE_FMT_S32:
		case AV_SAMPLE_FMT_S32P:
			return SND_PCM_FORMAT_S32_LE;

		default:
			return SND_PCM_FORMAT_UNKNOWN;

	}

}

static int alsa_loop(void*arg);

int alsa_open( AVCodecParameters *params, struct AudioBuffer *_abuf )
{
	int err;
	abuf = _abuf;

	snd_pcm_format_t format = format_av2alsa( params->format );

	if ((err = snd_pcm_open(&handle, device, SND_PCM_STREAM_PLAYBACK, 0)) < 0){
        scuep_logf("Playback open error: %s\n", snd_strerror(err));
		return -1;
    }
	
	silence = calloc( abuf->period, abuf->sizeof_frame );

    if ((err = snd_pcm_set_params(handle,
		format,
		SND_PCM_ACCESS_RW_INTERLEAVED,
		params->channels,
		params->sample_rate,
		1,     // soft resample
		20000  // latency
		) ) < 0
	){   
        scuep_logf("Playback open error: %s\n", snd_strerror(err));
		return -1;
    }

	thrd_create( &thread, &alsa_loop, NULL );

	printf("Alsa open OK\n");

	return 0;
}

void alsa_close(void){

}


int alsa_loop(void*arg){
	printf("Thread OK\n");


	snd_pcm_sframes_t  frames;
	while(1){
		
		printf( "played: %i bufhp: %i\n", 
			abuf->frames_played,
			abuf->frames_decoded - abuf->frames_played 
		);

		if( abuf->frames_played + abuf->period > abuf->frames_decoded ){
			snd_pcm_writei(handle,
				silence,
				abuf->period
			);
			continue;
		}

		frames = snd_pcm_writei(handle,
			abuf->data + abuf->tail * abuf->sizeof_frame,
			abuf->period
		);
			
		if( frames < 0 ) {
			// TODO Proper error handling !
			printf("Alsa error? %s\n", snd_strerror(frames));
			snd_pcm_prepare(handle);
			continue;
		}

		abuf->tail += frames;
		abuf->tail %= abuf->frames;
		abuf->frames_played += frames;

	}

	return 0;
}

