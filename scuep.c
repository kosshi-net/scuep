#define _XOPEN_SOURCE 
#include <ncurses.h>
#include <taglib/tag_c.h>	
#include <libcue/libcue.h>
#include <mpv/client.h>
#include <pthread.h>

#include <wchar.h> 

#include <locale.h>
#include <fcntl.h>

#include <string.h> 
#include <unistd.h>
#include <stdbool.h>
#include <poll.h>

#include <sys/stat.h>
#include <sys/time.h>

#include <wordexp.h>

#include "filehelper.h"
#include "util.h"
#include "log.h"


#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

#define SCUEP_TITLE "scuep"
#define SCUEP_VERSION_MAJOR 0
#define SCUEP_VERSION_MINOR 24

struct LibraryItem
{
	wchar_t *album;
	wchar_t *performer;
	wchar_t *track;
	char *path;
	
	uint64_t start;
	uint64_t length;

	int8_t  chapter;

	bool marked;
	bool error;
	bool disabled;
	bool warning;
};

enum { 
	MPV_INIT,
	MPV_PLAY
} mpv_state = MPV_INIT;

enum {
	MODE_DEFAULT,
	MODE_COMMAND,
	MODE_SEARCH,
} input_mode = MODE_DEFAULT;

static time_t time_ms(void);
static void save_playlist(char*playlist);
static void save_state(void);
static int sprinturl( char *dst, struct LibraryItem *item );
static int fprinturl( FILE *, struct LibraryItem *item );

static void build_database(char*);

static void cleanup(void);
static void quit(void);
static void mpverr( int status );


static int scuep_match( struct LibraryItem *item, wchar_t *match );
static void scuep_search(void);

static void shell_item( struct LibraryItem *item );
static void run_command( const char *cmd );

static void playpause();
static void play();
static void seekload( struct LibraryItem *item );
static void next(int num);
static void prev(int num);
static void seek(int seconds);
static void set_volume(double volume);

static int current_track_progress(void);
static int current_track_length(void);

static enum Flag parse_flag( char* str );


static void queue_redraw(int);
static void draw_progress(void);
static void draw_library(void);
static void draw_texthl( uint32_t, uint32_t, wchar_t*, uint32_t, uint32_t );

static void draw(void);

static int input(void);

/*
 * To avoid unnecessary redraws, use queue_redraw(ELEMENT_*) when relevant 
 * state changes happen.
 * */

#define ELEMENT_ALL      0xFFFF // Redraw all elements
#define ELEMENT_CLEAR    (1<<0) // Clear all elemenets. Use ELEMENT_ALL to set

#define ELEMENT_PROGRESS (1<<1)
#define ELEMENT_LIBRARY  (1<<2) // Currently includes everything but progress
#define ELEMENT_PROMPT   (1<<2) // Does nothing for now

uint32_t elements_dirty = ELEMENT_ALL; // Use via queue_redraw(), not directly


#define MAX_CMD_LEN 256
#define PADX 4

// $HOME/.config/scuep/file
char track_id_path[1024];
char playlist_path[1024];
char volume_path  [1024];

bool nosave = 0;

mpv_handle *ctx; 

uint32_t row, col; 

struct LibraryItem 	*library = NULL;
uint32_t			 library_items;

uint32_t			 playing_id  = -1;
uint32_t			 selected_id = -1;
bool 			   	 selection_follows = 1;

double output_volume = 100.0f;

char     command      [MAX_CMD_LEN];
wchar_t  command_wchar[MAX_CMD_LEN];
uint32_t command_cursor = 0;

bool     input_delete = 0;
int32_t  repeat = 0;

wchar_t *library_wide;
size_t   library_wide_size;

char    *library_char;
size_t   library_char_size;

char cmd_buf[1024*8];


time_t time_ms(){
    struct timeval t;
    gettimeofday(&t, NULL);
    return t.tv_sec*1000+t.tv_usec/1000;
}

void cleanup(){
	if(library) {
		free(library);
		library = NULL;
	}
	scuep_log_stop();
}

int current_track_progress(){
	int seconds = 0; 

	char *receive = mpv_get_property_string(ctx, "playback-time");
	if(receive)
		seconds = atoi(receive) - library[playing_id].start;

	return seconds;
};

int current_track_length(){
	return library[playing_id].length;
};

void queue_redraw(int elem){
	elements_dirty |= elem;
}



