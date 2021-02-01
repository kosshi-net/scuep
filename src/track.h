#ifndef SCUEP_TRACK_H 
#define SCUEP_TRACK_H


struct ScuepTrackUTF8 {
	const char *album;
	const char *artist;
	const char *title;
	const char *path;
	const char *url;

	int32_t start;
	int32_t length;
	int32_t chapter;
	int32_t mask;
};

#endif

