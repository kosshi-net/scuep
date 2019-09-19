#define _XOPEN_SOURCE_EXTENDED
#include <ncurses.h>
#include <libcue/libcue.h>
#include <locale.h>
#include <fcntl.h>


#include <string.h> 
#include <unistd.h>

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h> 
#include <sys/un.h> 
#include <sys/select.h>
#include <sys/inotify.h>

#include <pthread.h>

#include <poll.h>
#include "filehelper.h"

#include <taglib/tag_c.h>	
#include <mpv/client.h>

char track_id_path[512];
char playlist_path[512];

int nosave = 0;

mpv_handle *ctx; 
enum { 
	MPV_INIT,
	MPV_PLAY
} mpv_state = MPV_INIT;

struct LibraryItem
{
	wchar_t *album;
	wchar_t *performer;
	wchar_t *track;
	char *path;
	
	long    start;
	long    length;

	int8_t  chapter;
	uint8_t marked   : 1;
	uint8_t error    : 1;
	uint8_t disabled : 1;
	uint8_t warning  : 1;
};


uint32_t 			 layout_col_width = 32;

struct LibraryItem 	*library;
uint32_t			 library_items;

uint32_t			 playing_id  = -1;
uint32_t			 selected_id = -1;
char 			   	 selection_follows = 1;

enum {
	MODE_DEFAULT,
	MODE_COMMAND,
	MODE_SEARCH,
} input_mode = MODE_DEFAULT;

char     command[128];
wchar_t  command_wchar[128];
uint32_t command_cursor = 0;

// vim style repeat
uint32_t				 repeat = 0;

#define MAX_ITEMS (1<<10)
#define PADX 4




time_t time_ms(){
    struct timeval t;
    gettimeofday(&t, NULL);
    return t.tv_sec*1000+t.tv_usec/1000;
}

char *read_file(char *path){
	FILE *f = fopen(path, "r");

	if(f==NULL) return NULL;

	fseek(f, 0, SEEK_END);
	long fsize = ftell(f);
	fseek(f, 0, SEEK_SET); 

	char *string = calloc(fsize + 16, 1 );
	if(string==NULL) return NULL;

	fread(string, 1, fsize, f);
	fclose(f);

	return string;
}

uint32_t row, col;