int debug_wide_blocks = 0;
int debug_char_blocks = 0;
size_t debug_bytes = 0;
#define MAX_STR_LEN 512
#define BLOCK_SIZE 1024*4 
void build_database(char *playlist){

	// wchar_t is used for anything that will be rendered
	// char for things like filepaths

	library_wide_size = BLOCK_SIZE;
	library_char_size = BLOCK_SIZE;
	library_wide = malloc(library_wide_size * sizeof(wchar_t) );
	debug_bytes += BLOCK_SIZE*sizeof(wchar_t);
	library_char = malloc(library_char_size);
	debug_bytes += BLOCK_SIZE*sizeof(char);


	int track_count = 1;
	char *c = playlist; 
	do{ if(*c=='\n') track_count++; } while(*++c);
	library = calloc( track_count, sizeof(struct LibraryItem) );

	uint32_t wi=0;
	uint32_t ci=0;

	char line_buf[4096];
	char *_line_buf;

	c = playlist;

	for (int i = 0; ; ++i) {
		// Read in line from playlist
		_line_buf = line_buf;
		while( *c != '\n'){
			if( *c == '\0' ) return; // End of file
			*_line_buf++ = *c++;
		}
		*_line_buf = '\0'; 
		c++;

		char *url = line_buf;

		// TODO: Launch option to disable this, in case this slows down loading
		if( 1 ){
			erase();
			char *str = "Parsing metadata ...";

			mvprintw(0,0, "%s %i %%\n%s\n%i\n", 
				str, 
				(int)(i/(float)track_count * 100.0),
				line_buf,
				strlen(line_buf)
			);
			refresh();
		}

		// Make sure we have enough memory
		// If not, alloc a new block
		// Previous one is "leaked" currently !!
		if( ci + MAX_STR_LEN * 1 >= library_char_size ){
			library_char = malloc( library_char_size * sizeof(char) );
			ci = 0;
			debug_char_blocks++;
			debug_bytes += BLOCK_SIZE*sizeof(char);
		}
		if( wi + MAX_STR_LEN * 3 >= library_wide_size ){
			library_wide = malloc( library_wide_size * sizeof(wchar_t) );
			wi = 0;
			debug_wide_blocks++;	
			debug_bytes += BLOCK_SIZE*sizeof(wchar_t);
		}
		
		Cd          *cue_cd  = NULL;
		TagLib_File *tl_file = NULL;

		const char *utf8_title  = NULL;
		const char *utf8_artist = NULL;
		const char *utf8_album  = NULL;

		if( strncmp(&url[0], "cue://", 6)  == 0  ) {
			// CUE sheet, use libcue
			// Parse chapter
			char *chap = url + strlen(url);
			while(*--chap != '/');
			*chap++ = '\0';
			int chapter = atoi(chap);
			if(chapter == 0) {
				endwin();
				printf("Error: Bad URL (item %i)\n", i);
				printf("%s\n", url);
				cleanup();
				exit(1);
			}

			// Copy path
			library[i].path = library_char+ci;
			int prot_len = strlen("cue://");
			for (
				int j = prot_len;
				url[j] != '\0' && j-prot_len < MAX_STR_LEN-1;
				library_char[ci++] = url[j++]
			);
			library_char[ci++] = '\0';

			library[i].chapter 	 = chapter;

			char  *string = scuep_read_file(library[i].path);
			if(string == NULL) {
				endwin();
				printf("Error: Missing file (item %i)\n", i);
				printf("Path: %s\n", library[i].path);

				cleanup();
				exit(1);
			}
			cue_cd = cue_parse_string( string + scuep_bom_length(string) );
			free(string);

			Track 	*track      = cd_get_track( cue_cd, chapter );
			Cdtext 	*cdtext     = cd_get_cdtext(cue_cd);
			Cdtext 	*tracktext  = track_get_cdtext(track);

			utf8_album  = cdtext_get( PTI_TITLE, cdtext );
			utf8_artist = cdtext_get( PTI_PERFORMER, tracktext );
			utf8_title  = cdtext_get( PTI_TITLE, tracktext );
			library[i].start  = track_get_start  (track) / 75;
			library[i].length = track_get_length (track) / 75;

			if(library[i].length == 0) {
				// Lenght of the last chapter cant be parsed from the cue sheet 
				// Use taglib to get the length of the whole file
				const char *cue_filename = track_get_filename(track);
				char cue_path[1024*4];
				strcpy( cue_path, library[i].path );
				strcpy( scuep_basename(cue_path), cue_filename );

				tl_file = taglib_file_new( cue_path );
				const TagLib_AudioProperties *tl_prop = taglib_file_audioproperties( tl_file ); 
				library[i].length = taglib_audioproperties_length( tl_prop ) - library[i].start;
			}
		}else{ // Misc file, use taglib

			// Copy path
			library[i].path = library_char+ci;
			for (
				int j = 0;
				url[j] != '\0' && j< MAX_STR_LEN-1;
				library_char[ci++] = url[j++]
			);
			library_char[ci++] = '\0';

			tl_file     = taglib_file_new( url );
			if(tl_file == NULL) {
				endwin();
				printf("Error: Missing file (item %i)\n", i);
				printf("Path: %s\n", library[i].path);

				cleanup();
				exit(1);
			}
			TagLib_Tag  *tl_tag = taglib_file_tag( tl_file );
			utf8_title  = taglib_tag_title (tl_tag);
			utf8_artist = taglib_tag_artist(tl_tag);
			utf8_album  = taglib_tag_album (tl_tag);

			const TagLib_AudioProperties *tl_prop = taglib_file_audioproperties( tl_file );
			library[i].start = 0;
			library[i].length = taglib_audioproperties_length( tl_prop );

			library[i].chapter 	 = -1;
		}

		if(!utf8_album)  utf8_album = "";
		if(!utf8_title)  utf8_title = "";
		if(!utf8_artist) utf8_artist = "";

		if( !utf8_title[0] )  utf8_title = scuep_basename(url);

		library[i].album = library_wide+wi;
		wi += mbstowcs(library_wide+wi, utf8_album, MAX_STR_LEN-1 );
		library_wide[wi++] = '\0';

		library[i].performer = library_wide+wi;
		wi += mbstowcs(library_wide+wi, utf8_artist, MAX_STR_LEN-1 );
		library_wide[wi++] = '\0';

		library[i].track = library_wide+wi;
		wi += mbstowcs(library_wide+wi, utf8_title, MAX_STR_LEN-1 );
		library_wide[wi++] = '\0';

		if(cue_cd) {
			cd_delete(cue_cd);
		}
		if(tl_file) {
			taglib_tag_free_strings();
			taglib_file_free( tl_file );
		}

		library_items++;
	}	
}


