#include "database.h"
#include "player.h"
#include "util.h"

#include "alsa.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include <threads.h> 

#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
//#include <libswresample/swresample.h>

// Function declarations
int decoder_load(TrackId, float);
static int player_write( AVFrame*, int, int );
int decoder_loop(void*arg);
void decoder_start(void);
void decoder_stop(void);
void decoder_free(void);

static struct PlayerState *player = NULL;

void print_averr(int err){
	char errstr[512];
	av_make_error_string(errstr, 512, err);
	printf("AVERR %i: %s\n", err, errstr);
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
	player = calloc(sizeof(struct PlayerState), 1);
}

int player_play(void){
	player->pause = 0;
	return -1;
}
int player_pause(void){
	player->pause = 1;
	return -1;
}


int player_load(TrackId track_id){
	player->track_id = track_id;
	decoder_load(track_id, 0.0);
	decoder_start();
	sleep_ms(1000);
	if(!player->sndsvr_close) alsa_open( player );
	return 0;
}

int player_seek(float seconds){

	printf("SEEK %f\n", seconds);

	struct PlayerState *this = player;

	decoder_load(this->track_id, seconds);
	decoder_start();
	if(!this->sndsvr_close) alsa_open( this );
	return 0;
}






int player_stop(void)
{
	struct PlayerState *this = player;

	if( this->sndsvr_close ) this->sndsvr_close();
	decoder_free();
	printf("Freeing player\n");

	if( this->data ) {
		free(this->data);
		this->data = NULL;
	}
	free( player );

	player = NULL; 

	return 0;
}


int player_reconfig( AVCodecParameters *param )
{
	struct PlayerState *this = player;

	this->period      = 1024;
	this->frames      = this->period * 100;
	
	if(	this->channels    != param->channels
	||	this->sample_rate != param->sample_rate
	||	this->format      != param->format
	){
		printf("Hard reconfig\n");
		if( this->sndsvr_close ) this->sndsvr_close();
		if( this->data && 0) {
			free(this->data);
			this->data = NULL;
		}
		// Hard reconfig!
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
		this->tail = 0;
		this->head = 0;
		this->frames_played = 0;
		this->frames_decoded = 0;
	} else {
		if(1){ // cut buffer
			this->frames_decoded = this->frames_played + this->period;
			this->head = this->tail + this->period;
			this->head %= this->frames;
		}
		printf("Soft reconfig\n");
	}

	printf("Buffer time: %f seconds\n", this->frames / (float)param->sample_rate);

	
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

	this->stream = NULL;
	this->track = track_free(this->track);
}

int decoder_load(TrackId track_id, float seek)
{
	printf("SEEK %f\n", seek);
	struct DecoderState *this = &player->av;
	int ret = 0;
	
	decoder_free();

	this->track     = track_load(track_id);
	struct ScuepTrack *track = this->track;

	if (avformat_open_input(&this->format, track->path, NULL, NULL) != 0) {
		printf("Could not open file '%s'\n", track->path);
		return -1;
	}

	if (avformat_find_stream_info(this->format, NULL) < 0) {
		printf("Could not retrieve stream info from file '%s'\n", track->path);
		return -1;
    }

	/*****************
	 * SELECT STREAM *
	 *****************/

	printf( "Track has %i stream(s)\n", this->format->nb_streams  );
    int stream_index = -1;
    for (int i = 0; i < this->format->nb_streams; i++) 
	{
		enum AVMediaType type = this->format->streams[i]->codecpar->codec_type;

		const char *stream_type = av_get_media_type_string( type ); // Leak??

		printf( "Stream %i type: %s\n", i, stream_type);
        if (type == AVMEDIA_TYPE_AUDIO) stream_index = i;
    }
	if(stream_index== -1){
		printf( "No suitable stream!\n");
		return -1;
	}

	this->stream = this->format->streams[stream_index];

	AVCodecParameters *param = this->stream->codecpar; 

	/*****************************
	 * SEEK & INIT FFMPEG DECODE *
	 *****************************/

	int sample_rate = param->sample_rate;
	AVRational sample_scale = { .den = sample_rate, .num = 1 };
	int __seek   = seek * sample_rate;
	int __start  = track->start   * (sample_rate/1000.0f) + __seek;
	int __length = track->length  * (sample_rate/1000.0f) - __seek;
	printf("Frames: %i + %i, seek %i\n", __start, __length, __seek);

	int64_t seek_to = av_rescale_q(
			__start, 
			sample_scale,
			this->stream->time_base
		);
	
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
	int interleaved   = av_sample_fmt_is_planar(param->format);
	int channels    = param->channels;
	if(channels != 2) {
		printf("Channel count of %i is not yet supported", channels);
		goto error;
	}
	
	{ // Print info about codecs and stuff
		const char *format_str = av_get_sample_fmt_name(param->format);
		printf( "%ihz %ic %s (%ib) %s\n", 
				param->sample_rate, 
				channels,
				format_str, 
				sizeof_sample, 
				this->codec->long_name 
			);
		printf("Timebase: %i / %i\n", this->stream->time_base.den, this->stream->time_base.num);
	}
	
	/**************
	 * OPEN AUDIO *
	 **************/

	printf("Open audio\n");
	player_reconfig( param );
	printf("Reconfig ok\n");


	return 0;
error:
	printf("Error happened!\n");
	print_averr(ret);
	return -1;
}

