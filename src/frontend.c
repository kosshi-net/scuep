#include "config.h"
#include "frontend.h"
#include "database.h"
#include "player.h"
#include "log.h"
#include "shell.h"
#include "util.h"

#include <stdlib.h>
#include <unistd.h>
#include <ncurses.h>
#include <locale.h>
#include <string.h>
#include <wchar.h>
#include <locale.h>
#include <sys/stat.h>
#include <poll.h>

/* In VIM, use ":r!figlet Text" to print big labels */

/*
 ____        __ _
|  _ \  ___ / _(_)_ __   ___  ___
| | | |/ _ \ |_| | '_ \ / _ \/ __|
| |_| |  __/  _| | | | |  __/\__ \
|____/ \___|_| |_|_| |_|\___||___/
*/


#define KEY_ESCAPE 27


/*
 _____                       _           _
|  ___|   _ _ __   ___    __| | ___  ___| |
| |_ | | | | '_ \ / __|  / _` |/ _ \/ __| |
|  _|| |_| | | | | (__  | (_| |  __/ (__| |
|_|   \__,_|_| |_|\___|  \__,_|\___|\___|_|
*/


static void draw_carousel(void);
static void draw_progress(void);
static void draw_debug(void);
static void draw_prompt(void);
static void input(void);
static void queue_redraw(int elem);
static void poll_remote(void);
static void layout_update(void);
static void poll_remote(void);
static void prompt_set_prefix(const char *str);
static void frontend_search(int dir);
static void prompt_clear(void);
static void input_prompt(int key);
static void cursor_lock(void);
static void cursor_free(void);
static void frontend_play(int id);
static void input_default(int key);
static void prompt_delete(int32_t pos);
static void prompt_insert(char c);


/*
  ____ _       _           _
 / ___| | ___ | |__   __ _| |___
| |  _| |/ _ \| '_ \ / _` | / __|
| |_| | | (_) | |_) | (_| | \__ \
 \____|_|\___/|_.__/ \__,_|_|___/
*/

/* TODO: Put everything under "this" */

/*
 * To avoid unnecessary redraws, use queue_redraw(ELEMENT_*) when relevant
 * state changes happen.
 * */

/* ELEMENT_CLEAR clears all elemenets. Use ELEMENT_ALL to set */
#define ELEMENT_CLEAR      (1<<0)
#define ELEMENT_PROGRESS   (1<<1)
#define ELEMENT_CAROUSEL   (1<<2)
#define ELEMENT_PROMPT     (1<<3)
#define ELEMENT_DEBUG      (1<<4)
#define ELEMENT_PROPERTIES (1<<4)
/* Redraws all elements */
#define ELEMENT_ALL      (0xFFFF-ELEMENT_PROPERTIES)

/* Use via queue_redraw(), not directly */
static uint32_t elements_dirty = ELEMENT_ALL;

static int term_cols = 0;
static int term_rows = 0;
static int debug_mode = 0;


static struct {
	int32_t pad[2];
	int32_t carousel[2];
	int32_t progress;
	int32_t debug;
	int32_t prompt;
} layout;


/* The "this" struct */
static struct {
	SCREEN *screen;

	int32_t input_repeat;
	bool    should_quit;

	/* Zero indexed */
	int32_t playlist_items;

	int32_t cursor;
	bool    cursor_locked;

	struct {
		char    c[1024];
		wchar_t w[1024];
	} search;

	struct {
		/* Visual prompt label, eg ":" or "/". Also used for responses */
		wchar_t prefix[1024];
		uint32_t prefix_color;

		/* UTF-8 input staging buffer, flushed to widechar buffer when it's a
		 * valid UTF-8 string. */
		uint32_t c_len;
		char     c[128];

		/* Widechar prompt buffer.
		 * Assumed UTF-32, treated as grapheme clusters
		 * TODO: Use libgrapheme perhaps? */
		uint32_t w_len;
		wchar_t  w[1024];

		/* Cursor position in widechar prompt buffer */
		int32_t cursor;
	} cmd;

	struct {
		int fd;
		struct pollfd fds[1];
		char buffer[1024];
	} fifo;

