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
int decoder_load(TrackId track_id);
static int player_write( AVFrame*, int, int );
void decoder_loop(void);


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
	decoder_loop();
	return -1;
}

int player_load(TrackId track_id){
	decoder_load(track_id);
	return 0;
}

int player_stop(void)
{
	struct PlayerState *this = player;
	player = NULL; 
	
	// Stop threads
	
	if( this->sndsvr_close ) {
		this->sndsvr_close();
	}
	free( this->data );
	free( this );

	return 0;
}


int player_reconfig( AVCodecParameters *param )
{
	struct PlayerState *this = player;

	if( this->data ) {
		free(this->data);
		this->data = NULL;
	}

	this->period = 1024;
	this->channels = param->channels;
	this->format  = param->format;
	this->frames  = this->period * 10;

	this->sizeof_sample = av_get_bytes_per_sample(param->format);
	this->sizeof_frame  = this->sizeof_sample * this->channels;

	this->size = this->frames * this->sizeof_frame;
	
	printf("Buffer frames: %i\n", this->frames);

	this->data    = malloc(this->size);
	
	return 0;
}

int decoder_load(TrackId track_id)
{
	struct DecoderState *this = &player->av;
	int ret = 0;
	
	this->format    = NULL;
	this->codec     = NULL;
	this->codec_ctx = NULL;
	this->packet    = NULL;
	this->frame     = NULL;

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
	int __start  = track->start   * (sample_rate*0.001) ;
	int __length = track->length  * (sample_rate*0.001) ;
	printf("Frames: %i + %i\n", __start, __length);

	
	ret = av_seek_frame( this->format, stream_index, __start, AVSEEK_FLAG_BACKWARD );

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
	alsa_open( param, player );


	return 0;
error:
	printf("Error happened!\n");
	print_averr(ret);
	return -1;
}

