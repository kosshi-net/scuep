# WIP!! This is a rewrite branch !!

# SCUEP - Simple CUE Player
A simple playlist oriented terminal music player.

Scuep is a minimal but powerful playlist-oriented music player, designed to be
as intuitive to use as possible. Fundamentally it operates much like sxiv; you
give it a list of files (a playlist!), and scuep will just play them for you,
while also offering features to filter and edit said playlist.

Originally the purpose of it was just to play CUE+TTA properly, thus the name.
But it has grown to be much more general and powerful than that.

## Features
Note: Some listed features may be incomplete or buggy

- Supports most filetypes (anything ffmpeg supports)
- CUE sheet support
- Gapless playback
- Instant startup (SQLite metadata cache)
- Sxiv-like marking/selecting system
- Vi-like commands
- Vi-like search
- Playlist editing and deduplication features
- Unicode support

Unimplemented features
- Many legacy commands
- Volume control
- Some playlist editing functions
    - Reordering
	- In-player deduplication command
- Configuration

Other issues, bugs, & TODO
- FFmpeg decodes to native endianess while alsa driver always assumes little-endian
- Currently only alsa is supported, but adding native support for other sound servers is trivial (~150 line file)
	- All relevant sound servers can take alsa audio, so this isn't a big issue.
- `wchar_t` must be UTF-32, behavior with UTF-16 is undefined (does ncursesw even handle UTF-16?)
- Decoder edge case bugs
	- Seeking while paused does not work correctly
- Player does warn about running multiple instances leading to weird behavior
- libcue has some minor issues, write your own cue sheet parser?
- Undefined behavior when gaplessly playing tracks shorter than the ring buffer
    - Or when stacking track preloads in general
- Various UI improvements needed
    - Cursor behavior is erratic and laggy since gapless playback was implemented
	- Cursor position is wrong when deletions occur (fix: use id instead of ordinal)
- A lot more testing needed
    - Not tested on other \*nixes
- More precise searching options (eg. album name only search, etc)
- Manuals
- Better shell system with tab completition
- Mysterious bug pins a cpu core to 100% when a track has been paused for a long time

## Documentation
TODO. See legacy branch for more information.

### General controls

| Control        | Action |
| ---            | --- |
| 0-9            | Repeat following command where reasonable |
| j, k, Down, Up | Navigate playlist |
| Enter          | Play selected item |
| z              | Play previous |
| c              | Toggle play/pause |
| v              | Stop playback. Unitializes decoder and audio driver, minimizes idle resource usage |
| b              | Play next |
| :              | Enter command (see Commands)  |
| /              | Search |
| n, N           | Find next or prev track matching search |
| m              | Mark or unmark track |
| D              | Delete marked tracks from playlist |
| Esc            | Cancel search/command, refocus on currently playing file |
| Left, Right    | Seek 5 seconds |
| d              | (DEBUG) Toggle debug panel |
| L              | (DEBUG) Force preload a track for gapless playback |

### Commands

These commands can be ran from the TUI by pressing `:`,
with `scuep-remote`, or by piping them into `~/.config/scuep/fifo`.

| Command              | Action |
| ---                  | --- |
| `toggle` `pause`     | Toggles play/pause of playback |
| `next` `skip`        | Skip track |
| `prev`               | Previous track |
| `noh`                | Clears search highlighting  |
| `m/<term>`           | Mark by search |
| `append <file>`, `a` | Write marked items to a file |
| `dedup`              | Mark all duplicate tracks in the playlist |
| `push`               | Stashes away current marks to a stack |
| `pop`                | Retrieves marks from the stack, or clears current marks |
| `delete`             | Deletes marked tracks from playlist |

Path arguments parse environment variables and the other usual shell shortcuts, such as `~`.

### Command line arguments
| Flag | Function |
| ---              | --- |
| --help           | Displays help |
| --version        | Displays version |
| -i, -            | Read playlist from stdin |
| --readonly, --ro | Read only mode (THIS MIGHT BE UNIMPLEMENTED) |
| --reset          | Reset database, clearing cache |
| --debug          | Enable logging to stderr. Usage: `scuep --debug 2>log.txt` |

