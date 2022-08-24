#include "config.h"

#include "frontend.h"
#include "database.h"
#include "player.h"

#include "util.h"

#include <stdlib.h>
#include <unistd.h>

#include <ncurses.h>
#include <locale.h>

#include <string.h>
#include <wchar.h>


#include <wchar.h> 
#include <locale.h>


/*
 * To avoid unnecessary redraws, use queue_redraw(ELEMENT_*) when relevant 
 * state changes happen.
 * */

#define ELEMENT_CLEAR    (1<<0) // Clear all elemenets. Use ELEMENT_ALL to set

#define ELEMENT_PROGRESS (1<<1)
#define ELEMENT_CAROUSEL (1<<2) 
#define ELEMENT_PROMPT   (1<<3) 
#define ELEMENT_DEBUG    (1<<4) 

#define ELEMENT_PROPERTIES (1<<4) 

#define ELEMENT_ALL      (0xFFFF-ELEMENT_PROPERTIES) // Redraw all elements

// Use via queue_redraw(), not directly
static uint32_t elements_dirty = ELEMENT_ALL; 

void queue_redraw(int elem){
	elements_dirty |= elem;
}



static void draw_carousel();
static void draw_progress();
static void draw_debug();
static void input();

static int term_cols = 0;
static int term_rows = 0;

static SCREEN *screen = NULL;

// These are zero-indexed!
static int32_t playlist_items;
static int32_t cursor_pos = 0;

enum {
	MODE_DEFAULT,
	MODE_COMMAND,
	MODE_SEARCH,
} input_mode = MODE_DEFAULT;

static int input_repeat = 0;

static bool should_quit = false;

int frontend_initialize(void)
{
	

	
	const char *term_type = getenv("TERM");
	FILE* term_in = fopen("/dev/tty", "r");

	screen = newterm(term_type, stdout, term_in);
	
	cbreak();              // 
	noecho();              // input echo
	curs_set(0);           // Disable cursor
	keypad(stdscr, TRUE);  // Arrow keys

	notimeout(stdscr, FALSE);
	ESCDELAY = 25;
	use_default_colors();
	start_color();
	init_pair(1, 13, -1);
	init_pair(2, COLOR_BLACK, COLOR_RED);
	init_pair(3, COLOR_YELLOW, -1);
	init_pair(4, COLOR_RED, -1);

	playlist_items = playlist_count();

	
	while( !should_quit ){
		frontend_tick();
	}


	return 0;
}

int frontend_tick(void)
{
	input();


	int _rows, _cols;
	getmaxyx( stdscr, _rows, _cols );
	if( _rows != term_rows || _cols != term_cols ){
		queue_redraw(ELEMENT_ALL);
		term_cols = _cols;
		term_rows = _rows;
	}

	static int progress_last = -1;
	int progress_now = floorf(player_position_seconds());
	if( progress_last != progress_now ){
		queue_redraw(ELEMENT_PROGRESS);
		progress_last = progress_now;
	}

	if( elements_dirty & ELEMENT_CLEAR    ) clear();
	if( elements_dirty & ELEMENT_CAROUSEL ) draw_carousel();
	if( elements_dirty & ELEMENT_PROGRESS ) draw_progress();
	if( true                              ) draw_debug();

	if(elements_dirty) refresh();
	elements_dirty = 0;

	return 0;
}

int frontend_terminate(void)
{
	endwin();
	return 0;
}

