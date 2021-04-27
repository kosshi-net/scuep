# SCUEP - Simple CUE Player
A simple terminal music player for GNU/Linux.

## This branch is depricated
Complete rewrite and redesign is (slowly) happening in branch ffsql. Once it reaches feature-parity with Master, this will be replaced and a first release scheduled soon™ after.

The player in this branch is perfectly funcional even if little unpolished. However the codebase has outgrown itself.

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

Todo until first release:
- Image here!
- Deduplication tools
- Sane way to reorder playlists

Todo:
- Man pages
- Improve extensibility (eg a program to query metadata of current playing track)
- Improve and add some basic missing vim/sxiv controls 
- Improve looks
- Improve everything

Features in consideration:
- Metadata caching? Only useful with massive playlists (several thousand items) and slow drives
- Consider multimedia key integration for some desktop enviroments (you have to bind scuep-remote yourself currently)
- Key rebinding
- Support for other *nix systems
- Switch to a more minimal backend. 

Feel free to open up a issue for feature requests, even if its already listed 
here. 

Goals (and more todo):
- A player that can read metadata and play most filetypes, and not much else
- Simple but powerful controls to allow you to sort your music efficiently 
- Simple shell-friendly APIs to allow you to make the player do wahtever you wish 

### Performance
Scuep can load massive playlists, largest tested is 60 000 tracks. 55 
megabyte memory usage and player remained perfectly responsive. Loading such
massive playlists however can take a while, and there is no caching mechanisms 
in place currently.

# Another music player? Why?
This player was originally made pretty much to play only CUE+TTA because CMUS
CUE support 
[seems to be broken for Gentoo](https://github.com/cmus/cmus/issues/886).
I didn't exactly like how CMUS behaved in the first place, and didn't feel 
like hunting for a minimal but powerful player that still supports the obscure 
formats I need. 

If you already use MPV or other generic media player for playing your music, 
this  player is for you. It's not much more than a fancy MPV frontend and a 
playlist manager.

## Usage
The player can only play playlists of files, and those playlists cannot be
modified much at runtime. To load a new playlist, shuffle, or anything else, 
the program must be restarted with a new playlist. 

The player does not have a concept of a library. You are given the freedom to
manage your library yourself, with tools provided or already installed. No
obscure database formats, only dead-simple playlists.

### Playlists and startup
The player must be provided a playlist on first startup. 
```
scuep-media-scanner /path/to/your/music >> ~/new_playlist

scuep ~/new_playlist
```
Playlists can be piped to the program. 
```
shuf ~/new_playlist | scuep -
```
The provided playlist is saved to `~/.config/scuep/playlist`. If no new 
playlist is provided, the player will resume from previous state. 

Playlist format is just list of absolute paths to files with the exception of 
CUE:
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

#### CUE sheets
To scan for CUE sheets, use ``scuep-cue-scanner``.

Note, even though the player is designed for CUEs, usage of alternatives is 
preferred. Many CUE sheets you will find are just plain invalid, if the media 
is eg just regular flac files, use them instead.

### Flags
`--nosave` Don't save state (new playlist, playing track, marked itmes ..). 
You should still not run multiple instances of it, all will attempt to listen
the same fifo file for remote control.

`-`, `-i` Read playlist from stdin

`--debug` Enable logging with syslog, as an alternative to printf debugging ;)

### File marks
Marking files allows you to perform operations on a subset of the current 
playlist. See controls and commands below. 

Marks are saved to `~./config/scuep/marks` to allow the player to remember 
them over restarts.

### General controls
Please suggest improvements!

| Command        | Action |
| ---            | --- |
| 0-9            | Repeat following command where reasonable |
| j, k, Down, Up | Navigate playlist |
| Enter          | Play selected item |
| z              | Play previous |
| c              | Toggle play/pause |
| b              | Play next |
| +, =           | Volume up |
| -              | Volume down |
| m              | Toggle mark on selected item |
| M              | Toggle marks on all items |
| dM             | Unmark all items |
| D              | Disable all marked items |
| /              | Enter search |
| n, N           | Find next or previous item * |
| :              | Enter command (see Commands)  |
| Esc            | Cancel search/command, refocus on currently playing file |
| Left, Right    | Seek 5 seconds |
| l              | Clear out the display, fixes corruption |
| p              | Display properties |

* Either an item matching search, if no search active, find a marked item

#### Disabled items
Items are rendered in red and prefixed with a `#`. These items will be skipped
when playing. If you deselect all, the player will get really confused.

### Commands
`:q` Quit. CTRL+C works too currently.

`:m/searchterm` Marks all items that contain the search term. 
 Note: Performs a basic case insensitive substring search, no regex etc

`:volume <integer>` Set volume in range 0 - 100. Number can be prefixed with 
`+` or `-` for relative control. 

`:append <path>` Aliases: `a, addto` Append selected items to file. Does not check for duplicates. 

`:mfile <path>` Mark all items that appear in a playlist

`:!command` Enter a shell command, where % is a path or url of an item.
The command is run for every single marked item, one by one. If there is no
marked items, the command is run once for currently selected item.
Example usage:
`:!echo '%' >> ~/new_playlist;`

BUG: $ and ' are valid characters in filenames and 
can break single and double quotes. This is not handled properly currently.
Do not use this command unless you know what you're doing.

Playback control commands:

`:prev`
`:next`
`:pause`

### Usage examples
To play eg. only a specific album in your current playlist, 
``:m/Album Name`` (Mark matching) , ``M`` (Reverse marks) , ``D`` (Disable marked)

Wip

### Remote

Playback can be controlled externally with
```
scuep-remote <command>
```
You can run any command listed in Commands.
Examples:
`scuep-remote next`
`scuep-remote volume -5`

Communication is done with a FIFO in ~/.config/scuep/fifo

### Install
- Check dependencies
- Clone the repo and cd to it
- ``make`` 
- ``sudo make install`` (Installs to /usr/local/bin)

### Uninstall
- ``sudo make uninstall``

Use the same Makefile you used to install. There's been very little testing of 
distribution and Makefile itself may change.

## Dependencies
- ncursesw
- [libmpv](https://github.com/mpv-player/mpv) 
- [libcue](https://github.com/lipnitsk/libcue) 
- [taglib](https://github.com/taglib/taglib)

All should be in repositeries of most distributions, if not already installed.
Scripts in opt/ may have additional requirements.

## License
GPLv2

