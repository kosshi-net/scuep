#include "config.h"
#include <stdio.h>

#include <taglib/tag_c.h>	
#include <libcue/libcue.h>
#include <sqlite3.h>

#include <wchar.h> 
#include <locale.h>
#include <fcntl.h>

#include <string.h> 
#include <stdint.h> 
#include <unistd.h>
#include <stdbool.h>
#include <poll.h>
#include <uchar.h>
#include <locale.h>

#include <sys/stat.h>
#include <sys/time.h>

#include <wordexp.h>

#include "filehelper.h"
#include "util.h"
#include "log.h"

#include "database.h"
#include "player.h"
#include "frontend.h"

#define SCUEP_TITLE "scuep"
#define SCUEP_VERSION_MAJOR 1
#define SCUEP_VERSION_MINOR 0

static char *path_config_folder;
static char *path_database;
static void  load_playlist(char *playlist);

static bool ro = 0;

void scuep_exit(const char *error);


void scuep_exit( const char *error)
{
	// Do clean up
	if(error){
		fprintf(stderr, "%s\n", error);
		exit(1);
	}
	exit(0);
}

enum Flag {
	flag_default,
	flag_help,
	flag_version,
	flag_ro,
	flag_reset,
	flag_debug,
	flag_stdin
};
enum Flag parse_flag( char* str )
{
	if(strcmp(str, "--help"
	)==0) return flag_help;

	if(strcmp(str, "--version"
	)==0) return flag_version;

	if(strcmp(str, "--readonly"
	)==0) return flag_ro;

	if(strcmp(str, "--reset"
	)==0) return flag_reset;

	if(strcmp(str, "--debug"
	)==0) return flag_debug;

	if(strcmp(str, "-"
	)==0) return flag_stdin;

	if(strcmp(str, "-i"
	)==0) return flag_stdin;

	return flag_default;
}

#define HELP_MESSAGE \
"SCUEP - Simple CUE Player\n\n" \
"The player is in an early development state.\n" \
"Please visit https://github.com/kosshishub/scuep for updates and additional\n" \
"help and usage examples.\n" \
"--help\n" \
"    Display help\n" \
"--version\n" \
"    Print version\n" \
"--debug\n" \
"    Enable logging with Syslog\n" \
"--ro\n" \
"    Don't overwrite playlist saved in .config\n" \
"-i, -\n" \
"    Read playlist from stdin\n" 


int build_config_paths(){
	char *cfgroot = getenv("XDG_CONFIG_HOME");

	if( !cfgroot ){
		char *home = getenv("HOME");
		asprintf( &cfgroot, "%s/.config", home );
	}

	asprintf(&path_config_folder, "%s/scuep-dev", cfgroot );
	asprintf(&path_database, "%s/scuep.db", path_config_folder );

	if( access( cfgroot, W_OK ) ){
		fprintf(stderr, "%s is not a valid config path\n", cfgroot);
		return -1;
	}
	
	if( access( path_config_folder, W_OK ) ){
		mkdir( path_config_folder, 0700 );
	}

	printf("%s\n", path_config_folder);
	printf("%s\n", path_database);

	return 0;
}


int main(int argc, char **argv)
{
	setlocale(LC_ALL,"");

	if( build_config_paths() ) return -1;

	char *input_file = NULL;
	
	for(int i = 1; i < argc; i++){
		char *arg = argv[i];
		enum Flag flag = parse_flag(arg);
		
		switch(flag){
			case flag_help:
				printf("%s", HELP_MESSAGE);
				scuep_exit(NULL);
				break;
			case flag_version:
				printf("%s-%i.%i\n",
					SCUEP_TITLE,
					SCUEP_VERSION_MAJOR,
					SCUEP_VERSION_MINOR
				);
				scuep_exit(NULL);
				break;
			case flag_ro:
				ro = 1;
				break;
			case flag_stdin:
				input_file = read_stdin();
				break;
			case flag_debug:
				scuep_log_start();
				break;
			case flag_reset:
				unlink(path_database);
				printf("Database reset\n");
				break;
			default:
				// Assume its a file
				input_file = read_file(arg);
				if(!input_file){
					scuep_exit("Invalid option or file\nscuep --help");
				}

				break;
		}
	}

	if( !input_file ){
		scuep_exit("No input file. Required for this development version");
	}
	
	if (db_initialize( path_database )) {
		fprintf(stderr, "Database error\n");
		return 1;
	}else{
		printf("Database OK\n");
	}

	playlist_clear();
	load_playlist( input_file );
	transaction_end();

	
	player_load( playlist_track(1) );

	//frontend_initialize();
	//frontend_terminate();

	db_terminate();
}

#define CD_FRAMERATE 75

#define MAX_STR_LEN 512
#define MAX_PATH_LEN 512
#define LOAD_PLAYLIST_INCLUDE 1
#ifdef LOAD_PLAYLIST_INCLUDE

