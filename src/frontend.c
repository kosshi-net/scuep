#include "config.h"

#include "frontend.h"
#include "database.h"
#include "player.h"
#include "log.h"

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

#define KEY_ESCAPE 27

#define LENGTH(arr) (sizeof(arr)/sizeof(*arr))

/* Use via queue_redraw(), not directly */
static uint32_t elements_dirty = ELEMENT_ALL; 

void queue_redraw(int elem){
	elements_dirty |= elem;
}


static void draw_carousel();
static void draw_progress();
static void draw_debug();
static void draw_prompt();
static void input();

static int term_cols = 0;
static int term_rows = 0;
static int debug_mode = 0;

static SCREEN *screen = NULL;

// These are zero-indexed!




static struct {
	int32_t input_repeat;
	bool    should_quit;

	/* Zero indexed */
	int32_t playlist_items;
	int32_t active;

	int32_t cursor;
	bool    cursor_locked;

	struct {
		wchar_t prefix[512];

		uint32_t c_len;
		char     c[128];
		uint32_t w_len;
		wchar_t  w[1024];

		int32_t cursor;
	} cmd;

	enum {
		MODE_DEFAULT,
		MODE_COMMAND,
		MODE_SEARCH,
	} input_mode;

} this = {
	.cursor_locked = true,
	.input_mode = MODE_DEFAULT,
};

static struct {
	int32_t pad[2];
	int32_t carousel[2];
	int32_t progress;
	int32_t debug;
	int32_t prompt;
} layout;

void prompt_set_prefix(char *str){
	mbstowcs(this.cmd.prefix, str, LENGTH(this.cmd.prefix));
}

void prompt_set_prefix_w(wchar_t *str){
	wcsncpy(this.cmd.prefix, str, LENGTH(this.cmd.w));
}

void command_clear_c(void)
{
	memset(this.cmd.c, 0, sizeof(this.cmd.c));
	this.cmd.c_len = 0;
	scuep_logf("cmd.c cleared\n");
}
void command_clear(void)
{
	command_clear_c();
	memset(this.cmd.w, 0, sizeof(this.cmd.w));
	this.cmd.w_len = 0;
	this.cmd.cursor = 0;
	queue_redraw(ELEMENT_PROMPT);
}

void command_delete(int32_t pos){
	if(pos < 0) return;
	if(pos >= this.cmd.w_len) return;
	
	for (int i = pos; i < this.cmd.w_len; i++) {
		this.cmd.w[i] = this.cmd.w[i+1];
	}
	this.cmd.w_len--;
}

void command_insert(char c)
{
	scuep_logf("Insert %i to %i\n", c, this.cmd.c_len);
	this.cmd.c[this.cmd.c_len++] = c;

	wchar_t w[128];
	size_t ret = mbstowcs(w, this.cmd.c, LENGTH(w));

	if (ret == -1) {
		scuep_logf("cmd.c invalid\n");
		return;
	}

	scuep_logf("String valid: [%s], committing\n", this.cmd.c);

	this.cmd.w_len += ret;
	for (int32_t i = this.cmd.w_len+ret; i > this.cmd.cursor; i--) {
		this.cmd.w[i] = this.cmd.w[i-ret];
	}

	for (size_t i = 0; i < ret; i++){
		this.cmd.w[this.cmd.cursor++] = w[i];

	}

	command_clear_c();
}

void input_command(int key)
{

	scuep_logf("%i\n", key);
	switch (key) {
		case KEY_ESCAPE: 
			this.input_mode = MODE_DEFAULT;
			prompt_set_prefix("");
			break;
		case KEY_BACKSPACE:
		case 127:
			this.cmd.cursor--;
			command_delete(this.cmd.cursor);
			break;
		case 330: /* Delete */
			command_delete(this.cmd.cursor);
			break;

		case KEY_LEFT:
			this.cmd.cursor--;
			break;
		case KEY_RIGHT:
			this.cmd.cursor++;
			break;
		
		case KEY_ENTER:
			this.input_mode = MODE_DEFAULT;
			break;

		default:
			command_insert(key);
			break;
	}
	this.cmd.cursor = MIN(MAX(this.cmd.cursor, 0),this.cmd.w_len);
	queue_redraw(ELEMENT_PROMPT);
}

void layout_update()
{
	layout.pad[0] = 4;
	layout.pad[1] = 1;

	if (term_cols < 32) layout.pad[0] = 0;

	layout.progress = term_rows - layout.pad[1] - 1;

	layout.debug = term_rows - 5;
	if(debug_mode) layout.progress -= 5;

	layout.prompt = layout.progress+1;

	layout.carousel[0] = 3;
	layout.carousel[1] = layout.progress-1;

}

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
	init_pair(5, COLOR_BLACK, COLOR_WHITE);

	this.playlist_items = playlist_count();

	
	while( !this.should_quit ){
		frontend_tick();
	}


	return 0;
}


