#include "alsa.h"
#include "log.h"
#include "player.h"
#include "util.h"

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
	
	silence = calloc( player->period, player->sizeof_frame );
	snd_pcm_format_t format = format_av2alsa( player->format );

	if ((err = snd_pcm_open(&pcm, device, SND_PCM_STREAM_PLAYBACK, 0)) < 0){
        scuep_logf("Playback open error: %s\n", snd_strerror(err));
		return -1;
    }
	
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
	
	player->sndsvr_close = alsa_close;
	thrd_create( &thread, &alsa_loop, NULL );

	scuep_logf("Alsa open OK\n");

	return 0;
}

int alsa_close(void)
{
	if(thread_run){
		player->sndsvr_close = NULL;
		thread_run = 0;
		thrd_join(thread, NULL);
	}
	if(silence)
		free(silence);
	silence = NULL;

	return 0;
}


int alsa_loop(void*arg)
{
	scuep_logf("ALSA Thread open\n");
	thread_run = 1;

	player->tail.track_id       = player->head.track_id;
	player->tail.stream_changed = player->head.stream_changed;
	player->tail.stream_offset  = player->head.stream_offset;

	snd_pcm_sframes_t  frames = 0;
	snd_pcm_prepare(pcm);

	while(thread_run){
		
		if( player->tail.stream_changed != player->head.stream_changed 
		&&  player->tail.total          >= player->head.stream_changed
		){
			player->tail.track_id       = player->head.track_id;
			player->tail.stream_changed = player->head.stream_changed;
			player->tail.stream_offset  = player->head.stream_offset;
		}

		uint32_t tail = player->tail.ring;


		int total = MIN(player->head.total - player->tail.total, player->period);

		if( total < player->period
		||  player->pause 
		){
			snd_pcm_writei(pcm,
				silence,
				player->period
			);
			continue;
		}


		while( total > 0 ){
			frames = snd_pcm_writei(pcm,
				player->data + tail * player->sizeof_frame,
				total
			);
			scuep_logf("PCM FRAMES %li %li\n", frames, total);
			if(frames < 0) break;
			total -= frames;
		}
		

		if( frames < 0 ) {
			// TODO Proper error handling !
			scuep_logf("Alsa error %s\n", snd_strerror(frames));
			//snd_pcm_prepare(pcm);
			int ok = snd_pcm_recover(pcm, frames, 0);
			if(!ok){
				scuep_logf("Failed to recover. State undefined.\n");
			}
			continue;
		}

		tail += frames;
		tail %= player->frames;
		player->tail.ring = tail;

		player->tail.total += frames;

	}

	snd_pcm_drop(pcm);
	snd_pcm_close(pcm);
	pcm = NULL;

	player->sndsvr_close = NULL;
	thread_run = 0;
	scuep_logf("ALSA Thread close\n");
	player->pause = 1;
	return 0;
}

