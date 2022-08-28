#include "database.h"
#include "player.h"
#include "util.h"

#include "alsa.h"
#include "log.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include <threads.h> 

#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
//#include <libswresample/swresample.h>

/*
 * Function declarations
 */
static int decoder_load(TrackId, float);
static int player_write( AVFrame*, int, int );
static void player_write_blank_period();
static int  decoder_loop(void*arg);
static void decoder_start(void);
static void decoder_stop(void);
static void decoder_free(void);

/*
 * File globals
 */
static struct PlayerState *player = NULL;
static struct PlayerInfo   info;

/* Debug stuff */

static struct {
	bool decoder_quit;
} debug;
void debug_quit_decoder()
{
	debug.decoder_quit = !debug.decoder_quit;
}


/* Function impl */

struct PlayerState *_get_playerstate()
{
	return player;
}

const struct PlayerInfo *player_get_info(void)
{
	info.player = player; 

	if(info.player == NULL) return &info;

	info.paused   = player->pause;
	info.track_id = player->tail.track_id;

	info.progress = player_position_seconds();
	info.duration = player_duration_seconds();

	info.next_available = false;

	if( player->head.done 
	&&  player->head.total == player->tail.total )
		info.next_available = true;

	return &info;
}

void print_averr(int err){
	char errstr[512];
	av_make_error_string(errstr, 512, err);
	scuep_logf("AVERR %i: %s\n", err, errstr);
}

/*
 * Time bases?
 *
 * In struct ScuepTrack: milliseconds as integer
 * In between: as sample counter
 * In FFMPEG: Whatever it feels like
 * 
 * Conversion from samples to ffmpeg's timebase is nice with av_rescale_q,
 * should scueptrack timestamps be samples as well?
 */

void player_init(void){
	if(!player) player = calloc(sizeof(struct PlayerState), 1);
}

int player_play(void){
	if(!player) return 1;
	player->pause = 0;
	return 0;
}
int player_pause(void){
	if(!player) return 1;
	player->pause = 1;
	return 0;
}
int player_toggle(void){
	if(!player) return 1;
	player->pause = !player->pause;
	return 0;
}


int player_load(TrackId track_id)
{
	if(!player)
		player_init();
	decoder_load(track_id, 0.0);
	decoder_start();
	if(!player->sndsvr_close) alsa_open( player );
	return 0;
}

int player_seek(float seconds)
{
	struct PlayerState *this = player;
	if(!this) return -1;

	decoder_load(this->tail.track_id, seconds);
	decoder_start();
	if(!player->sndsvr_close) alsa_open( player );
	return 0;
}


int player_seek_relative(float seconds)
{
	struct PlayerState *this = player;
	if(!this) return -1;

	float seek = player_position_seconds() + seconds;

	seek = MAX(0, seek);

	player_seek(seek);
	return 0;
}

float player_position_seconds(void)
{
	struct PlayerState *this = player;
	if(!this) return 0.0f;
	
	float pos = this->tail.total   -
	            this->tail.stream_changed  + 
	            this->tail.stream_offset; 
	return pos / (float) this->sample_rate;
}

float player_duration_seconds(void)
{
	/* TODO fix wrong duration when decoding next track */
	struct PlayerState *this = player;
	if(!this || !this->av.track) return 0.0f;
	return this->av.track->length / 1000.0f;
}

TrackId player_current_track()
{
	if(!player) return 0;
	return player->tail.track_id;
}

int player_stop(void)
{
	struct PlayerState *this = player;
	if(!this) return 1;

	if( this->sndsvr_close ) this->sndsvr_close();
	decoder_free();
	scuep_logf("Freeing player\n");

	if( this->data ) {
		free(this->data);
		this->data = NULL;
	}
	free( player );

	player = NULL; 

	return 0;
}


int player_reconfig( AVCodecParameters *param, bool flush )
{
	struct PlayerState *this = player;

	this->period      = 1024;
	this->frames      = this->period * 215;
	
	if (this->channels    != param->channels
	||	this->sample_rate != param->sample_rate
	||	this->format      != param->format
	||  this->pause       == true 
	){
		scuep_logf("Hard reconfig\n");
		if( this->sndsvr_close ) this->sndsvr_close();
		if( this->data ) {
			free(this->data);
			this->data = NULL;
		}

		this->channels    = param->channels;
		this->sample_rate = param->sample_rate;
		this->format      = param->format;
		
		this->sizeof_sample = av_get_bytes_per_sample(param->format);
		this->sizeof_frame  = this->sizeof_sample * this->channels;
		this->size          = this->frames * this->sizeof_frame;

		this->data    = malloc(this->size);
		if(1){
			for( int i = 0; i < this->size; i++ )
				this->data[i] = rand();
		}
		this->head.ring = 0;
		this->tail.ring = 0;
		this->head.total = 0;
		this->tail.total = 0;
	} else {
		if (flush) { // cut buffer
			this->head.total = this->tail.total + this->period;
			this->head.ring  = this->tail.ring  + this->period;
			this->head.ring %= this->frames;
		}
		scuep_logf("Soft reconfig\n");
	}

	scuep_logf("Buffer time: %f seconds\n", this->frames / (float)param->sample_rate);

	
	return 0;
}