void frontend_next(int32_t num);

int frontend_tick(void)
{
	input();

	int _rows, _cols;
	getmaxyx( stdscr, _rows, _cols );
	if (_rows != term_rows || _cols != term_cols){
		queue_redraw(ELEMENT_ALL);
		term_cols = _cols;
		term_rows = _rows;
	}

	static int progress_last = -1;
	int progress_now = floorf(player_position_seconds());
	if (progress_last != progress_now) {
		queue_redraw(ELEMENT_PROGRESS);
		progress_last = progress_now;
	}

	layout_update();

	if (elements_dirty & ELEMENT_CLEAR   ) clear();
	if (elements_dirty & ELEMENT_CAROUSEL) draw_carousel();
	if (elements_dirty & ELEMENT_PROGRESS) draw_progress();
	if (elements_dirty & ELEMENT_PROMPT  ) draw_prompt();
	if (debug_mode                       ) draw_debug();

	if(elements_dirty) refresh();
	elements_dirty = 0;


	/*
	 * Temporary autoplay 
	 * TODO: utilize predecoding ability of player for smooth transitions
	 */
	struct PlayerState *player = _get_playerstate();
	if (player) {
		if (player->head.done 
		&&  player->head.total - player->tail.total == 0 
		&& !player->pause
		) {
			frontend_next(1);
		}
	}

	return 0;
}

int frontend_terminate(void)
{
	endwin();
	return 0;
}

void frontend_play(int id)
{
	this.active = id;
	player_load( playlist_track( id+1 ) );
	player_play();

	if (this.cursor_locked)
		this.cursor = this.active;

	queue_redraw(ELEMENT_CAROUSEL);
}

void frontend_next(int32_t num){
	this.active += num;
	this.active = ( this.playlist_items + this.active ) % this.playlist_items;
	frontend_play(this.active);
}

void cursor_lock()
{
	this.cursor_locked = true;
	this.cursor = this.active;
	queue_redraw(ELEMENT_ALL);
}


void cursor_free()
{
	this.cursor_locked = false;
}



void input_default(int key)
{
	switch (key) {
		case ':':
			command_clear();
			this.input_mode = MODE_COMMAND;
			prompt_set_prefix(":");
			break;

		case 'd':
			debug_mode = !debug_mode;
			queue_redraw(ELEMENT_ALL);
			break;
		case 'D':
			debug_quit_decoder();
			break;

		case '\n':
		case KEY_ENTER:
			frontend_play(this.cursor);
			cursor_lock();
			break;

		case KEY_RIGHT:
			player_seek_relative( 5.0);
			break;
		case KEY_LEFT:
			player_seek_relative(-5.0);
			break;

		case KEY_ESCAPE:
			cursor_lock();
			break;

		case 'z':
			frontend_next(-1);
			break;
		case 'b':
			frontend_next(1);
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
			cursor_free();
			this.cursor -= MAX(1, this.input_repeat);
			this.input_repeat = 0;
			this.cursor = ( this.playlist_items + this.cursor ) % this.playlist_items;
			queue_redraw(ELEMENT_CAROUSEL);
			break;	
		case 'j':
		case KEY_DOWN:
			cursor_free();
			this.cursor += MAX( 1, this.input_repeat );
			this.input_repeat = 0;
			this.cursor = ( this.playlist_items + this.cursor ) % this.playlist_items;
			queue_redraw(ELEMENT_CAROUSEL);
			break;
		case 'q':
			this.should_quit = true;
			break;	

		case '1': case '2': case '3': 
		case '4': case '5': case '6': 
		case '7': case '8': case '9':
		case '0': 
			this.input_repeat = this.input_repeat*10 + (key-'0');
			break;
	}
}