void input_default( int key )
{
	switch( key ){
		
		case '\n':
		case KEY_ENTER:
			player_load( playlist_track( 1 + cursor_pos ) );
			player_play();
			break;

		case KEY_RIGHT:
			player_seek_relative( 5.0);
			break;
		case KEY_LEFT:
			player_seek_relative(-5.0);
			break;

		case 'z':
			// previous
			break;
		case 'b':
			// next
			break;
		
		case 'x':
			player_seek(0);
			player_play();
			break;

		case 'v':
			player_stop();
			break;


		case 'c':
		case ' ':
			player_toggle();
			break;

		case 'k':
		case KEY_UP:
			cursor_pos -= MAX( 1, input_repeat );
			input_repeat = 0;
			cursor_pos = ( playlist_items + cursor_pos ) % playlist_items;
			queue_redraw(ELEMENT_CAROUSEL);
			break;	
		case 'j':
		case KEY_DOWN:
			cursor_pos += MAX( 1, input_repeat );
			input_repeat = 0;
			cursor_pos = ( playlist_items + cursor_pos ) % playlist_items;
			queue_redraw(ELEMENT_CAROUSEL);
			break;	
		case 'q':
			should_quit = true;
			break;	

		case '1': case '2': case '3': 
		case '4': case '5': case '6': 
		case '7': case '8': case '9':
		case '0': 
			input_repeat = input_repeat*10 + (key-'0');
			break;
	}
}

void input(){

	timeout(100);
	int key = getch();

	while( key != ERR ){
		
		switch(input_mode){
			case MODE_DEFAULT:
				input_default(key);
				break;
			default:
				break;
		}


		timeout(0);
		key = getch();
	}

}



#define ALIGN_RIGHT 1
void carousel_text( int row, int col, int w, wchar_t *wctext, int flags )
{
	static wchar_t wccut[1024] = {0};
	uint32_t wcw;
	int cut = scuep_wcslice( wccut, wctext, w-2, &wcw ); 
	
	if( flags & ALIGN_RIGHT ){
		int total = wcw + (w-wcw)*cut;
		col -= total;
	}
	mvprintw( row, col, "%S", wccut);
	if( cut )
		mvprintw( row, col+wcw, "%.*s", w-wcw, ".....");
}

#define CAROUSEL_PAD_TOP 2
#define CAROUSEL_PAD_BOTTOM 2

static uint32_t rcount = 0;

void draw_carousel()
{
	mvprintw(0, 5, "%i, %lu, playlist: %i / %i", rcount++, time_ms(), cursor_pos , playlist_items);


	static wchar_t wctext[1024] = {0};

	int items = playlist_count();
	int center = MIN( term_rows/2, cursor_pos+CAROUSEL_PAD_TOP );
	int row;

	for( int i = cursor_pos-center; i < items; i++ ){

		//int row = i + CAROUSEL_PAD_TOP;
		row = i - cursor_pos + center;

		if( row < CAROUSEL_PAD_TOP ) continue;
		if( row >= term_rows-CAROUSEL_PAD_BOTTOM ) break;

		TrackId trackid = playlist_track( i+1 );
		struct ScuepTrack *track = track_load(trackid);

		move(row, 0);
		clrtoeol();

		if( i == cursor_pos ){
			mvprintw( row, 1, ">" );
		}
		mbstowcs(wctext, track->title, 1023);
		carousel_text(row, 2, 31, wctext, 0);
		
		mbstowcs(wctext, track->artist, 1023);
		carousel_text(row, term_cols-2-31-5-21, 21, wctext, 0);

		mbstowcs(wctext, track->album, 1023);
		carousel_text(row, term_cols-2, 31, wctext, ALIGN_RIGHT);

		track_free(track);
	}

	while ( row < term_rows-CAROUSEL_PAD_BOTTOM ){
		
		move(++row, 0);
		clrtoeol();

	}

}

void draw_debug()
{
	struct PlayerState *player = _get_playerstate();

	move(1,0);
	clrtoeol();

	if(!player){
		mvprintw(1,0, "%s", "Player uninitialized" );
		
	} else {

		mvprintw(1,0, 
			" paused: %i"
			" done: %i"
			" decoder: %i"
			" sndsvr %i"
			" buffer: %i"
			,player->pause 
			,player->head.done 
			,player->av.thread_run 
			,!!player->sndsvr_close
			,player->head.total - player->tail.total

		);



	}
	


}

void draw_progress()
{
	float fprogress = player_position_seconds();
	int progress = fprogress;
	mvprintw( 0, 50, "pos: %f", fprogress );

	int duration = player_duration_seconds();

	mvprintw( 0, 70, "%i:%02i / %i:%02i        ", 
		progress/60,
		progress%60,
		duration/60,
		duration%60
	);
};