#define TEXT_ALIGN_RIGHT (1<<0)
void draw_texthl(
	uint32_t x, uint32_t y, 
	wchar_t*text, 
	uint32_t width, 
	uint32_t options 
){
	static wchar_t slicebuf[512]; 
	uint32_t w = scuep_wcslice(slicebuf, text, width-2);
	if( options & TEXT_ALIGN_RIGHT ) x = x + (width-2 - w);
	mvprintw( y, x, "%S", slicebuf );
	if( w >= width-2 ) mvprintw( y, x+width-4, "%s", ".." );
	wchar_t *substring = NULL;

	if( command[0] == '/' && command_cursor > 1 )
		substring = scuep_wcscasestr( text, command_wchar+1 );
	
	if(substring) {
		attron(COLOR_PAIR(2));
		ptrdiff_t index = substring - text;
		
		int hx = x;
		for( int i = 0; i < index; i++ )
			hx += wcwidth( slicebuf[i] );

		mvprintw( y, hx, "%.*S", command_cursor-1, slicebuf + (index));
		attroff(COLOR_PAIR(2));
	}
}


void draw_progress(){

	char *receive = mpv_get_property_string(ctx, "playback-time");

	int length_seconds = current_track_length();
	int mpv_seconds =    current_track_progress();

	char *volstr = "Volume:  ..%";
	int vollen = strlen(volstr);

	move(row-2, 0);
	clrtoeol();

	mvprintw( row-2, col-PADX-vollen, "%s", volstr );

	mvprintw( row-2, col-PADX-5, "%4i%", (int)output_volume );

	int bar_length = col - 15 - 15 - PADX*2;

	for (int i = 0; i < bar_length; ++i){
		mvprintw( row-2, 15+PADX+i, "-");
	}

	mvprintw( row-2, 15+PADX+( mpv_seconds/(float)length_seconds * bar_length ), "|");

	mvprintw( row-2, PADX, "%i:%02i / %i:%02i", 
		mpv_seconds/60,
		mpv_seconds%60,
		length_seconds/60,
		length_seconds%60
	);
	if(receive) mpv_free(receive);
}

