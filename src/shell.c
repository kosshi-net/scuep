#include "shell.h"
#include "util.h"
#include "player.h"
#include "frontend.h"


void shell_run_w(const wchar_t* cmd)
{
	static char buffer[1024];
	wcstombs(buffer, cmd, sizeof(buffer)-1);
	shell_run(buffer);
}

void shell_run(const char* cmd)
{
	/* TODO: This is placeholder command driver. Do proper command parsing. */

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
}