	enum {
		MODE_DEFAULT,
		MODE_COMMAND,
		MODE_SEARCH,
	} input_mode;

} this = {
	.input_mode = MODE_DEFAULT,
};


/*
 __  __       _
|  \/  | __ _(_)_ __
| |\/| |/ _` | | '_ \
| |  | | (_| | | | | |
|_|  |_|\__,_|_|_| |_|
*/


int frontend_initialize(const char *fifopath)
{
	const char *term_type = getenv("TERM");
	FILE* term_in = fopen("/dev/tty", "r");

	this.screen = newterm(term_type, stdout, term_in);

	cbreak();
	noecho();              /* Input echo */
	curs_set(0);           /* Disable cursor */
	keypad(stdscr, TRUE);  /* Arrow keys */

	notimeout(stdscr, FALSE);
	ESCDELAY = 25;
	use_default_colors();
	start_color();
	init_pair(1, 13, -1);
	init_pair(2, COLOR_BLACK, COLOR_RED);
	init_pair(3, COLOR_YELLOW, -1);
	init_pair(4, COLOR_RED, -1);
	init_pair(5, COLOR_BLACK, COLOR_WHITE);
	init_pair(6, COLOR_BLACK, 12);
	init_pair(7, COLOR_BLACK, 14); /* Warn, black on yellow */
	init_pair(8, 15, COLOR_RED); /* Error, white on red */

	this.playlist_items = playlist_count();

	/* Set up fifo */
	mkfifo(fifopath, 0666);
	this.fifo.fd = open(fifopath, O_RDONLY | O_NONBLOCK);
	this.fifo.fds[0].fd     = this.fifo.fd;
	this.fifo.fds[0].events = POLLIN;
	/* Clear fifo in case it's been written into offline */
	while (read(this.fifo.fd, this.fifo.buffer, sizeof(this.fifo.buffer)-1));

	while (!this.should_quit) {
		frontend_tick();
	}

	return 0;
}


int frontend_tick(void)
{
	input();
	poll_remote();

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

	/* Detect when backend track changes. */
	static uint32_t prev_state;
	uint32_t new_state = player_state_key();
	if (new_state != -1 && prev_state != new_state) {
		queue_redraw(ELEMENT_ALL);
		if (this.cursor_locked) this.cursor = new_state;
	}
	prev_state = new_state;

	/*
	 * TODO: This is temporary hotwired autoplay, formalize the logic
	 */
	struct PlayerState *player = _get_playerstate();
	if (player) {
		if (player->head.done
		&&  player->head.total - player->tail.total == 0
		&& !player->pause
		) {
			frontend_next(1);
		}
		else
		if (player->head.done && !player->preload_failed) {
			/* Preload next track */
			uint32_t id = (this.playlist_items+player_state_key()+1) % this.playlist_items;
			player_load( playlist_track(id), id, true);
		}
	}

	return 0;
}


int frontend_terminate(void)
{
	endwin();
	return 0;
}


void poll_remote(void)
{
	if (poll(this.fifo.fds, 1, 0) < 1) {
		return;
	}

	int bytes = read(this.fifo.fd, this.fifo.buffer, sizeof(this.fifo.buffer)-1);
	if (bytes < 1) {
		return;
	}

	this.fifo.buffer[bytes] = 0;

	char *head = this.fifo.buffer;
	char *tail = head;

	while (*head) {
		switch (*head) {
		case '\n':
			*head = 0;
			shell_run(tail);
			tail = head+1;
			break;
		}
		head++;
	}
}


/*
 ___                   _
|_ _|_ __  _ __  _   _| |_
 | || '_ \| '_ \| | | | __|
 | || | | | |_) | |_| | |_
|___|_| |_| .__/ \__,_|\__|
          |_|
*/


void input(void)
{
	timeout(100);
	int key = getch();

	while (key != ERR) {
		switch(this.input_mode){
			case MODE_DEFAULT:
				input_default(key);
				break;
			case MODE_COMMAND:
			case MODE_SEARCH:
				input_prompt(key);
				queue_redraw(ELEMENT_CAROUSEL); /* For search highlighting */
				break;
			default:
				log_warn("Invalid input mode, resetting to default");
				this.input_mode = MODE_DEFAULT;
				break;
		}

		timeout(0);
		key = getch();
	}
}


