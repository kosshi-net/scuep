#include "alsa.h"
#include "log.h"
#include "player.h"

#include <alsa/asoundlib.h>
#include <libavcodec/avcodec.h>

#include <stdint.h>
#include <stdbool.h>

#include <threads.h>

static char *device = "default";
static snd_pcm_t *handle = NULL;

static struct PlayerState *player = NULL;

static thrd_t thread;
static int    thread_run = 0;

// A period of silence
static uint8_t *silence = NULL; 


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

int alsa_open( AVCodecParameters *params, struct PlayerState *_player )
{
	int err;
	player = _player;

	snd_pcm_format_t format = format_av2alsa( params->format );

	if ((err = snd_pcm_open(&handle, device, SND_PCM_STREAM_PLAYBACK, 0)) < 0){
        scuep_logf("Playback open error: %s\n", snd_strerror(err));
		return -1;
    }
	
	silence = calloc( player->period, player->sizeof_frame );

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

	player->sndsvr_close = alsa_close;

	printf("Alsa open OK\n");

	return 0;
}

int alsa_close(void)
{
	player->sndsvr_close = NULL;
	thread_run = 0;
	thrd_join(thread, NULL);

	return 0;
}


int alsa_loop(void*arg)
{
	printf("Thread OK\n");
	thread_run = 1;

	snd_pcm_sframes_t  frames;
	while(thread_run){
		
		printf( "played: %li bufhp: %li\n", 
			player->frames_played,
			player->frames_decoded - player->frames_played 
		);

		if( player->frames_played + player->period > player->frames_decoded ){
			snd_pcm_writei(handle,
				silence,
				player->period
			);
			continue;
		}

		frames = snd_pcm_writei(handle,
			player->data + player->tail * player->sizeof_frame,
			player->period
		);
			
		if( frames < 0 ) {
			// TODO Proper error handling !
			printf("Alsa error? %s\n", snd_strerror(frames));
			snd_pcm_prepare(handle);
			continue;
		}

		player->tail += frames;
		player->tail %= player->frames;
		player->frames_played += frames;

	}

	// TODO Don't clip
	
	snd_pcm_drop(handle);
	snd_pcm_close(handle);
	handle = NULL;

	return 0;
}