void decoder_free()
{
	if(!player) return;

	decoder_stop();
	struct DecoderState *this = &player->av;
	
	if(this->format)    avformat_close_input(&this->format);
	// AVCodec is freed by format?
	if(this->codec_ctx) avcodec_free_context(&this->codec_ctx);

	if(this->packet)    av_packet_free(&this->packet);
	if(this->frame)     av_frame_free(&this->frame);

	if(this->track)     this->track = track_free(this->track);

	this->stream = NULL;
}

int decoder_load(TrackId track_id, float seek)
{
	scuep_logf("Decoder load %i seek %f\n", track_id, seek);
	struct DecoderState *this = &player->av;
	int ret = 0;
	
	decoder_free();

	player->head.track_id = track_id;

	this->track     = track_load(track_id);
	if(!this->track){
		scuep_logf("Failed to load track (database error?)\n");
		return -1;
	}
	struct ScuepTrack *track = this->track;

	if (avformat_open_input(&this->format, track->path, NULL, NULL) != 0) {
		scuep_logf("Could not open file '%s'\n", track->path);
		return -1;
	}

	if (avformat_find_stream_info(this->format, NULL) < 0) {
		scuep_logf("Could not retrieve stream info from file '%s'\n", track->path);
		return -1;
    }

	/*****************
	 * SELECT STREAM *
	 *****************/

	scuep_logf( "Track has %i stream(s)\n", this->format->nb_streams  );
    int stream_index = -1;
    for (int i = 0; i < this->format->nb_streams; i++) 
	{
		enum AVMediaType type = this->format->streams[i]->codecpar->codec_type;

		const char *stream_type = av_get_media_type_string( type ); // Leak??

		scuep_logf( "Stream %i type: %s\n", i, stream_type);
        if (type == AVMEDIA_TYPE_AUDIO) stream_index = i;
    }
	if(stream_index== -1){
		scuep_logf( "No suitable stream!\n");
		return -1;
	}

	this->stream = this->format->streams[stream_index];

	AVCodecParameters *param = this->stream->codecpar; 

	/*****************************
	 * SEEK & INIT FFMPEG DECODE *
	 *****************************/

	int sample_rate = param->sample_rate;
	AVRational sample_scale = { .den = sample_rate, .num = 1 };

	if(track->length < 1) {
		scuep_logf("Recalculating duration...\n");
		int64_t recalc_dur =  av_rescale_q(
			this->stream->duration,
			this->stream->time_base,
			sample_scale
		);
		this->track->length = recalc_dur / (sample_rate) * 1000.0f - track->start;
	}

	int __seek   = seek * sample_rate;
	int __start  = track->start   * (sample_rate/1000.0f) + __seek;
	int __length = track->length  * (sample_rate/1000.0f) - __seek;
	scuep_logf("Frames: %i + %i, seek %i\n", __start, __length, __seek);
	


	int64_t seek_to = av_rescale_q(
			__start, 
			sample_scale,
			this->stream->time_base
		);
	
	player->head.stream_offset = __seek;

	ret = av_seek_frame( this->format, stream_index, seek_to, AVSEEK_FLAG_BACKWARD );
	if(ret<0) goto error;

	this->codec = avcodec_find_decoder( param->codec_id );
	this->codec_ctx = avcodec_alloc_context3( this->codec );

	ret = avcodec_parameters_to_context( this->codec_ctx, param );
	if(ret<0) goto error;
	
	ret = avcodec_open2(this->codec_ctx, this->codec, NULL);
	if(ret<0) goto error;

	this->packet = av_packet_alloc();
	this->frame  = av_frame_alloc();


	int sizeof_sample = av_get_bytes_per_sample(param->format);
	int channels    = param->channels;
	if(channels != 2) {
		scuep_logf("Channel count of %i is not yet supported", channels);
		goto error;
	}
	
	{ // Print info about codecs and stuff
		const char *format_str = av_get_sample_fmt_name(param->format);
		scuep_logf( "%ihz %ic %s (%ib) %s\n", 
				param->sample_rate, 
				channels,
				format_str, 
				sizeof_sample, 
				this->codec->long_name 
			);
		scuep_logf("Timebase: %i / %i\n", this->stream->time_base.den, this->stream->time_base.num);
	}
	
	/**************
	 * OPEN AUDIO *
	 **************/

	scuep_logf("Open audio\n");
	player_reconfig( param, true );
	scuep_logf("Reconfig ok\n");

	player->head.stream_changed  = player->head.total;
	player->head.done     = 0;

	return 0;
error:
	scuep_logf("Error happened!\n");
	print_averr(ret);
	return -1;
}