void decoder_loop(void)
{
	struct DecoderState *this  = &player->av;
	struct ScuepTrack   *track = this->track;
	int ret = 0;

	AVCodecParameters *param = this->stream->codecpar; 
	int sample_rate = param->sample_rate;
	AVRational sample_scale = { .den = sample_rate, .num = 1 };
	int __start  = track->start   * (sample_rate*0.001) ;
	int __length = track->length  * (sample_rate*0.001) ;

	while(1){
		ret = av_read_frame(this->format, this->packet);
		if(ret < 0) goto error;

		if( this->packet->stream_index != this->stream->index ) continue;
		
		ret  = avcodec_send_packet( this->codec_ctx, this->packet );
		if( ret < 0 ){
			printf("Send broke! %i\n", ret);
			print_averr(ret);
			break;
		}

		while( ret >= 0 ){

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
error:
	printf("Error happened!\n");
	print_averr(ret);
finish:	
	return;
}

int _old_play(TrackId track_id)
{
	struct ScuepTrack *track = track_load(track_id);

	int ret = 0;

	// Things you need to clean up
	AVFormatContext *format    = NULL;
	AVCodec         *codec     = NULL;
	AVCodecContext  *codec_ctx = NULL;
	AVPacket        *packet    = NULL;
	AVFrame         *frame     = NULL;

	// Open up file, select stream

	if (avformat_open_input(&format, track->path, NULL, NULL) != 0) {
		printf("Could not open file '%s'\n", track->path);
		return -1;
	}

	if (avformat_find_stream_info(format, NULL) < 0) {
		printf("Could not retrieve stream info from file '%s'\n", track->path);
		return -1;
    }

	printf( "Track has %i stream(s)\n", format->nb_streams  );
    int stream_index = -1;
    for (int i=0; i< format->nb_streams; i++) {
		enum AVMediaType type = format->streams[i]->codecpar->codec_type;
		const char *stream_type = av_get_media_type_string( type ); // Free it????
		printf( "Stream %i type: %s\n", i, stream_type);
        if (type == AVMEDIA_TYPE_AUDIO) {
            stream_index = i;
        }
    }
	if(stream_index== -1){
		printf( "No suitable streams!\n");
		return -1;
	}
	AVStream        *stream    = NULL;
	stream = format->streams[stream_index];

	// Stream is now known, fill helper vars
	
	int sample_rate = stream->codecpar->sample_rate;
	AVRational sample_scale = { .den = sample_rate, .num = 1 };
	int start_s  = track->start * (sample_rate*0.001) ;
	int length_s = track->length  * (sample_rate*0.001) ;
	printf("Frames: %i + %i\n", start_s, length_s);

	// Seek and setup decoder
	
	ret = av_seek_frame( format, stream_index, start_s, AVSEEK_FLAG_BACKWARD );

	codec = avcodec_find_decoder( stream->codecpar->codec_id );
	codec_ctx = avcodec_alloc_context3(codec);

	ret = avcodec_parameters_to_context( codec_ctx, stream->codecpar );
	if(ret<0) goto error;
	
	int sizeof_sample = av_get_bytes_per_sample(stream->codecpar->format);
	int interleaved   = av_sample_fmt_is_planar(stream->codecpar->format);
	int channels    = stream->codecpar->channels;
	if(channels != 2) {
		printf("Channel count of %i is not yet supported", channels);
		goto error;
	}
	


	{ // Print info about codecs and stuff
		AVCodecParameters *param = stream->codecpar;
		const char *format_str = av_get_sample_fmt_name(param->format);
		printf( "%ihz %ic %s (%ib) %s\n", 
				param->sample_rate, 
				channels,
				format_str, 
				sizeof_sample, 
				codec->long_name 
			);
		printf("Timebase: %i / %i\n", stream->time_base.den, stream->time_base.num);
	}

	/**************
	 * OPEN AUDIO *
	 **************/

	printf("Open audio\n");
	player_reconfig( stream->codecpar );
	printf("Reconfig ok\n");
	alsa_open( stream->codecpar, player );

	/****************
	 * BEGIN DEOCDE *
	 ****************/

	ret = avcodec_open2(codec_ctx, codec, NULL);
	if(ret<0) goto error;

	packet = av_packet_alloc();
	frame  = av_frame_alloc();

	printf("Begin decode\n");
	//FILE *fp = fopen("decode.bin", "wb+");
	int sample_count = 0;

	while(1){
		ret = av_read_frame(format, packet);
		if(ret < 0) goto error;

		if( packet->stream_index != stream_index ) continue;
		
		int ret = 0;
		ret  = avcodec_send_packet( codec_ctx, packet );
		if( ret < 0 ){
			printf("Send broke! %i\n", ret);
			print_averr(ret);
			break;
		}

		while( ret >= 0 ){

			ret = avcodec_receive_frame( codec_ctx, frame);
			

			int64_t sample_pos = av_rescale_q(
					frame->pts, 
					stream->time_base, 
					sample_scale 
				);

			int64_t samples    = frame->nb_samples;
			
			if(sample_pos > start_s+length_s) goto finish;
			if(!frame->nb_samples) continue;

			int cut_front = MAX( start_s - sample_pos, 0);
			int cut_back  = MAX( (sample_pos+samples)-(start_s+length_s), 0);
			int total     = samples-cut_front-cut_back;

			if(cut_front || cut_back){
				printf("%li, Frame %i+%i front %i, back %i\n", 
						sample_pos,
						codec_ctx->frame_number, 
						frame->nb_samples, 
						cut_front, total
				);
			}

			sample_count += frame->nb_samples;
			
			player_write(frame, cut_front, total);

			total+= sample_count;
			
		}
	}
	finish:

	printf("Stop decode, wrote %i\n", sample_count);
	if(format)    avformat_close_input(&format);
	// AVCodec is freed by format?
	if(codec_ctx) avcodec_free_context(&codec_ctx);
	if(packet)    av_packet_free(&packet);
	if(frame)     av_frame_free(&frame);

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