void draw_progress(long start, long length){

	char *receive = mpv_get_property_string(ctx, "playback-time");

	int length_seconds = length;
	int start_seconds = start;

	int mpv_seconds = 0;
	if(receive)
		mpv_seconds = atoi( receive ) - start_seconds;

	char *volstr = "Volume: 100%";
	int vollen = strlen(volstr);
	mvprintw( row-2, col-PADX-vollen, "%s", volstr );

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


wchar_t				*library_wide;
size_t				 library_wide_size;

char				*library_char;
size_t				 library_char_size;

char *scuep_basename(char*c){
	char *last = c;
	while(*++c != '\0'){
		if( *c == '/' ) last = c;
	}
	return last+1;
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
	char *c = playlist; // c will be reused
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
		*_line_buf = '\0'; // Null terminate line_buf
		c++;

		char *url = line_buf;
		
		if( i%10 == 0 ){
			erase();

			char *str = "Parsing metadata ...";

			mvprintw(0,0, "%s %i %%", 
				str, 
				(int)(i/(float)track_count * 100.0)
			);
			refresh();
		}

		// Make sure we have enough memory
		// If no, alloc a new block
		// Previous one is leaked currently !!
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

		char *utf8_title  = NULL;
		char *utf8_artist = NULL;
		char *utf8_album  = NULL;

		if(url[0] == '/'){
			library[i].path 	 = library_char+ci;
			
			// Copy path
			library[i].path = library_char+ci;
			for (
				int j = 0;
				url[j] != '\0' && j< MAX_STR_LEN-1;
				library_char[ci++] = url[j++]
			);
			library_char[ci++] = '\0';

			tl_file     = taglib_file_new( url );
			TagLib_Tag  *tl_tag = taglib_file_tag( tl_file );
			utf8_title  = taglib_tag_title (tl_tag);
			utf8_artist = taglib_tag_artist(tl_tag);
			utf8_album  = taglib_tag_album (tl_tag);

			const TagLib_AudioProperties *tl_prop = taglib_file_audioproperties( tl_file );
			library[i].start = 0;
			library[i].length = taglib_audioproperties_length( tl_prop );

			library[i].chapter 	 = -1;
		} else {
			// Parse chapter
			char *chap = url + strlen(url);
			while(*--chap != '/');
			*chap++ = '\0';
			int chapter = atoi(chap);
			if(chapter == 0) exit(1);

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
				//library[i].warning = 1;
				char *cue_filename = track_get_filename(track);
				char cue_path[1024*4];
				strcpy( cue_path, library[i].path );
				strcpy( scuep_basename(cue_path), cue_filename );

				// Use taglib to get the length of the whole file
				// Usually happens with last tracks in .cue
				tl_file = taglib_file_new( cue_path );
				const TagLib_AudioProperties *tl_prop = taglib_file_audioproperties( tl_file );
				library[i].length = taglib_audioproperties_length( tl_prop ) - library[i].start;
			}
		}
	
		if(!utf8_album)  utf8_album = "";
		if(!utf8_title)  utf8_title = "";
		if(!utf8_artist) utf8_artist = "";

		library[i].album = library_wide+wi;
		wi += mbstowcs(library_wide+wi, utf8_album, MAX_STR_LEN-1 );
		library_wide[wi++] = '\0';

		library[i].performer = library_wide+wi;
		wi += mbstowcs(library_wide+wi, utf8_artist, MAX_STR_LEN-1 );
		library_wide[wi++] = '\0';

		library[i].track = library_wide+wi;
		wi += mbstowcs(library_wide+wi, utf8_title, MAX_STR_LEN-1 );
		library_wide[wi++] = '\0';

		// Cleanup
		// Both libraries clean up utf8_ vars themselves
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


uint32_t scuep_wcslice(wchar_t* dst, wchar_t *wc, uint32_t max_width ){
	size_t width = 0;
	while(*wc && width < max_width){
		*dst++ = *wc;
		width += wcwidth( *wc++ );
	}
	*dst++ = 0;
	return width;
}

 
#define TEXT_ALIGN_RIGHT 1

void draw_and_search(uint32_t x, uint32_t y, wchar_t*text, uint32_t LCW, uint32_t options){
	static wchar_t slicebuf[512]; 
	uint32_t w = scuep_wcslice(slicebuf, text, LCW-2);
	if( options & TEXT_ALIGN_RIGHT ) x = x + (LCW-2 - w);
	mvprintw( y, x, "%S", slicebuf );
	if( w >= LCW-2 ) mvprintw( y, x+LCW-4, "%s", ".." );
	wchar_t *substring = NULL;
	if( command[0] == '/' && command_cursor > 1 )
		substring = wcsstr( text, command_wchar+1 );
	if(substring) {
		attron(COLOR_PAIR(2));
		ptrdiff_t index = substring - text;
		mvprintw( y, x+index, "%.*S", command_cursor-1, slicebuf + (index));
		attroff(COLOR_PAIR(2));
	}
}

int blink = 0;
int blink_read = 0;

void draw(){
	erase();

	getmaxyx(stdscr, row, col);

	draw_progress( library[playing_id].start, library[playing_id].length );

	//uint32_t LCW = layout_col_width;

	uint32_t center=MIN( (row/2), selected_id+3);

	for (int32_t i = selected_id-center; i < (int)library_items; ++i)
	{

		int y = i - selected_id + center;

		if( y < 3 ) continue;
		if( y + 4 > (unsigned int)row ) break;


		
		if(library[i].marked){
			mvprintw( y, 2, "%s", "*");
		}

		if(playing_id==i){
			mvprintw( y, 1, "%s", ">");
		}

		if(library[i].warning){
			attron(COLOR_PAIR(3));
		}

		if(selected_id==i){
			attron(COLOR_PAIR(1));
		}

		draw_and_search( PADX+0, 		y,  library[i].track,     32,   0 );
		draw_and_search( PADX+32, 		y,  library[i].performer, 32,   0 );
		draw_and_search( PADX+32+32, y,  library[i].album, col-32-32-PADX-2,  1 );

		attroff(COLOR_PAIR(1));
	}


	mvprintw( 1, PADX, "Playlist %i/%i ", selected_id, library_items );

	blink = !blink;

	char *title = "scuep 2.0";

	mvprintw( 1, col-strlen(title)-PADX, "%s", title );
	if(blink) 		mvprintw( 1l, col-PADX, "*" );
	if(blink_read)  mvprintw( 1l, col-PADX+1, "*" );

 
	mvprintw( row-1, 0, "%S", command_wchar );

	refresh();
}

int scuep_match( struct LibraryItem *item, wchar_t *match ){
	return ( 
		wcsstr( 
			item->track, 
			match
		) || 
		wcsstr( 
			item->performer, 
			match
		) ||
		wcsstr( 
			item->album, 
			match
		)
	);
}


void scuep_search(){
	for (uint32_t i = 0; i < library_items; ++i) {
		uint32_t j = (selected_id + i + 1) % library_items;
		if( 
			wcsstr( 
				library[ j ].track, 
				command_wchar+1
			) || 
			wcsstr( 
				library[ j ].performer, 
				command_wchar+1
			) ||
			wcsstr( 
				library[ j ].album, 
				command_wchar+1
			)
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
	return;
}

void quit(){
	endwin();
	exit(0);
}

void mpverr( int status  ){
	if(status < 0){
		endwin();
		printf("MPV API error: %s\n", mpv_error_string(status));
		exit(0);
	}	
}

int sprinturl( char *dst, struct LibraryItem *item ){
	if( item->chapter == -1 )
		return sprintf(dst, item->path );
	else
		return sprintf(dst, "cue://%s/%i", 
			item->path,
			item->chapter
		);
}

char cmd_buf[1024*8];

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

void run_command(){
	if(command[1] == 'q')
		quit();

	if(command[1] == '!'){
		int num_marked = 0;
		for( int i = 0; i < library_items; i++ ){
			if(!library[i].marked) continue;
			num_marked++;
			shell_item(library+i);
		}
		if(!num_marked) shell_item( library+selected_id );
	}

	if( strstr(command+1, "m/") == command+1 ){
		for( int i = 0; i < library_items; i++ ){
			if( scuep_match(library+i, command_wchar+3 ) ) {
				library[i].marked = 1;
			}
		}
	}

	command_cursor = 0;
	command[command_cursor] = 0;
}


void seekload( struct LibraryItem *item ){
	char *mpv_cmd[8]={NULL}; 
	mpv_cmd[0] = "loadfile"; 
	mpv_cmd[1] = item->path;
	mpv_cmd[2] = "replace";
	mpv_cmd[3] = malloc(16);
	sprintf( mpv_cmd[3], "start=#%i,end=#%i", item->chapter, item->chapter+1);
	mpverr(mpv_command(ctx, (const char**)mpv_cmd)); 
	free(mpv_cmd[3]);

	save_state();
	return;
}

char debug[128] = {0};

void playpause(){
	char *mpv_cmd[8]={NULL};
	mpv_cmd[0] = "cycle";
	mpv_cmd[1] = "pause";
	mpverr(mpv_command(ctx, (const char**)mpv_cmd)); 
	printf("cycle pause");
}

void seek(int seconds){
	char *mpv_cmd[8]={NULL}; 
	mpv_cmd[0] = "seek"; 
	mpv_cmd[1] = malloc(16);
	sprintf( mpv_cmd[1], "%i", seconds );
	mpverr(mpv_command(ctx, (const char**)mpv_cmd)); 
	free(mpv_cmd[1]);
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


// Returns count events processed
int input(void){
	
	timeout(100);
	int key = getch();
	int events = 0;
	
	while( key !=  ERR ) {
		events++;

		
		if( input_mode == MODE_DEFAULT ) 			goto SWITCH_MODE_DEFAULT;
		if( input_mode == MODE_COMMAND )	 		goto SWITCH_MODE_COMMAND;
		if( input_mode == MODE_SEARCH ) 			goto SWITCH_MODE_SEARCH;

		SWITCH_MODE_SEARCH:
		input_mode = MODE_DEFAULT;
		goto SWITCH_MODE_DEFAULT;
		goto END;


		SWITCH_MODE_COMMAND:
		switch(key){
			case 27:
				command_cursor = 0;
				command[command_cursor] = 0;
				input_mode = MODE_DEFAULT;
				break;
			case KEY_BACKSPACE:
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
					run_command();
					input_mode = MODE_DEFAULT;
				}
				break;
			default:
				command[command_cursor++] = key;
				command[command_cursor] = 0;
				break;
		}
		mbstowcs(command_wchar, command, 128 );
		goto END;


		SWITCH_MODE_DEFAULT:

		switch(key){
			case 27: // escape

				if( !selection_follows ){
					selection_follows = 1;
					selected_id = playing_id;
					break;
				}

				//quit();
				break;
			

			case KEY_LEFT:
				seek(-5*MAX( 1, repeat ));
				repeat=0;
				break;
			case KEY_RIGHT:
				seek( 5*MAX( 1, repeat ));
				repeat=0;
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
				break;
			case 'm':
				library[selected_id].marked = !library[selected_id].marked;
				break;

			case 'g':
				selected_id = repeat;
				selected_id = (library_items + selected_id) % library_items;
				selection_follows = 0;
				repeat = 0;
				break;


			case '\n':
				playing_id = selected_id;
				selection_follows = 1;
				repeat = 0;
				seekload( library+selected_id  );
				break;
			// CMUS CONTROLS
			case 'z':
				//system("scuep-remote prev");
				prev( MAX( 1, repeat ) );
				break;
			case 'b':
				next( MAX( 1, repeat ) );
				break;
			case 'c':
				//system("scuep-remote pause");
				playpause();
				break;
			case 'i':
				// Focus currently playing
				break;
			case 'l':
				clear();
				break;
			case 'N':
			case 'n':
				if(command[0] == '/')
					scuep_search();
				break;
			case '/':
			case ':':
				command_cursor = 0;
				input_mode = MODE_COMMAND;
				command[command_cursor++] = key;
				command[command_cursor] = 0;
				mbstowcs(command_wchar, command, 128 );
				break;
			default:
				break;
		}
		goto END;

		END:
		timeout(0);
		key = getch();
	}

	return events;
}



enum CLIOptions {
	option_default,
	option_help,
	option_version,
	option_nosave,
	option_stdin
};

enum CLIOptions parse_cli_option( char* str ){
	if( strcmp( str, "--help"
	)==0) return option_help;

	if( strcmp( str, "--version"
	)==0) return option_version;

	if( strcmp( str, "--nosave"
	)==0) return option_nosave;

	if( strcmp( str, "-"
	)==0) return option_stdin;
	if( strcmp( str, "-i"
	)==0) return option_stdin;

	return option_default;
}


char *read_stdin(){
	size_t buffer_size = 1024*4; // 4kb
	size_t buffer_index = 0;
	char *buffer = malloc(buffer_size);
	int c = 0;
	while( (c = getchar()) != EOF ) {
		buffer[buffer_index++] = c;
		if(buffer_index == buffer_size){
			buffer_size*=2;
			buffer = realloc(buffer, buffer_size);
		}
	}
	return buffer;
}



int main(int argc, char **argv)
{
	// Build path vars
	char *home = getenv("HOME");
	snprintf( track_id_path, 512, "%s/.config/scuep/track_id", home);
	snprintf( playlist_path, 512, "%s/.config/scuep/playlist", home);


	char *playlist = NULL;
	int new_playlist = 0;

	for(int i = 1; i < argc; i++){
		char*arg = argv[i];
		enum CLIOptions flag = parse_cli_option(arg);
		
		switch(flag){
			case option_help:
				printf("Err\n");
				quit();
				break;
			case option_version:
				printf("0.1\n");
				quit();
				break;
			case option_nosave:
				nosave = 1;
				break;
			case option_stdin:
				playlist = read_stdin();
				new_playlist = 1;
				break;
			default:
				// Assume its a file
				playlist = read_file(arg);
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
	mpverr(mpv_set_option_string(ctx, "input-default-bindings", 	"no"));
	mpverr(mpv_set_option_string(ctx, "input-vo-keyboard", 		"no"));
	mpverr(mpv_set_option_string(ctx, "vo", "null"));
	mpverr(mpv_set_option(ctx, "osc", MPV_FORMAT_FLAG, &val));
	mpverr(mpv_initialize(ctx));

	// Locale must be set before database build, and ncurses
  	setlocale(LC_ALL,"");

	// Create socket
	char fifopath[512];
	snprintf( fifopath, 512, "%s/.config/scuep/fifo", home);
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
	use_default_colors();
	start_color();
	init_pair(1, 13, -1);
	init_pair(2, COLOR_BLACK, COLOR_RED);
	init_pair(3, COLOR_YELLOW, -1);

	// Scan files and build DB
	build_database(playlist);
	
	if( new_playlist ) save_playlist(playlist);

	free(playlist);
	
	// Start playback
	playing_id = 0;
	if(!new_playlist) {
		char *_track_id = read_file(track_id_path);
		playing_id = atoi(_track_id);
		free(_track_id);
	} 
	

	fd = open(fifopath, O_RDONLY | O_NONBLOCK);
	struct pollfd fds[1];
	fds[0].fd = fd;
	fds[0].events = POLLIN;

	char fifobuf[128];

	// Clear FIFO in case of offline writes
	while(read(fd, fifobuf, 128) );

	time_t last_draw = time_ms();

	// Main loop
	while(1){
		int events = 0;

		// Check if current track ended
		while(1){
			mpv_event *event = mpv_wait_event(ctx, 0.0f);

			if(!event->event_id) break;
			if( event->event_id == MPV_EVENT_IDLE) {
				if( mpv_state == MPV_INIT )	{
					mpv_state = MPV_PLAY;
					seekload( library+playing_id );
				} else
					next(1);
				events++;
			}
				
			sprintf(debug, "Debug: %s\n", mpv_event_name(event->event_id));
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
						if( strcmp(tail, "pause")==0
						 || strcmp(tail, "play" )==0 ) playpause();
						if( strcmp(tail, "next" )==0 ) next(1);
						if( strcmp(tail, "prev" )==0 ) prev(1);
						tail = head+1;
						events++;
						break;
				}
				head++;
			}
		}
		
		// Process input
		// Also sleeps 100ms
		events += input();
		
		// Force redraw once a second
		int now = time_ms();
		if( now > last_draw + 1000 ) events++;
		
		// Dont draw if no events
		if( events == 0 ) continue;
		last_draw = now;
		
		if(selection_follows)
			selected_id = playing_id;

		draw();
		/*
		mvprintw(0, 40, debug);
		mvprintw(0,40+30, "fifo: %s", fifobuf);
		mvprintw(0,40+40, "c%i w%i (%iKB)", debug_char_blocks, debug_wide_blocks, 
		debug_bytes / 1024);*/
	}
	
	quit();
}
