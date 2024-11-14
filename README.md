# SCUEP - Simple CUE Player
A simple terminal music player for GNU/Linux.

## WIP!! - Rewrite branch
This is a near full rewrite of scuep. Largest differences to legacy brach:
- Entierly custom multithreaded audio backend using libavcodec (ffmpeg)
- Heavy use of SQLite as a metadata cache
	- Import and storage is far more robust and much faster than before
	- Instant cold start
- Much nicer modular design

Unimplemented features
- Search
- Many legacy commands
- Selections
- Volume control

Other issues, bugs, & TODO
- FFmpeg decodes to native endianess while alsa driver always assumes little-endian
- Currently only alsa is supported, but adding native support for other sound servers is trivial (~150 line file)
	- All relevant sound servers can take alsa audio, so this isn't a big issue.
- Not tested on other \*nixes
- `wchar_t` must be UTF-32, behavior with UTF-16 is undefined (does ncursesw even handle UTF-16?)
- Decoder edge case bugs
	- Playback sometimes freezes at the end of tracks
	- Pausing  sometimes causes 100% cpu usage
	- Unwanted noise when skipping a paused track
- Segfault on missing cached files
- Player does warn about running multiple instances leading to weird behavior
- libcue has some minor issues, write your own cue sheet parser?
- Various UI improvements

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
| v              | Stop playback. Unitializes decoder and audio driver, minimizing idle resource usage |
| b              | Play next |
| :              | Enter command (see Commands)  |
| /              | Search  |
| n, N           | Find next or prev track matching search |
| Esc            | Cancel search/command, refocus on currently playing file |
| Left, Right    | Seek 5 seconds |
| d              | Toggle debug panel |

### Commands

These commands can be ran from the TUI by pressing `:`,
with `scuep-remote`, or by piping them into `.config/scuep/fifo`.

| Command           | Action |
| ---               | --- |
| `toggle` `pause`  | Toggles play/pause of playback |
| `next` `skip`     | Skip track |
| `prev`            | Previous track |
| `noh`             | Clears search highlighting  |

### Command line arguments
| Flag | Function |
| ---              | --- |
| --help           | Displays help |
| --version        | Displays version |
| -i, -            | Read playlist from stdin |
| --readonly, --ro | Read only mode (THIS MIGHT BE UNIMPLEMENTED) |
| --reset          | Reset database, clearing cache |
| --debug          | Enable logging to stderr. Usage: `scuep --debug 2>log.txt` |

## License
GPLv2