uint8_t blink = 0;
void draw_library(){


	uint32_t center=MIN( (row/2), selected_id+3);

	int last_redrawn = 0;

	for (int32_t i = selected_id-center; i < (int)library_items; ++i)
	{

		int y = i - selected_id + center;

		if( y < 3 ) continue;
		if( y + 4 > (uint32_t)row ) break;

		last_redrawn = y + 1;
		move(y, 0);
		clrtoeol();

		if(library[i].marked)
			mvprintw( y, 2, "%s", "*");

		if(playing_id==i)
			mvprintw( y, 1, "%s", ">");

		if(library[i].warning)
			attron(COLOR_PAIR(3));

		if(selected_id==i){
			if( selected_id!=playing_id )
				mvprintw( y, 1, "%s", "~");
			attron(COLOR_PAIR(1));
		}

		if(library[i].disabled){
			attron(COLOR_PAIR(4));
			mvprintw( y, 3, "%s", "#");
		}
	
		// "Cursors", how much space have been used from each edge?
		int curleft  = PADX;
		int curright = col-2;

		int x, w; 
		int align= 1;

		x = MAX(curright - 32, curleft + 32*2);
		w = curright - x;
		
		if(w > 7){
			draw_texthl( x, y,  library[i].album, w, align );
			align = 0;
			curright -= w;
		}
		
		x = MAX(curright - 32, curleft + 32);
		w = curright - x;

		if(w > 7){
			draw_texthl( x,	y,  library[i].performer, 	w,   align );
			curright -= w;
		}

		align = 0; // Draw the track left-aligned anyway

		x = curleft;
		w = curright - x;
		draw_texthl( x, y,  library[i].track,     	w,   align );

		attroff(COLOR_PAIR(1));
	}
	
	while( last_redrawn+3 <= row ){
		move(last_redrawn++, 0);
		clrtoeol();
	}

	move(1, 0);
	clrtoeol();

	mvprintw( 1, PADX, "Playlist %i/%i ", selected_id, library_items );
	blink = !blink;
	mvprintw( 1, col-strlen(SCUEP_TITLE)-PADX - 5, "scuep %i.%i", 
		SCUEP_VERSION_MAJOR,
		SCUEP_VERSION_MINOR
	);
	if(blink) 		mvprintw( 1l, col-PADX, "*" );

	move(row-1, 0);
	clrtoeol();
	printw("%S", command_wchar );
	refresh();
}

void draw(){

	// Poll term size and detect resize events
	int nrow, ncol;
	getmaxyx(stdscr, nrow, ncol);
	if( nrow != row || ncol != col) queue_redraw(ELEMENT_ALL);
	col = ncol;
	row = nrow;

	if( elements_dirty & ELEMENT_CLEAR ) clear();

	if( elements_dirty & ELEMENT_PROGRESS ) draw_progress();
	if( elements_dirty & ELEMENT_LIBRARY  ) draw_library();
	
	elements_dirty = 0;
}

int scuep_match( struct LibraryItem *item, wchar_t *match ){
	return ( 
		scuep_wcscasestr( item->track,     match) || 
		scuep_wcscasestr( item->performer, match) ||
		scuep_wcscasestr( item->album,     match) 
	);
}

void scuep_search(){
	for (uint32_t i = 0; i < library_items; ++i) {
		uint32_t j = (selected_id + i + 1) % library_items;
		if( 
			scuep_match( library+j, command_wchar+1 )
		){
			selection_follows = 0;
			selected_id = j;
			return;
		}
	}
}

void save_playlist(char*playlist){
	if(nosave) return;
	FILE * fp;
	fp = fopen (playlist_path,"w");
	fwrite(playlist, 1, strlen(playlist), fp);
	fclose(fp);
	return;
}

void save_state(){
	if(nosave) return;
	FILE * fp;

	fp = fopen (track_id_path,"w");
	fprintf (fp, "%i\n", playing_id);
	fclose (fp);

	fp = fopen (volume_path,"wb");
	fwrite(&output_volume, 1, sizeof(double), fp);
	fclose (fp);
	return;
}

void quit(){
	endwin();
	cleanup();
	exit(0);
}

void mpverr( int status  ){
	if(status < 0){
		endwin();
		printf("MPV API error: %s\n", mpv_error_string(status));
		cleanup();
		exit(0);
	}	
}

int sprinturl( char *dst, struct LibraryItem *item ){
	if( item->chapter == -1 )
		return sprintf(dst, "%s", item->path );
	else
		return sprintf(dst, "cue://%s/%i", 
			item->path,
			item->chapter
		);
}

int fprinturl( FILE*fp, struct LibraryItem *item){
	if( item->chapter == -1 )
		return fprintf(fp, "%s\n", item->path);
	else
		return fprintf(fp, "cue://%s/%i\n", 
			item->path,
			item->chapter
		);
}


