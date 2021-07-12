#include "alsa.h"
#include "log.h"
#include "player.h"

#include <alsa/asoundlib.h>
#include <libavcodec/avcodec.h>

#include <stdint.h>
#include <stdbool.h>

#include <threads.h>

static char *device = "default";
static snd_pcm_t *pcm = NULL;

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

int alsa_open( struct PlayerState *_player )
{
	int err;
	player = _player;
	
	alsa_close();

	snd_pcm_format_t format = format_av2alsa( player->format );

	if ((err = snd_pcm_open(&pcm, device, SND_PCM_STREAM_PLAYBACK, 0)) < 0){
        scuep_logf("Playback open error: %s\n", snd_strerror(err));
		return -1;
    }
	
	silence = calloc( player->period, player->sizeof_frame );

    if ((err = snd_pcm_set_params(pcm,
		format,
		SND_PCM_ACCESS_RW_INTERLEAVED,
		player->channels,
		player->sample_rate,
		1,     // soft resample
		50000  // latency
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
	if(thread_run){
		player->sndsvr_close = NULL;
		thread_run = 0;
		thrd_join(thread, NULL);
	}
	return 0;
}


int alsa_loop(void*arg)
{
	printf("ALSA Thread open\n");
	thread_run = 1;

	snd_pcm_sframes_t  frames;
	snd_pcm_prepare(pcm);
	while(thread_run){
		
		uint32_t tail = player->tail;

		printf( "played: %li bufhp: %li, h %i t %i\n", 
			player->frames_played,
			player->frames_decoded - player->frames_played ,
			player->head,
			tail
		);

		if( player->frames_played + player->period > player->frames_decoded
		||  player->pause 
		){
			snd_pcm_writei(pcm,
				silence,
				player->period
			);
			continue;
		}
		int total = player->period;

		while( total > 0 ){
			frames = snd_pcm_writei(pcm,
				player->data + tail * player->sizeof_frame,
				player->period
			);
			printf("PCM FRAMES %li %li\n", frames, total);
			if(frames < 0) break;
			total -= frames;
		}
		

		if( frames < 0 ) {
			// TODO Proper error handling !
			printf("Alsa error %s\n", snd_strerror(frames));
			//snd_pcm_prepare(pcm);
			int ok = snd_pcm_recover(pcm, frames, 0);
			if(!ok){
				printf("Failed to recover. State undefined.\n");
			}
			continue;
		}

		tail += frames;
		tail %= player->frames;
		player->tail = tail;

		player->frames_played += frames;


	}

	// TODO Don't clip
	
	snd_pcm_drop(pcm);
	snd_pcm_close(pcm);
	pcm = NULL;

	printf("ALSA Thread close\n");
	return 0;
}

