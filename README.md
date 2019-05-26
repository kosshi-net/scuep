# SCUEP - Simple CUE Player
A tiny music player with CUE support for Linux.

Uses MPV for the actual playback. This is just a wrapper to give more easy to use control over playback and playlists. Made to be modified and extended. Written mostly in posix-compliant shell and tiny amount of C.

Allows you to handle tracks in .cue sheets individually, like CMUS. This whole program only exists because CMUS [seems to be no longer maintained](https://github.com/cmus/cmus/issues/856) and .cue support [seems to be broken for Gentoo](https://github.com/cmus/cmus/issues/886).

Has no user interface. You are expected to generate and manage playlists yourself, with scripts and a text editor. See [Generating playlists](#generating-playlists).

## WORK IN PROGRESS!!
Still lacks many basic features, has plenty bugs, not tested at all and generally just not polished. But it works if you don't feed it bad data. DO NOT RUN AS SUDO! And please improve me!


## Usage

Start the "server" with ``./scuep [/path/to/playlist]``.
If no new playlist is supplied, it will resume from previous state.

Example of a playlist file:
```
/home/bob/Music/somesong.wav
/home/bob/Music/another song.mp3
cue:///home/bob/Music/awesome album.cue/1
cue:///home/bob/Music/awesome album.cue/2
```

Playback can be controlled with 
```./scuep-remote <pause/next/prev>```.

Example of a cool way to extend this player:
```./scuep-remote addto </path/to/playlist>```
Adds currently playing track to a playlist (if not already). Bind this to your keyboard and you have a favorite button!

See more with ``./scuep-remote help``

### Generating playlists
There are couple example scripts in ``opt/`` on generating playlists.
Example:
```
opt/scuep-media-scanner /path/to/your/music >> ~/playlist;
bin/scuep ~/playlist;
```
Use ``scuep-cue-scanner`` for .cue files.

## Install
- Check dependencies
- Clone the repo
- Put it somewhere
- Run ``make``

## Dependencies
- [mpv](https://github.com/mpv-player/mpv)
- [libcue](https://github.com/lipnitsk/libcue) 
  - Optional, if you can't be bothered to install it, you can disable it in config.h

Both should be in repositeries of most distributions.

## License
MIT