void  shell_item( struct LibraryItem *item ){
	char *src = command+2;
	char *dst = cmd_buf;
	while(*src != '\0'){
		if( *src == '%' ){
			dst += sprinturl( dst, item );
			src++;
		} else {
			*dst++ = *src++;
		}
	}
	*dst = '\0';

	system(cmd_buf);
}


void playpause(){
	char *mpv_cmd[8]={NULL};
	mpv_cmd[0] = "cycle";
	mpv_cmd[1] = "pause";
	mpverr(mpv_command(ctx, (const char**)mpv_cmd)); 
}

void play(){
	int flag = 0;
	mpverr(mpv_set_property(ctx, "pause", MPV_FORMAT_FLAG, &flag));
}
void seekload( struct LibraryItem *item ){
	if(item->disabled) {
		next(1); // Replace this with something smarter 
		return;
	}

	char *mpv_cmd[8]={NULL}; 
	mpv_cmd[0] = "loadfile"; 
	mpv_cmd[1] = item->path;
	mpv_cmd[2] = "replace";
	mpv_cmd[3] = malloc(16);
	sprintf( mpv_cmd[3], "start=#%i,end=#%i", 
		item->chapter, item->chapter+1
	);
	mpverr(mpv_command(ctx, (const char**)mpv_cmd)); 
	free(mpv_cmd[3]);
	save_state();
	play();

	queue_redraw(ELEMENT_PROGRESS | ELEMENT_LIBRARY);
	return;
}


void seek(int seconds){

	int new_pos = current_track_progress() + seconds; 
	int max = current_track_length(); 

	if( new_pos > max ) return next(1);
	if( new_pos < 0   ) seconds -= new_pos;

	char *mpv_cmd[8]={NULL}; 
	mpv_cmd[0] = "seek"; 
	mpv_cmd[1] = malloc(16);
	sprintf( mpv_cmd[1], "%i", seconds );
	mpverr( mpv_command(ctx, (const char**)mpv_cmd) ); 
	free(mpv_cmd[1]);

	queue_redraw(ELEMENT_PROGRESS);
}

void next(int num){
	playing_id+= num;
	playing_id = (library_items + playing_id) % library_items;
	seekload( library+playing_id );
}

void prev(int num){
	playing_id-= num;
	playing_id = (library_items + playing_id) % library_items;
	seekload( library+playing_id );
}

void set_volume(double volume){
	double dvol = volume;
	mpverr(mpv_set_property(ctx, "volume", MPV_FORMAT_DOUBLE, &dvol));
	queue_redraw(ELEMENT_PROGRESS);
};