void decoder_start()
{
	struct DecoderState *this  = &player->av;
	if( !this->thread_run )
		thrd_create( &this->thread, &decoder_loop, NULL );
}

void decoder_stop()
{
	struct DecoderState *this  = &player->av;
	if( this->thread_run ){
		printf("Stopping decoder thread\n");
		this->thread_run = 0;
		thrd_join(this->thread, NULL);
	}
}

int decoder_loop(void*arg)
{
	printf("Begin decode thread!\n");
	struct DecoderState *this  = &player->av;
	struct ScuepTrack   *track = this->track;
	int ret = 0;

	this->thread_run = 1;

	AVCodecParameters *param = this->stream->codecpar; 
	int sample_rate = param->sample_rate;
	AVRational sample_scale = { .den = sample_rate, .num = 1 };
	int __start  = track->start   * (sample_rate*0.001) ;
	int __length = track->length  * (sample_rate*0.001) ;

	while(this->thread_run){
		ret = av_read_frame(this->format, this->packet);
		if(ret < 0) goto error;

		if( this->packet->stream_index != this->stream->index ) continue;
		
		ret  = avcodec_send_packet( this->codec_ctx, this->packet );
		if( ret < 0 ){
			printf("Send broke! %i\n", ret);
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
			
			if(sample_pos > __start+__length) goto finish;
			if(!this->frame->nb_samples) continue;

			int cut_front = MAX( __start - sample_pos, 0);
			int cut_back  = MAX( (sample_pos+samples)-(__start+__length), 0);
			int total     = samples-cut_front-cut_back;

			if(cut_front || cut_back){
				printf("%li, Frame %i+%i front %i, back %i\n", 
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
	printf("Decoder quit!\n");
	return 0;
error:
	printf("Error happened!\n");
	print_averr(ret);
	return -1;
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

	uint32_t head = this->head;

	printf("DECODER_WRITE %i\n", head);

	// Loop until the whole packet has been written out
	while( left != 0 && this->av.thread_run ){

		int available = this->frames - 
			           (this->frames_decoded - 
		                this->frames_played);
		
		available = MIN( available, this->frames - head );
		available = MIN( available, left );


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
			// Nobody should use MP3 anyway
			int i = head * this->sizeof_frame;
			for( int f = 0; f < available; f++ )           // Frame
			for( int c = 0; c < packet->channels; c++ )    // Channel
			for( int b = 0; b < this->sizeof_sample; b++ ) // Byte
			{
				this->data[i++] = packet->data[c][ 
					(f+packet_written) * this->sizeof_sample + b
				];
			}
		} 

		head += available;
		head %= this->frames;
		this->head = head;
		this->frames_decoded += available;

		packet_written += available;
		left -= available;
	}
	return 0;
}


