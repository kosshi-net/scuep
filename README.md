# SCUEP - Simple CUE Player
A simple terminal music player for GNU/Linux.

## Still very much WORK IN PROGRESS!

Features:
- Supports playback for basically every file type imaginable (libmpv)
- Terminal interface with ncurses
- Metadata parsing for common filetypes (libtag)
- CUE parsing and metadata
- Unicode aware
- Simple fifo api for remote control
- sxiv style file marking
- vim style controls and search

Todo:
- Add an image here
- Fix bugs and clean up the code
- Error handling (eg. missing files just segfault)
- Volume controls
- Test general metadata parsing
- Improve looks
- Improve extensibility
- Improve search (case insensitive, make it search tags and comments etc)
- Improve and add some basic missing vim/sxiv controls 
- Improve everything
- Multimedia key integration for some desktop enviroments (you have to bind scuep-remote yourself currently)
- Metadata caching (Loading metadata of 3188 mp3 files from a 7200rpm consumer hard drive took 55 seconds after a cold boot)
- Tools or controls to allow reordering playlists efficiently
- Man pages

Goals:
- A player that can read metadata and play most filetypes, and not much else
- Simple but powerful controls to allow you to sort your music efficiently 
- Simple shell-friendly APIs to allow you to implement missing features yourself

# Another music player? Why?
This player was originally made pretty much to play CUE+TTA only because CMUS
CUE support 
[seems to be broken for Gentoo](https://github.com/cmus/cmus/issues/886).
I didn't exactly like how CMUS behaved in the first place, and didn't feel 
like hunting for a minimal but powerful player that still supports the obscure 
formats I need.


## Usage
The player can only play playlists of files, and those playlists cannot be
modified much at runtime. To load a new playlist, shuffle, or anything else, 
the program must be restarted with a new playlist. 

The idea is to let you manage your music library and its organization yourself,
with existing tools, with as little player specific stuff as possible.

### Starting
```
scuep /path/to/playlist
```
You can also use stdin
```
shuf ~/playlist | scuep -
```
If no new playlist is given, player will resume from previous state.

Accepted playlist format is just absolute paths to files with the exception of CUE:
```
/home/bob/Music/somesong.wav
/home/bob/Music/another song.mp3
cue:///home/bob/Music/awesome album.cue/1
cue:///home/bob/Music/awesome album.cue/2
```
Where CUE URLs are:
```
cue://<path to file>/<track number starting from 1>
```
### Flags

`--nosave` Don't save state (new playlist or playing track). 
You should still not run multiple instances of it, all will attempt to listen
the same fifo file for remote control.

`-`, `-i` Read playlist from stdin


### General controls
| Command        | Action |
| ---            | --- |
| 0-9            | Repeat following command where reasonable |
| j, k, Down, Up | Navigate playlist |
| Enter          | Play highlighted item |
| z              | Play previous |
| c              | Toggle play/pause |
| b              | Play next |
| m              | Toggle mark selected item |
| M              | Toggle marks on all items |
| dM             | Unmark all items |
| D              | Disable all marked items |
| /              | Enter search |
| n, N           | Find next or previous item * |
| :              | Enter command (see Commands)  |
| Esc            | Cancel search/command, refocus on currently playing file* |
| Left, Right    | Seek 5 seconds |
| l              | Clear out the display, fixes corruption |

* Either an item matching search, if no search active, find a marked item

#### Disabled items
Items are rendered in red and prefixed with a `#`. These items will be skipped
when playing. 

### Commands
`:q` Quit. CTRL+C works too currently.

`:m/searchterm` Marks all items that contain the search term. 
WIP Note: The searching is basic case sensitive substring search currently.

`:!command` Enter a shell command, where % is a path or url of an item.
The command is run for every single marked item, one by one. If there is no
marked items, the command is run once for currently selected item.
Example usage:

`:!echo '%' >> ~/new_playlist;`


### Usage examples
To play a eg. only a specific album in your current playlist, 
``:m/Album Name`` (Mark matching) , ``M`` (Reverse marks) , ``D`` (Disable marked)

Wip

### Remote

Playback can be controlled locally with
```
scuep-remote <pause/next/prev>
```

Communication is done with a FIFO in ~/.config/scuep/fifo

### Generating playlists
```
scuep-media-scanner /path/to/your/music >> ~/playlist;
scuep ~/playlist;
# Or just pipe it
scuep-cue-scanner /path/to/your/albums | scuep -
```
``scuep-cue-scanner`` is used for .cue files.

### Install
- Check dependencies
- Clone the repo and cd to it
- ``make`` 
- ``sudo make install`` (Installs to /usr/local/bin)

### Uninstall
- ``sudo make uninstall``

## Dependencies
- ncursesw
- [libmpv](https://github.com/mpv-player/mpv) 
- [libcue](https://github.com/lipnitsk/libcue) 
- [taglib](https://github.com/taglib/taglib)

All should be in repositeries of most distributions, if not already installed.

## License
GPLv2