void run_command( const char *cmd  ){

	scuep_logf( "Command: %s", cmd );

	if( prefix("next" ,cmd)) {next(1);     goto command_clear;}
	if( prefix("prev" ,cmd)) {prev(1);     goto command_clear;}
	if( prefix("play" ,cmd)) {playpause(); goto command_clear;}
	if( prefix("pause",cmd)) {playpause(); goto command_clear;}

	if(cmd[0] == 'q')
		quit();

	if(cmd[0] == '!'){
		int num_marked = 0;
		for( int i = 0; i < library_items; i++ ){
			if(!library[i].marked) continue;
			num_marked++;
			shell_item(library+i);
		}
		if(!num_marked) shell_item( library+selected_id );
		goto command_clear;
	}

	if( prefix("m/", cmd)){
		for( int i = 0; i < library_items; i++ ){
			if( scuep_match(library+i, command_wchar+3 ) ) {
				library[i].marked = 1;
			}
		}
		goto command_clear;
	}

	// TODO Option to block addto when running from a fifo 
	if( prefix("addto", cmd) ){

		wordexp_t p;
		int err = wordexp( cmd+strlen("addto "), &p, 0 );
		if( err ){
			sprintf( command, "Error parsing path (wordexpr error %i)", err  );
			mbstowcs(command_wchar, command, 128 );
			return;
		}
		if( p.we_wordc != 1 ){
			sprintf( command, "Error parsing path (%li matches, need 1)", p.we_wordc );
			mbstowcs(command_wchar, command, 128 );
			return;
		}

		FILE *fp = fopen( p.we_wordv[0], "a");

		if(!fp){
			sprintf( command, "Error opening file" );
			mbstowcs(command_wchar, command, 128 );
			wordfree(&p);
			return;
		}
		
		int num_written = 0;
		int num_marked = 0;
		for( int i = 0; i < library_items; i++ ){
			if(!library[i].marked) continue;
			num_marked++;
			num_written++;
			fprinturl(fp, library+i );
		}

		
		if(!num_marked){
			fprinturl(fp, library+selected_id );
			num_written++;
		}

		sprintf( command, "Written %i lines to %s", num_written, p.we_wordv[0]);
		mbstowcs(command_wchar, command, 128 );

		wordfree(&p);
		fclose(fp);
		
		goto command_clear;
	}

	if( prefix("mfile", cmd) ){

		wordexp_t p;
		int err = wordexp( cmd+strlen("mfile "), &p, 0 );
		if( err ){
			sprintf( command, "Error parsing path (wordexpr error %i)", err  );
			mbstowcs(command_wchar, command, 128 );
			return;
		}
		if( p.we_wordc != 1 ){
			sprintf( command, "Error parsing path (%li matches, need 1)", p.we_wordc );
			mbstowcs(command_wchar, command, 128 );
			return;
		}
		
		char *mfile = read_file( p.we_wordv[0] );
		wordfree(&p);

		if(!mfile){
			sprintf( command, "Can't open file" );
			mbstowcs(command_wchar, command, 128 );
			return;
		}

		char *s = mfile;
		
		// Replace all newlines with null
		for( s=mfile-1; *++s; *s*=(*s!='\n') );

		char urlbuf[1024*8];
		for( int i = 0; i < library_items; i++ ){
			sprinturl(urlbuf, library+i);
			s = mfile;
			while(1){
				int l = strlen(s);
				if(l==0) break;  
				if(strcmp(urlbuf, s)==0) library[i].marked = 1;
				s+=l+1;
			}
		}

		free(mfile);
		goto command_clear;
	}

	if( prefix("volume", cmd) 
	||  prefix("vol",    cmd)){

		int direction = 0;

		const char *arg = cmd;
		while(*arg && *arg++ != ' ');

		if(*arg == '+'){
			direction++;
			arg++;
		} else if(*arg == '-'){
			direction--;
			arg++;
		}

		uint8_t vol = atoi( arg );
		if(direction == 0)
			output_volume = vol;
		else
			output_volume += direction*vol;

		output_volume = MIN(MAX(output_volume, 0.00), 100.0);

		set_volume(output_volume);
		save_state();
		goto command_clear;
	}

	sprintf( command, "%s", "No such command" );
	mbstowcs(command_wchar, command, 128 );
	return;

	command_clear:
	command_cursor = 0;
	command[command_cursor] = 0;
}

int debug_last_key = 0;

