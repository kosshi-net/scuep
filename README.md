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
- Improve looks
- Volume controls
- Test general metadata parsing
- Improve extensibility
- Add some basic missing vim/sxiv controls
- Metadata caching (Loading metadata of 3188 mp3 files from a 7200rpm consumer hard drive took 55 seconds after a cold boot)

Goals:
- A player that can read metadata and play most filetypes, and not much else.
- Simple shell-friendly API to allow you to implement missing features yourself.


# Another music player? Why?
This player was originally made pretty much to play CUE+TTA only because CMUS
CUE support 
[seems to be broken for Gentoo](https://github.com/cmus/cmus/issues/886).
I didn't exactly like how CMUS behaved in the first place, and I didn't feel 
like hunting for a new player that matches my needs for a simple player with 
support for obscure formats, with easy extensibility.


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
Using ``--nosave`` will not save state (new playlist or playing track).
You should still not run multiple instances of it, all will attempt to listen
the same fifo file for remote control.

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

### General controls
| Key            | Action |
| ---            | --- |
| 0-9            | Repeat following command where reasonable |
| j, k, Down, Up | Navigate playlist |
| Enter          | Play highlighted item |
| z              | Play previous |
| c              | Toggle play/pause |
| b              | Play next |
| m              | Mark/unmark selected item | 
| /              | Enter search |
| n              | Find next item matching the search term |
| :              | Enter command (see Commands)  |
| Esc            | Cancel search/command, refocus on currently playing file* |
| Left, Right    | Seek 5 seconds |
| l              | Clear out the display, fixes corruption

* Esc currently responds only after a second

### Commands
`:q` Quit. CTRL+C works too currently.

`:m/searchterm` Marks all items that contain the search term. 
WIP Note: The searching is basic case sensitive substring search currently.

`:!command` Enter a shell command, where % is a path or url of an item.
The command is run for every single marked item, one by one. If there is no
marked items, the command is run once for currently selected item.
Example usage:

`:!echo '%' >> ~/new_playlist;`


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

All should be in repositeries of most distributions.

## License
GPLv2