void input_default(int key)
{
	switch (key) {
		case ':':
			prompt_clear();
			this.input_mode = MODE_COMMAND;
			prompt_set_prefix(":");
			break;

		case '?':
		case '/':
			prompt_clear();
			this.input_mode = MODE_SEARCH;
			prompt_set_prefix((char[2]) {key, '\0'});
			break;

		case 'n':
			frontend_search(+1);
			break;

		case 'N':
			frontend_search(-1);
			break;

		case 'm':
			{
				int mark = playlist_get_mark(this.cursor);
				if (mark > 0)
					playlist_set_mark(this.cursor, 0);
				else if (mark == 0)
					playlist_set_mark(this.cursor, 1);

				queue_redraw(ELEMENT_CAROUSEL);
			}
			break;

		case 'd':
			debug_mode = !debug_mode;
			queue_redraw(ELEMENT_ALL);
			break;

		case 'D':
			if (playlist_delete_marked(1) == 0) {
				playlist_delete(this.cursor);
			}
			this.playlist_items = playlist_count();
			queue_redraw(ELEMENT_ALL);
			break;

		case '\n':
		case KEY_ENTER:
			frontend_play(this.cursor);
			cursor_lock();
			break;

		case 'L':
			/* Preload track, debugging purposes */
			player_load( playlist_track(this.cursor), this.cursor, true);
			cursor_lock();
			break;

		case KEY_RIGHT:
			player_seek_relative(+5.0);
			break;
		case KEY_LEFT:
			player_seek_relative(-5.0);
			break;

		case KEY_ESCAPE:
			cursor_lock();
			queue_redraw(ELEMENT_ALL);
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
			queue_redraw(ELEMENT_CAROUSEL);
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
			this.cursor = (this.playlist_items + this.cursor) % this.playlist_items;
			queue_redraw(ELEMENT_CAROUSEL);
			break;
		case 'j':
		case KEY_DOWN:
			cursor_free();
			this.cursor += MAX( 1, this.input_repeat );
			this.input_repeat = 0;
			this.cursor = (this.playlist_items + this.cursor) % this.playlist_items;
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


void input_prompt(int key)
{
	switch (key) {
		case KEY_ESCAPE:
			this.input_mode = MODE_DEFAULT;
			prompt_set_prefix("");
			prompt_clear();
			break;

		case KEY_BACKSPACE:
		case 127:
			this.cmd.cursor--;
			prompt_delete(this.cmd.cursor);
			break;
		case 330: /* Delete */
			prompt_delete(this.cmd.cursor);
			break;

		case KEY_LEFT:
			this.cmd.cursor--;
			break;
		case KEY_RIGHT:
			this.cmd.cursor++;
			break;

		case KEY_ENTER: /* Keypad enter */
		case '\n':
			if (this.input_mode == MODE_COMMAND) {
				shell_run_w(this.cmd.w);
			}
			if (this.input_mode == MODE_SEARCH) {
				frontend_set_search(this.cmd.w);
				frontend_search(+1);
			}
			this.input_mode = MODE_DEFAULT;
			break;

		default:
			prompt_insert(key);
			break;
	}
	this.cmd.cursor = MIN(MAX(this.cmd.cursor, 0),this.cmd.w_len);
	queue_redraw(ELEMENT_PROMPT);
}


/*
 ____                            _
|  _ \ _ __ ___  _ __ ___  _ __ | |_
| |_) | '__/ _ \| '_ ` _ \| '_ \| __|
|  __/| | | (_) | | | | | | |_) | |_
|_|   |_|  \___/|_| |_| |_| .__/ \__|
                          |_|
*/


void prompt_set_prefix(const char *str)
{
	mbstowcs(this.cmd.prefix, str, LENGTH(this.cmd.prefix)-1);
}


void prompt_set_prefix_w(wchar_t *str)
{
	wcsncpy(this.cmd.prefix, str, LENGTH(this.cmd.prefix)-1);
}


void prompt_clear_c(void)
{
	this.cmd.prefix_color = SCUEP_INFO;
	memset(this.cmd.c, 0, sizeof(this.cmd.c));
	this.cmd.c_len = 0;
}


void prompt_clear(void)
{
	prompt_clear_c();
	memset(this.cmd.w, 0, sizeof(this.cmd.w));
	this.cmd.w_len = 0;
	this.cmd.cursor = 0;
	queue_redraw(ELEMENT_PROMPT);
}


void frontend_print(uint32_t color, const char *msg)
{
	prompt_clear();
	prompt_set_prefix(msg);
	this.cmd.prefix_color = color;
}

void frontend_printf(uint32_t color, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	static char buffer[1024];

	vsnprintf(buffer, sizeof(buffer)-1, format, args);
	va_end(args);

	prompt_clear();
	prompt_set_prefix(buffer);
	this.cmd.prefix_color = color;
}


void prompt_delete(int32_t pos)
{
	if (pos < 0) return;
	if (pos >= this.cmd.w_len) return;

	for (int i = pos; i < this.cmd.w_len; i++) {
		this.cmd.w[i] = this.cmd.w[i+1];
	}
	this.cmd.w_len--;
}


void prompt_insert(char c)
{
	this.cmd.c[this.cmd.c_len++] = c;

	wchar_t w[1024];
	size_t ret = mbstowcs(w, this.cmd.c, LENGTH(w)-1);

	if (ret == -1) {
		return;
	}

	this.cmd.w_len += ret;
	for (int32_t i = this.cmd.w_len+ret; i > this.cmd.cursor; i--) {
		this.cmd.w[i] = this.cmd.w[i-ret];
	}

	for (size_t i = 0; i < ret; i++){
		this.cmd.w[this.cmd.cursor++] = w[i];
	}

	prompt_clear_c();
}


/*
 ____                      _
/ ___|  ___  __ _ _ __ ___| |__
\___ \ / _ \/ _` | '__/ __| '_ \
 ___) |  __/ (_| | | | (__| | | |
|____/ \___|\__,_|_|  \___|_| |_|
*/


void frontend_set_search(wchar_t *str)
{
	wcsncpy (this.search.w, str,           LENGTH(this.search.w)-1);
	wcstombs(this.search.c, this.search.w, LENGTH(this.search.c)-1);
}


void frontend_search(int dir)
{
	char *needle = this.search.c;
	if (needle[0] == '\0') return;

	for (uint32_t j = 1; j < this.playlist_items; j++) {
		uint32_t index = this.cursor + dir * j;
		index = (index+this.playlist_items) % this.playlist_items;

		TrackId trackid = playlist_track(index);
		struct ScuepTrack *track = track_load(trackid);

		int match = (
			strcasestr(track->title,  needle) ||
			strcasestr(track->album,  needle) ||
			strcasestr(track->artist, needle)
		);
		track_free(track);

		if (match) {
			this.cursor_locked = 0;
			this.cursor = index;
			queue_redraw(ELEMENT_CAROUSEL);
			return;
		}
	}
}


void frontend_mark_by_search(const char *needle)
{
	if (needle[0] == '\0') return;

	for (uint32_t i = 0; i < this.playlist_items; i++) {

		TrackId trackid = playlist_track(i);
		struct ScuepTrack *track = track_load(trackid);
		int match = (
			strcasestr(track->title,  needle) ||
			strcasestr(track->album,  needle) ||
			strcasestr(track->artist, needle)
		);
		track_free(track);

		if (match) {
			playlist_set_mark(i, 1);
		}
	}

	queue_redraw(ELEMENT_CAROUSEL);
}


/*
 ____  _             _                _           _        _
|  _ \| | __ _ _   _| |__   __ _  ___| | __   ___| |_ _ __| |
| |_) | |/ _` | | | | '_ \ / _` |/ __| |/ /  / __| __| '__| |
|  __/| | (_| | |_| | |_) | (_| | (__|   <  | (__| |_| |  | |
|_|   |_|\__,_|\__, |_.__/ \__,_|\___|_|\_\  \___|\__|_|  |_|
               |___/
*/


void frontend_play(int id)
{
	player_load( playlist_track(id), id, false );
	player_play();

	if (this.cursor_locked) {
		this.cursor = id;
	}
}

void frontend_next(int32_t num)
{
	int32_t item = player_state_key() + num;
	item = (this.playlist_items + item) % this.playlist_items;
	frontend_play(item);
}


/*
  ____
 / ___|   _ _ __ ___  ___  _ __
| |  | | | | '__/ __|/ _ \| '__|
| |__| |_| | |  \__ \ (_) | |
 \____\__,_|_|  |___/\___/|_|
*/


void cursor_lock(void)
{
	int32_t key = player_state_key();
	if (key < 0) return;
	this.cursor_locked = true;
	this.cursor = key;
}


void cursor_free(void)
{
	this.cursor_locked = false;
}


/*
 ____                _
|  _ \ ___ _ __   __| | ___ _ __
| |_) / _ \ '_ \ / _` |/ _ \ '__|
|  _ <  __/ | | | (_| |  __/ |
|_| \_\___|_| |_|\__,_|\___|_|
*/


void queue_redraw(int elem)
{
	elements_dirty |= elem;
}


void layout_update(void)
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


#define CAROUSEL_PRINT_ALIGN_RIGHT (1<<0)
#define CAROUSEL_PRINT_FOCUSED     (1<<1)
void carousel_text(int row, int col, int w, wchar_t *wctext, int flags)
{
	static wchar_t wccut[1024] = {0};
	uint32_t wccut_len = 0;
	uint32_t wcw; /* Visual width of wccut */

	int cut = scuep_wcsnvslice(wccut, wctext, w-2, LENGTH(wccut), &wcw);
	wccut_len = wcslen(wccut);

	if (flags & CAROUSEL_PRINT_ALIGN_RIGHT) {
		uint32_t total = wcw + (w-wcw)*cut;
		col -= total;
	}

	if (flags & CAROUSEL_PRINT_FOCUSED) {
		attron(COLOR_PAIR(1));
	}
	mvprintw(row, col, "%S", wccut);
	attroff(COLOR_PAIR(1));

	/*
	 * Search highlighting
	 */

	bool hl_cut = false;

	wchar_t *substring  = NULL;
	wchar_t *needle     = this.search.w;
	uint32_t needle_len = wcslen(needle);

	if (this.input_mode == MODE_SEARCH && this.cmd.w_len > 0) {
		needle       = this.cmd.w;
		needle_len   = this.cmd.w_len;
	}

	/* Highlight :m/ command */
	if (this.input_mode == MODE_COMMAND
	&& wcsncmp(L"m/", this.cmd.w, 2) == 0
	){
		needle = this.cmd.w+2;
		needle_len = wcslen(needle);
	}

	if (needle_len) {
		wchar_t *haystack   = wctext;
		wchar_t  hl[1024];
		uint32_t index = 0;

		attron(COLOR_PAIR(6));
		while ((substring = scuep_wcscasestr(haystack, needle))) {
			index = substring - wctext;
			if (index >= wccut_len) {
				hl_cut = true;
				break;
			}

			uint32_t hl_len = MIN(needle_len, LENGTH(hl)-1);
			wcsncpy(hl, wccut+index, hl_len);
			hl[hl_len] = '\0';

			/* Calculate actual position of needle */
			uint32_t hx = col;
			for (uint32_t i = 0; i < index; i++) {
				hx += wcwidth(wctext[i]);
			}

			mvprintw(row, hx, "%S", hl);

			haystack += needle_len;
		}
		attroff(COLOR_PAIR(6));
		if (haystack-wctext >= wccut_len) hl_cut = true;
	}

	/* Print "..." for truncated text */
	if (cut) {
		if (hl_cut) attron(COLOR_PAIR(6));
		mvprintw( row, col+wcw, "%.*s", w-wcw, ".....");
		if (hl_cut) attroff(COLOR_PAIR(6));
	}
}


void draw_carousel(void)
{
	mvprintw(1, layout.pad[0], "Playlist: %i / %i", this.cursor+1, this.playlist_items);
	clrtoeol();
	mvprintw(1, term_cols-layout.pad[0] - 11, "scuep-ffsql" );

	static wchar_t wctext[1024] = {0};

	int items = playlist_count();
	int center = MIN((term_rows-1)/2, this.cursor+layout.carousel[0]);
	int row = 0;

	for (int i = this.cursor-center; i < items; i++) {
		int flags = 0;

		row = i - this.cursor + center;

		if (row <  layout.carousel[0]) continue;
		if (row >= layout.carousel[1]) break;

		TrackId trackid = playlist_track(i);
		int mark = playlist_get_mark(i);

		struct ScuepTrack *track = track_load(trackid);

		move(row, 0);
		clrtoeol();

		if (mark > 0) {
			mvprintw(row, 2, "*");
		}

		if (i == this.cursor) {
			mvprintw( row, 1, "~" );
			flags |= CAROUSEL_PRINT_FOCUSED;
		}

		if (i == player_state_key()) {
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
			carousel_text(row, term_cols - r, album_min, wctext,
				flags | CAROUSEL_PRINT_ALIGN_RIGHT
			);

			w -= album_min;
			r += album_min;
		}

		if (w-title_min > artist_min) {
			mbstowcs(wctext, track->artist, LENGTH(wctext)-1);
			r += artist_min;
			carousel_text(row, term_cols-r, artist_min, wctext, flags);
			w -= artist_min;
			w -= 2; /* Leave a gap between title and artist */
		}

		mbstowcs(wctext, track->title, LENGTH(wctext)-1);
		carousel_text(row, l, w, wctext, flags);

		track_free(track);
	}

	while (row < layout.carousel[1]) {
		move(++row, 0);
		clrtoeol();
	}
}


void draw_debug(void)
{
	struct PlayerState *player = _get_playerstate();

	for (size_t i = 0; i < term_cols; i++) {
		mvprintw(layout.debug,i, "-" );
	}
	for (size_t i = layout.debug+1; i < term_rows; i++) {
		move(i,0);
		clrtoeol();
	}

	mvprintw(layout.debug, term_cols/2-3, " Debug " );

	if (!player) {
		mvprintw(layout.debug+1,0, "%s", "Player uninitialized" );
	} else {
		mvprintw(layout.debug+1,0,
			"paused: %i"
			" done: %i"
			" decoder: %i"
			" sndsvr: %i"
			,player->pause
			,player->head.done
			,player->av.thread_run
			,!!player->sndsvr_close
		);
		mvprintw(layout.debug+2,0,
			"%.02f / %.02f"
			" head: %li tail: %li"
			" buffer: %li",
			player_position_seconds(),
			player_duration_seconds(),
			player->head.ring,
			player->tail.ring,
			player->head.total - player->tail.total
		);
		mvprintw(layout.debug+3,0,
			"Input mode: %i, cursor: %i",
			this.input_mode, this.cmd.cursor
		);
	}
}


void draw_progress(void)
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
	int32_t l = term_cols   - layout.pad[0];

	mvprintw(layout.progress, layout.pad[0], "%s", buf);

	if (l-r >= 20) {
		l-=11;
		mvprintw(layout.progress, l, "Volume 100%%");
		l-=2;
	}


	int32_t pos = floor( (l-r-1) * (fprogress/(float)duration) );
	pos += r;

	while (l-r > 0) {
		mvprintw(layout.progress, r, (pos == r) ?  "|" : "-");
		r++;
	}
};


void draw_prompt(void)
{
	move(layout.prompt, 0);
	clrtoeol();

	int pair = 0;

	switch (this.cmd.prefix_color) {
	case SCUEP_DEBUG:
		pair = 6;
		break;
	case SCUEP_WARN:
		pair = 7;
		break;
	case SCUEP_ERROR:
		pair = 8;
		break;
	}
	if (pair) attron(COLOR_PAIR(pair));
	mvprintw(layout.prompt, 0, "%S", this.cmd.prefix);
	if (pair) attroff(COLOR_PAIR(pair));

	for (int32_t i = 0; i <= this.cmd.w_len; i++) {
		wchar_t wc = this.cmd.w[i];
		if (wc == 0) wc = ' ';
		if (i == this.cmd.cursor
		&& (this.input_mode == MODE_COMMAND || this.input_mode == MODE_SEARCH)
		){
			attron(COLOR_PAIR(5));
		}
		printw("%C", wc);
		attroff(COLOR_PAIR(5));
	}
}