// Returns count events processed
int input(void){
	
	timeout(100);
	int key = getch();
	int events = 0;
	
	int direction = 1;

	while( key !=  ERR ) {
		events++;
		
		debug_last_key = key;
		
		if( input_mode == MODE_DEFAULT ) goto SWITCH_MODE_DEFAULT;
		if( input_mode == MODE_COMMAND ) goto SWITCH_MODE_COMMAND;
		if( input_mode == MODE_SEARCH )  goto SWITCH_MODE_SEARCH;

		SWITCH_MODE_SEARCH:
		input_mode = MODE_DEFAULT;
		goto SWITCH_MODE_DEFAULT;
		goto END;


		SWITCH_MODE_COMMAND:
		switch(key){
			case 27: // Escape
				command_cursor = 0;
				command[command_cursor] = 0;
				input_mode = MODE_DEFAULT;
				break;
			case KEY_BACKSPACE:
			case '\b':
			case 127:
				command_cursor--;
				command[command_cursor] = 0;
				if(command_cursor == 0) 
					input_mode = MODE_DEFAULT;
				break;
			case KEY_LEFT:
			case KEY_RIGHT:
				break;
			case '\n':
				if( command[0] == '/' ){
					scuep_search();
					input_mode = MODE_SEARCH;
				} else {
					run_command(command+1);
					input_mode = MODE_DEFAULT;
				}
				break;
			default:
				command[command_cursor++] = key;
				command[command_cursor] = 0;
				break;
		}
		queue_redraw(ELEMENT_LIBRARY);
		mbstowcs(command_wchar, command, 128 );
		goto END;


		SWITCH_MODE_DEFAULT:
		switch(key){
			case 27: // Escape

				queue_redraw(ELEMENT_LIBRARY);

				if( command[0] == '/' ){				
					command_cursor = 0;
					command[command_cursor] = 0;
					command_wchar[command_cursor] = 0;
					break;
				}

				if( !selection_follows ){
					selection_follows = 1;
					selected_id = playing_id;
					break;
				}
				break;

			case KEY_LEFT:
				seek(-5*MAX( 1, repeat ));
				repeat=0;
				break;
			case KEY_RIGHT:
				seek( 5*MAX( 1, repeat ));
				repeat=0;
				break;

			case '-':
				repeat = - (repeat || 1 );
			case '+':
			case '=':
				if(repeat == 0) repeat++;
				output_volume += 5.0 * repeat;
				output_volume = MIN(MAX(output_volume, 0.00), 100.0);
				set_volume( output_volume );
				repeat = 0;
				break;

			case '1': case '2': case '3': 
			case '4': case '5': case '6': 
			case '7': case '8': case '9':
			case '0': 
				repeat = repeat*10 + (key-'0');
				break;


			case 'k':
			case KEY_UP:
				selected_id -= MAX( 1, repeat );
				goto input_skip_key_down;
			case 'j':
			case KEY_DOWN:
				selected_id += MAX( 1, repeat );
				input_skip_key_down:
				selected_id = (library_items + selected_id) % library_items;
				selection_follows = 0;
				repeat = 0;
				queue_redraw(ELEMENT_LIBRARY);
				break;

			case 'm':
				library[selected_id].marked = !library[selected_id].marked;
				queue_redraw(ELEMENT_LIBRARY);
				break;
			case 'M':
				for( int i = 0; i < library_items; i++  ){
					library[i].marked = !library[i].marked;
					if(input_delete) library[i].marked = 0;
				}
				queue_redraw(ELEMENT_LIBRARY);
				break;
			case 'g':
				selected_id = repeat;
				selected_id = (library_items + selected_id) % library_items;
				selection_follows = 0;
				repeat = 0;
				queue_redraw(ELEMENT_LIBRARY);
				break;


			case '\n':
				playing_id = selected_id;
				selection_follows = 1;
				repeat = 0;
				seekload( library+selected_id  );
				break;

			case 'z':
				prev( MAX( 1, repeat ) );
				break;
			case 'b':
				next( MAX( 1, repeat ) );
				break;
			case 'c':
				playpause();
				break;

			case 'd':
				input_delete = 1;
				break;

			case 'D':;
				uint32_t num_marked = 0;
				for( int i = 0; i < library_items; i++ ){
					if(!library[i].marked) continue;
					num_marked++;
					library[i].disabled = !library[i].disabled;
				}
				if(!num_marked) 
					library[selected_id].disabled = !library[selected_id].disabled;
				queue_redraw(ELEMENT_LIBRARY);
				break;

			case 'l':
				clear();
				queue_redraw(ELEMENT_ALL);
				break;

			case 'N':
				direction = -1;
			case 'n':
				for( uint32_t i = 1; i < library_items; i++ ){
					uint32_t j = ( selected_id + i*direction ) % library_items;

					if( command[0] == '/' ){
						if( !scuep_match(library+j, command_wchar+1) ) continue;
					} else {
						if( !library[j].marked ) continue;
					}
					
					selected_id = j;
					selection_follows = 0;
					queue_redraw(ELEMENT_LIBRARY);
					break;
					
				}
				break;

			case '/':
			case ':':
				command_cursor = 0;
				input_mode = MODE_COMMAND;
				command[command_cursor++] = key;
				command[command_cursor] = 0;
				mbstowcs(command_wchar, command, 128 );
				queue_redraw(ELEMENT_LIBRARY);
				break;
			default:
				break;
		}
		goto END;

		END:
		timeout(0);

		if(key != 'd')
			input_delete = 0;

		key = getch();
	}

	return events;
}



enum Flag {
	flag_default,
	flag_help,
	flag_version,
	flag_nosave,
	flag_debug,
	flag_stdin
};


