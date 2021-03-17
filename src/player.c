#include "database.h"
#include "player.h"
#include "util.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>


struct TrackAV{
	AVFormatContext *format;
	AVStream        *stream;
	
	AVCodec         *codec;
	AVCodecContext  *codec_ctx;

	AVPacket *packet;
	AVFrame  *frame;

	int32_t sample_rate;
	// As samples
	int64_t start;
	int64_t length;

	struct ScuepTrack *track;

};


static struct TrackAV av;


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


int player_load(TrackId track_id)
{
	struct ScuepTrack *track = track_load(track_id);

	int ret = 0;
	//struct AVTrack *avt = calloc(sizeof(struct AVTrack), 1);

	AVFormatContext *format = NULL;
	AVStream        *stream = NULL;

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
	stream = format->streams[stream_index];

	// Stream is now known, fill helper vars
	int sample_rate = stream->codecpar->sample_rate;
	AVRational sample_scale = { .den = sample_rate, .num = 1 };
	int start_s  = track->start * (sample_rate*0.001) ;
	int length_s = track->length  * (sample_rate*0.001) ;
	printf("Frames: %i + %i\n", start_s, length_s);

	// Seek and setup decoder
	ret = av_seek_frame( format, stream_index, start_s, AVSEEK_FLAG_BACKWARD );

	AVCodec *codec = avcodec_find_decoder( stream->codecpar->codec_id );
	AVCodecContext *codec_ctx = avcodec_alloc_context3(codec);
	ret = avcodec_parameters_to_context( codec_ctx, stream->codecpar );
	if(ret<0) goto error;
	
	{ // Print info about codecs and stuff
		AVCodecParameters *param = stream->codecpar;
		const char *format_str = av_get_sample_fmt_name(param->format);
		printf( "%ihz %ic %s %s\n", 
				param->sample_rate, 
				param->channels, 
				format_str, 
				codec->long_name 
			);
		printf("Timebase: %i / %i\n", stream->time_base.den, stream->time_base.num);
	}

	ret = avcodec_open2(codec_ctx, codec, NULL);
	if(ret<0) goto error;

	AVPacket *packet = av_packet_alloc();
	AVFrame  *frame  = av_frame_alloc();

	printf("Begin decode\n");
	FILE *fp = fopen("decode.bin", "wb+");
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

			if(cut_front || cut_back){
				printf("%li, Frame %i+%i front %i, back %i\n", 
						sample_pos,
						codec_ctx->frame_number, 
						frame->nb_samples, 
						cut_front, cut_back
				);
			}

			sample_count += frame->nb_samples;
			int total = samples-cut_front-cut_back;
			fwrite( frame->data[1]+(cut_front*2), sizeof(uint16_t), total*2, fp );
			total+= sample_count;
			
		}



	}
	finish:

	printf("Stop decode, wrote %i\n", sample_count);
	fclose(fp);
	return 0;

	error:
	printf("Error happened!\n");
	print_averr(ret);
	return -1;
}



