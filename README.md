# SCUEP - Simple CUE Player
A tiny music player with CUE support for Linux.

Uses MPV for audio playback. This is a wrapper to give more easy to use control over playlists and better cue handling. Made to be modified and extended. Written in posix-compliant shell and some C.

Allows you to handle tracks in .cue sheets individually, like CMUS. This whole program only exists because CMUS [seems to be no longer maintained](https://github.com/cmus/cmus/issues/856) and .cue support [seems to be broken for Gentoo](https://github.com/cmus/cmus/issues/886).

Has no user interface. You are expected to generate and manage playlists yourself, with scripts and a text editor. See [Generating playlists](#generating-playlists).

## WORK IN PROGRESS!!
Still lacks many basic features, has plenty bugs, not tested at all and generally just not polished. But it works if you don't feed it bad data. Use at your own risk. And please improve me!


## Usage

Start the "server" with 
```
scuep /path/to/playlist
```
You can also use stdin
```
cat ~/playlist | shuf | scuep -
```
If no argument is given, player will resume from previous state.

Accepted playlist format is just absolute paths to files:
```
/home/bob/Music/somesong.wav
/home/bob/Music/another song.mp3
cue:///home/bob/Music/awesome album.cue/1
cue:///home/bob/Music/awesome album.cue/2
```

Playback can be controlled with 
```
scuep-remote <pause/next/prev>
```

Example of a cool way to extend this player:
```
scuep-remote addto ~/Music/favorites
```
Adds currently playing track to a playlist (if not already). Bind this to your keyboard and you have a favorite button!

See more with ``scuep-remote help``

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
- [mpv](https://github.com/mpv-player/mpv)
- [libcue](https://github.com/lipnitsk/libcue) 
  - Optional, if you can't be bothered to install it, you can disable it in config.h

Both should be in repositeries of most distributions.

## License
MIT
