# SCUEP - Simple CUE Player
A simple terminal music player for GNU/Linux.

## This is an experimental SQLite/FFmpeg development branch. 
This is a near full rewrite of SCUEP. It aims to replaces the slow rigid 
storage spaghetti with SQLite and playback will be implemented with FFmpeg. 

Both of those goals have been met as of 10/05/21.

## What works
- Import and storage is far more robust than in master branch
- Excellent cold start performance 
- Decoding arbitrary formats
- Basic multithreaded audio playback
- Playlist carousel

## What doesn't
- Interface is largely unimplemented

## (Non-)Portability notes
#### wchar_t
Couple functions currently assume 32bit wchar_t.
#### Endianess
FFmpeg decodes to native endianess while alsa driver always assumes 
little-endian. 
#### Other Nixes
Never tested
#### Sound server support
Currently only alsa is supported. 
Adding native support for other sound servers will be trivial, alsa driver 
fits in to a file 100 lines long.


## License
GPLv2