enum Flag parse_flag( char* str ){
	if(strcmp(str, "--help"
	)==0) return flag_help;
	if(strcmp(str, "--version"
	)==0) return flag_version;
	if(strcmp(str, "--nosave"
	)==0) return flag_nosave;
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
"--nosave\n" \
"    Don't overwrite playlist saved in .config\n" \
"-i, -\n" \
"    Read playlist from stdin\n" 

int main(int argc, char **argv)
{
	// Build path vars
	char *home = getenv("HOME");
	snprintf( track_id_path, 1024, "%s/.config/scuep/track_id", home);
	snprintf( playlist_path, 1024, "%s/.config/scuep/playlist", home);
	snprintf( volume_path,   1024, "%s/.config/scuep/volume"  , home);

	char *playlist = NULL;
	int new_playlist = 0;

	for(int i = 1; i < argc; i++){
		char*arg = argv[i];
		enum Flag flag = parse_flag(arg);
		
		switch(flag){
			case flag_help:
				printf("%s", HELP_MESSAGE);
				quit();
				break;
			case flag_version:
				printf("%s-%i.%i\n",
					SCUEP_TITLE,
					SCUEP_VERSION_MAJOR,
					SCUEP_VERSION_MINOR
				);
				quit();
				break;
			case flag_nosave:
				nosave = 1;
				break;
			case flag_stdin:
				playlist = read_stdin();
				new_playlist = 1;
				break;
			case flag_debug:
				scuep_log_start();
				break;
			default:
				// Assume its a file
				playlist = read_file(arg);
				if(!playlist){
					printf("Invalid option or file\nscuep --help\n");
					quit();
				}

				new_playlist = 1;
				break;
		}
	}

	if(!playlist) playlist = read_file(playlist_path);
	if(!playlist) quit();

	// Init MPV
	// Must be done before setting locale and ncurses
	ctx = mpv_create();
	if(!ctx) exit(1);
	int val = 0;
	mpverr(mpv_set_option_string(ctx, "input-default-bindings", "no"));
	mpverr(mpv_set_option_string(ctx, "input-vo-keyboard", "no"));
	mpverr(mpv_set_option_string(ctx, "vo", "null"));
	mpverr(mpv_set_option(ctx, "osc", MPV_FORMAT_FLAG, &val));
	mpverr(mpv_initialize(ctx));

	// Locale must be set before database build, and ncurses
  	setlocale(LC_ALL,"");

	// Create fifo
	char fifopath[1024];
	snprintf( fifopath, 1024, "%s/.config/scuep/fifo", home);
	mkfifo( fifopath, 0666 );
	int fd;

	// Init ncurses
	// tty thing required if a playlist was piped to the program
	const char* term_type = getenv("TERM");
	FILE* term_in = fopen("/dev/tty", "r");
	SCREEN* main_screen = newterm(term_type, stdout, term_in);
	set_term(main_screen);
	cbreak();
	noecho();
	curs_set(FALSE);
	keypad(stdscr, TRUE);
	notimeout(stdscr, FALSE);
	ESCDELAY = 25;
	use_default_colors();
	start_color();
	init_pair(1, 13, -1);
	init_pair(2, COLOR_BLACK, COLOR_RED);
	init_pair(3, COLOR_YELLOW, -1);
	init_pair(4, COLOR_RED, -1);

	// Scan files and build DB
	build_database(playlist);
	scuep_logf("Database memory usage: %i KB", debug_bytes/1024);

	if( new_playlist ) save_playlist(playlist);

	free(playlist);
	
	playing_id = 0;
	if(!new_playlist) {
		char *_track_id = read_file(track_id_path);
		playing_id = atoi(_track_id);
		free(_track_id);
	} 
	
	char *_vol = read_file( volume_path );
	if(_vol){
		output_volume = *(double*)_vol;
		free(_vol);
	}

	fd = open(fifopath, O_RDONLY | O_NONBLOCK);
	struct pollfd fds[1];
	fds[0].fd = fd;
	fds[0].events = POLLIN;

	char fifobuf[128];

	// Clear FIFO in case of offline writes
	while(read(fd, fifobuf, 128) );

	time_t last_draw = time_ms();

	// volume in seekload doesnt work on all machines?
	set_volume(output_volume);

	clear();

	// Main loop
	while(1){
		int events = 0; // Unused for now, was used to control redraws

		// Check if current track ended
		while(1){
			mpv_event *event = mpv_wait_event(ctx, 0.0f);
			if(!event->event_id) break;

			scuep_logf("MPV: %s", mpv_event_name(event->event_id) );

			if( event->event_id == MPV_EVENT_IDLE) {
				if( mpv_state == MPV_INIT )	{
					mpv_state = MPV_PLAY;
					seekload( library+playing_id );
				} else
					next(1);
				events++;
				queue_redraw(ELEMENT_LIBRARY);
				queue_redraw(ELEMENT_PROGRESS);
			}
		}

		// Process remote
		if(poll( fds, 1, 0 ) > 0) {
			memset(fifobuf, 0, 64);
			read(fd, fifobuf, 128);

			char *head = fifobuf;
			char *tail = fifobuf;
			
			while( *head ){		
				switch( *head ) {
					case '\n':
						*head = 0;
						run_command(tail);
						tail = head+1;
						events++;
						break;
				}
				head++;
			}
		}
		
		// Process input, also sleeps 100ms
		events += input();
		
		time_t now = time_ms();
		if( now > last_draw + 1000 ){
			last_draw = now;
			queue_redraw(ELEMENT_PROGRESS);
		}
		
		if(selection_follows)
			selected_id = playing_id;

		draw();

	}

	quit();
}

