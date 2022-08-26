# SCUEP - Simple CUE Player
A simple terminal music player for GNU/Linux.

## WIP!! - Rewrite branch
This is a near full rewrite of scuep. Largest differences to legacy brach:
- Eniterly custom multithreaded audio backend using libavcodec (ffmpeg)
- Heavy use of SQLite as a metadata cache 
- Much nicer modular design

## What works
- Import and storage is far more robust than in master branch
- Metadata parsing and caching, with excellent performance
- Decoding and playing arbitrary formats 
- Multithreading
- Limited user interface

## What doesn't 
- Search
- In-player commands
- Remote control
- Autoplay

## Other Issues (TODO)
- FFmpeg decodes to native endianess while alsa driver always assumes little-endian
- Currently only alsa is supported, but adding native support for other sound servers is trivial (~150 line file)
- Not tested on other \*nixes
- wchar_t must be a non-multibyte encoding of unicode (eg UTF-32)

## License
GPLv2

