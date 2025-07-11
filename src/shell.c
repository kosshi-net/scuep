#include <wordexp.h>

#include "database.h"
#include "shell.h"
#include "util.h"
#include "player.h"
#include "frontend.h"
#include "log.h"

void shell_run_w(const wchar_t* cmd)
{
	static char buffer[1024];
	wcstombs(buffer, cmd, sizeof(buffer)-1);
	shell_run(buffer);
}

char *parse_path(const char *arg)
{
	static wordexp_t p;
	int err = wordexp(arg, &p, 0);

	if (err) {
		frontend_printf(SCUEP_ERROR, "wordexp() error %i", err);
		return NULL;
	}

	if (p.we_wordc != 1) {
		frontend_print(SCUEP_ERROR, "wordexp() error, multiple matches");
		return NULL;
	}

	return p.we_wordv[0];
}

void shell_run(const char* cmd)
{
	/* TODO: This is a very crude placeholder command driver. Make a proper
	 * framework for this. */

	const char *arg = cmd;
	while (*arg && *arg!=' ') arg++;

	if (scuep_prefix("toggle", cmd)
	||  scuep_prefix("pause", cmd)
	){
		player_toggle();
		return;
	}

	if (scuep_prefix("skip", cmd)
	||  scuep_prefix("next", cmd)
	){
		frontend_next(1);
		return;
	}

	if (scuep_prefix("prev", cmd)
	){
		frontend_next(-1);
		return;
	}

	if (scuep_prefix("noh", cmd)
	){
		frontend_set_search(L"");
		return;
	}

	if (scuep_prefix("log_warn ", cmd)){
		frontend_print(SCUEP_WARN, cmd+9);
		return;
	}
	if (scuep_prefix("log_debug ", cmd)){
		frontend_print(SCUEP_DEBUG, cmd+10);
		return;
	}
	if (scuep_prefix("log_error ", cmd)){
		frontend_print(SCUEP_ERROR, cmd+10);
		return;
	}

	if (scuep_prefix("m/", cmd)) {
		frontend_mark_by_search(cmd+2);
		return;
	}

	if (scuep_prefix("debug.marks", cmd)) {
		int mark = playlist_get_mark(frontend_get_cursor());
		frontend_printf(SCUEP_DEBUG, "%x", mark);
		return;
	}

	if (scuep_prefix("debug.wordexp ", cmd)) {
		const char *path = parse_path(arg);
		if (!path)
			return;

		frontend_printf(SCUEP_DEBUG, "Parsed path: %s", path);
		return;
	}

	if (scuep_prefix("append ", cmd)
	||  scuep_prefix("a ", cmd)
	) {
		/* TODO Deduplication, don't write uris already in the file */

		int mark_bit = 1<<markstack_index();

		const char *path = parse_path(arg);
		if (!path) return;

		FILE *fp = fopen(path, "a");

		if (!fp) {
			frontend_print(SCUEP_ERROR, "File open error");
			return;
		}

		int tracks = playlist_count();

		int written_count = 0;

		/* TODO: This could be done more efficiently by querying sqlite for
		 * marked tracks directly */
		for (int i = 0; i < tracks; i++) {

			if (!(playlist_get_mark(i) & mark_bit))
				continue;

			TrackId id = playlist_track(i);
			struct ScuepTrack *track = track_load(id);

			fprintf(fp, "%s\n", track->uri);

			written_count++;
			track_free(track);
		}
		/* TODO if written_count=0, write frontend's hovered track */

		frontend_printf(SCUEP_INFO, "Wrote %i line%s to %s",
				written_count,
				written_count==1 ? "s" : "",
				path
			);

		fclose(fp);
		return;
	}

	if (scuep_prefix("push", cmd)) {
		markstack_push();
		return;
	}

	if (scuep_prefix("pop", cmd)) {
		markstack_pop();
		return;
	}

	/* TODO Commands to be added TODO
	 *
	 * mfile
	 *   Read marks from a file
	 *
	 * volume
	 *   Change playback volume
	 *
	 * Many more I forgot
	 */


	frontend_print(SCUEP_ERROR, "No such command");
}