void decoder_start()
{
	struct DecoderState *this  = &player->av;
	if (!this->thread_run)
		thrd_create( &this->thread, &decoder_loop, NULL );
}

void decoder_stop()
{
	struct DecoderState *this  = &player->av;
	if (this->thread_run) {
		scuep_logf("Stopping decoder thread\n");
		this->thread_run = 0;
		thrd_join(this->thread, NULL);
	}
}

int decoder_loop(void*arg)
{
	struct DecoderState *this  = &player->av;
	this->thread_run = 1;
	struct ScuepTrack   *track = this->track;
	int ret = 0;
	
	scuep_logf("Decode thread started\n");

	AVCodecParameters *param = this->stream->codecpar; 
	int sample_rate = param->sample_rate;
	AVRational sample_scale = { .den = sample_rate, .num = 1 };
	int __start  = track->start   * (sample_rate*0.001) ;
	int __length = track->length  * (sample_rate*0.001) ;

	while(this->thread_run){
		av_packet_unref(this->packet);
		ret = av_read_frame(this->format, this->packet);
		if(ret < 0) goto error;

		if( this->packet->stream_index != this->stream->index ) continue;
		
		ret  = avcodec_send_packet( this->codec_ctx, this->packet );
		if (ret < 0) {
			scuep_logf("Send broke! %i\n", ret);
			print_averr(ret);
			break;
		}

		while( ret >= 0 && this->thread_run ){

			ret = avcodec_receive_frame( this->codec_ctx, this->frame);
			

			int64_t sample_pos = av_rescale_q(
					this->frame->pts, 
					this->stream->time_base, 
					sample_scale 
				);

			int64_t samples = this->frame->nb_samples;
			
			if(sample_pos > __start+__length || debug.decoder_quit) {
				debug.decoder_quit = false;
				player->head.done = true;
				goto finish;
			}
			if(!samples)        continue;

			int cut_front = MAX( __start - sample_pos, 0);
			int cut_back  = MAX( (sample_pos+samples)-(__start+__length), 0);
			int total     = samples-cut_front-cut_back;

			if (cut_front || cut_back){
				scuep_logf("%li, Frame %i+%i front %i, back %i\n", 
						sample_pos,
						this->codec_ctx->frame_number, 
						this->frame->nb_samples, 
						cut_front, total
				);
			}
			
			player_write(this->frame, cut_front, total);
		}
	}

finish:	
	player_write_blank_period();
	av_packet_unref(this->packet);
	scuep_logf("Decoder quit!\n");
	this->thread_run = 0;
	return 0;
error:
	av_packet_unref(this->packet);
	scuep_logf("Error happened!\n");
	this->thread_run = 0;
	print_averr(ret);
	return -1;
}

void player_write_blank_period()
{
	struct PlayerState *this = player;

	uint64_t head = this->head.ring;
	uint64_t next = (head / this->period ) * this->period;
	
	if(next == head) return;	
	
	next += this->period;
	uint64_t total = next - head;

	memset(
		this->data + (head * this->sizeof_frame),
		0,
		total * this->sizeof_frame
	);

	head += total;
	head %= this->frames;
	this->head.ring = head;
	this->head.total += total;

	return;
}

int player_write( 
	AVFrame *packet, 
	int cut_front, 
	int total 
){
	struct PlayerState *this = player;

	int interleaved = !av_sample_fmt_is_planar(packet->format);
	
	int left = total;
	int packet_written = cut_front;

	uint32_t head = this->head.ring;

	scuep_logf("DECODER_WRITE %i\n", head);

	/* Loop until the whole packet has been written out */
	while (left != 0 && this->av.thread_run) {

		int available = this->frames - 
			           (this->head.total - 
		                this->tail.total);
		
		available = MIN(available, this->frames - head);
		available = MIN(available, left);


		if(available == 0){
			sleep_ms(5);
			continue;
		}

		if(interleaved){
			memcpy( 
				this->data      + (head     * this->sizeof_frame), 
				packet->data[0] + (packet_written * this->sizeof_frame),  
				available * this->sizeof_frame
			);
		} else {
			/* Nobody should use MP3 anyway */
			int i = head * this->sizeof_frame;
			for (int f = 0; f < available; f++)           // Frame
			for (int c = 0; c < packet->channels; c++)    // Channel
			for (int b = 0; b < this->sizeof_sample; b++) // Byte
			{
				this->data[i++] = packet->data[c][ 
					(f+packet_written) * this->sizeof_sample + b
				];
			}
		} 

		head += available;
		head %= this->frames;
		this->head.ring = head;
		this->head.total += available;

		packet_written += available;
		left -= available;
	}
	return 0;
}


