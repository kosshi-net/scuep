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
- In-player commands
- Remote control
- Selections

Other issues
- FFmpeg decodes to native endianess while alsa driver always assumes little-endian
- Currently only alsa is supported, but adding native support for other sound servers is trivial (~150 line file)
	- All relevant sound servers can take alsa audio, so this isn't even a big issue. 
- Not tested on other \*nixes
- wchar_t must be a non-multibyte encoding of unicode (eg UTF-32)
- Decoder edge case bugs

## License
GPLv2