// TODO: Use strdup instead of const disgarding, move it to import.c and split 
// it up more, make it return on error instead of calling exit()
void load_playlist(char *playlist)
{
	//int track_count = 1;
	//do{ if(*c=='\n') track_count++; } while(*++c);
	
	transaction_begin();

	char clip[4096];
	char path[MAX_PATH_LEN];

	char 		*head = playlist;
	const char  *tail = playlist;

	for (int i = 0; ; ++i) {
		
		/* Save progress for large scans
		if( i % 1000 == 0 && i){
			transaction_end();
			transaction_begin();
		}*/

		Cd          *cue_cd  = NULL;
		TagLib_File *tl_file = NULL;

		TrackId track_id = -1;

		while( *head && *head != '\n' ) head++;
		if(*head=='\0') goto finish; // END OF LINE
		*head = '\0';
		
		strcpy(clip, tail);
		const char *uri = tail;

		// Check if URL exists in cache database
		// if true, add id and continue
		track_id = track_by_uri(uri);
		
		printf("track_id %i\n", track_id);

		if( track_id > -1){
			printf("track_id found %i, skip \n", track_id);
			playlist_push(track_id);
			goto skip;
		}


		struct ScuepTrack track;
		memset(&track, 0, sizeof(struct ScuepTrack));
		track.uri  = uri;

		if (strncmp(&uri[0], "cue://", 6)  == 0) {

			// Parse chapter number
			char *chap = clip + strlen(clip);
			while(*--chap != '/');
			*chap++ = '\0';
			track.chapter = atoi(chap);
			if(track.chapter == 0) {
				printf("Error: Bad URL (item %i)\n", i);
				scuep_exit(uri);
			}

			// Copy path
			int prot_len = strlen("cue://");
			strncpy( path, clip+prot_len , MAX_PATH_LEN );


			char *string = scuep_read_file( path );
			if(string == NULL) {
				printf("Error: Missing file (item %i)\n", i);
				scuep_exit( path );
			}
			cue_cd = cue_parse_string( string + scuep_bom_length(string) );
			free(string);

			Track 	*cue_track  = cd_get_track( cue_cd, track.chapter );
			Cdtext 	*cdtext     = cd_get_cdtext(cue_cd);
			Cdtext 	*tracktext  = track_get_cdtext(cue_track);

			track.album  = cdtext_get( PTI_TITLE, cdtext );
			track.artist = cdtext_get( PTI_PERFORMER, tracktext );
			track.title  = cdtext_get( PTI_TITLE, tracktext );
			//printf("FRAME %li\n", track_get_start(cue_track) );
			track.start  = track_get_start  (cue_track) / (CD_FRAMERATE*0.001);
			track.length = track_get_length (cue_track) / (CD_FRAMERATE*0.001);
						
			track.basename   = track_get_filename(cue_track);
			track.dirname    = scuep_dirname(path);
			printf("BASENAME %s\n", track.basename);

			if(track.length == 0) {
				// Length of the last chapter cant be parsed from the cue sheet 
				// Use taglib to get the length of the whole file
				const char *cue_filename = track_get_filename(cue_track);
				char cue_path[1024*4];
				strcpy( cue_path, track.path );
				strcpy( scuep_basename(cue_path), cue_filename );

				tl_file = taglib_file_new( cue_path );
				const TagLib_AudioProperties *tl_prop = taglib_file_audioproperties( tl_file ); 
				track.length = 
					  taglib_audioproperties_length( tl_prop ) 
					* 1000 
					- track.start;
			}
		}
		else{ // Misc file, use taglib
			
			strncpy( path, uri , MAX_PATH_LEN );
			printf("%s\n", uri);


			if (access(uri, R_OK)!=0) {
				printf("Error: Missing file (item %i)\n", i);
				scuep_exit( path );
			}

			track.basename = scuep_strdup( scuep_basename(path) );
			track.dirname  = scuep_dirname( path );

			tl_file     = taglib_file_new( uri );


			TagLib_Tag  *tl_tag = taglib_file_tag( tl_file );
			track.title  = taglib_tag_title (tl_tag);
			track.artist = taglib_tag_artist(tl_tag);
			track.album  = taglib_tag_album (tl_tag);

			const TagLib_AudioProperties *tl_prop = taglib_file_audioproperties( tl_file );

			track.start = 0;
			track.length = taglib_audioproperties_length( tl_prop ) * 1000;
			track.chapter 	 = -1;
		}

		if(!track.album)  track.album = "";
		if(!track.title)  track.title = "";
		if(!track.artist) track.artist = "";

		// if metadata failed to load, use filename instead
		if( !track.title[0] )  track.title = scuep_basename(uri);
		/*
		printf( "%s // %s // %s // %i - %is, #%i\n", \
			track.title, 
			track.artist, 
			track.album, 
			track.start,
			track.length,
			track.chapter
		);
		printf( "uri: %s\n", track.uri);
		*/
		// TODO errorcheck!
		track_store(&track);
		track_id = track_by_uri(uri);
		playlist_push( track_id );
		
		skip:
		if(cue_cd) {
			cd_delete(cue_cd);
		}
		if(tl_file) {
			taglib_tag_free_strings();
			taglib_file_free( tl_file );
		}
		tail = ++head;
	}
finish:
	printf("Transaction end\n");
	transaction_end();
	return;
}


#endif