void input(){
	timeout(100);
	int key = getch();

	while (key != ERR) {
		switch(this.input_mode){
			case MODE_DEFAULT:
				input_default(key);
				break;
			case MODE_COMMAND:
				input_command(key);
				break;
			default:
				scuep_logf("Invalid input mode, resetting to default\n");
				this.input_mode = MODE_DEFAULT;
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
	
	if (flags & ALIGN_RIGHT) {
		int total = wcw + (w-wcw)*cut;
		col -= total;
	}
	mvprintw( row, col, "%S", wccut);
	if (cut)
		mvprintw( row, col+wcw, "%.*s", w-wcw, ".....");
}

static uint32_t rcount = 0;

void draw_carousel()
{

	mvprintw(1, layout.pad[0], "Playlist: %i / %i", this.cursor , this.playlist_items);
	mvprintw(1, term_cols-layout.pad[0] - 15, "scuep-ffsql 0.0" );

	static wchar_t wctext[1024] = {0};

	int items = playlist_count();
	int center = MIN( term_rows/2, this.cursor+layout.carousel[0] );
	int row;

	for (int i = this.cursor-center; i < items; i++) {

		row = i - this.cursor + center;

		if (row <  layout.carousel[0]) continue;
		if (row >= layout.carousel[1]) break;

		TrackId trackid = playlist_track( i+1 );
		struct ScuepTrack *track = track_load(trackid);

		move(row, 0);
		clrtoeol();

		if (i == this.cursor) {
			mvprintw( row, 1, "~" );
		}
		if (i == this.active) {
			mvprintw( row, 1, ">" );
		}

		int32_t pad = layout.pad[0];
		int32_t r = pad;
		int32_t l = pad;
		int32_t w = term_cols - r-l;
		int32_t title_min = 32;
		int32_t album_min = 25;
		int32_t artist_min = 25;

		if (w-title_min > album_min) {
			mbstowcs(wctext, track->album, 1023);
			carousel_text(row, term_cols - r, album_min, wctext, ALIGN_RIGHT);

			w -= album_min;
			r += album_min;
		}

		if (w-title_min > artist_min) {

			mbstowcs(wctext, track->artist, 1023);
			r += artist_min;
			carousel_text(row, term_cols-r, artist_min, wctext, 0);
			w -= artist_min;
			w -= 2; /* Leave a gap between title and artist*/
		}

		mbstowcs(wctext, track->title, 1023);
		carousel_text(row, l, w, wctext, 0);
		
		track_free(track);
	}

	while (row < layout.carousel[1]) {
		move(++row, 0);
		clrtoeol();
	}

}



void draw_debug()
{
	struct PlayerState *player = _get_playerstate();

	for (size_t i = 0; i < term_cols; i++) {
		mvprintw(layout.debug,i, "-" );
	}
	for (size_t i = layout.debug+1; i <term_rows; i++) {
		move(i,0);
		clrtoeol();
	}

	mvprintw(layout.debug, term_cols/2-3, " Debug " );


	if(!player){
		mvprintw(layout.debug+1,0, "%s", "Player uninitialized" );
	} else {
		mvprintw(layout.debug+1,0, 
			" paused: %i"
			" done: %i"
			" decoder: %i"
			" sndsvr %i"
			" buffer: %li"
			,player->pause 
			,player->head.done 
			,player->av.thread_run 
			,!!player->sndsvr_close
			,player->head.total - player->tail.total
		);
		mvprintw(layout.debug+2,0, 
			"%.02f / %.02f "
			"head: %li tail: %li",
			player_position_seconds(),
			player_duration_seconds(),
			player->head.ring,
			player->tail.ring
		);
		mvprintw(layout.debug+3,0, 
			"Input mode: %i, cursor: %i",
			this.input_mode, this.cmd.cursor
		);
	}
}

void draw_progress()
{
	float fprogress = player_position_seconds();
	int progress = round(fprogress);

	move(layout.progress, 0);
	clrtoeol();

	int duration = round(player_duration_seconds());

	char buf[1024];

	snprintf(buf, sizeof(buf), "%i:%02i / %i:%02i",
		progress/60,
		progress%60,
		duration/60,
		duration%60
	);

	int32_t r = strlen(buf) + layout.pad[0] + 2;
	int32_t l = term_cols - layout.pad[0];

	mvprintw(layout.progress, layout.pad[0], "%s", buf);

	if (l-r >= 20) {
		l-=11;
		mvprintw(layout.progress, l, "Volume 100%%");
		l-=2;
	}

	
	int32_t pos = floor( (l-r-1) * (fprogress/(float)duration) );
	pos += r;

	while (l-r > 0) {
		if (pos == r)
			mvprintw(layout.progress, r, "|");
		else
			mvprintw(layout.progress, r, "-");
		r++;
	}
};

void draw_prompt()
{
	move(layout.prompt, 0);
	clrtoeol();
	mvprintw(layout.prompt, 0, "%S",this.cmd.prefix);

	for (int32_t i = 0; i <= this.cmd.w_len; i++) {
		wchar_t wc = this.cmd.w[i];
		if(wc == 0) wc = ' ';
		if(i == this.cmd.cursor && this.input_mode == MODE_COMMAND)
			attron(COLOR_PAIR(5));
		printw("%C", wc);
		attroff(COLOR_PAIR(5));
	}

	if(0){
	mvprintw(layout.prompt, 1, "%S", this.cmd.w);

	if (this.input_mode == MODE_COMMAND) {

		move(layout.prompt, 1+this.cmd.cursor);

		attron(COLOR_PAIR(5));
		if (this.cmd.cursor < this.cmd.w_len)
			printw("%c", this.cmd.w[this.cmd.cursor]);
		else
			printw(" ");

		attroff(COLOR_PAIR(5));

	}
	}
}


