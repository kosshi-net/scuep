#ifndef SCUEP_DATABASE_H
#define SCUEP_DATABASE_H

#include <stdint.h>



typedef int TrackId;

struct ScuepTrack {
	char *uri; 

	char *path;

	char *basename; 
	char *dirname;

	char *title;
	char *artist;
	char *album;
	
	// In milliseconds
	int32_t start;
	int32_t length;

	int32_t chapter;
	int32_t mask;
};

/*
 * The only function that will ever return a struct ScuepTrack* is the load
 * function. Every other query/find etc returns a TrackId.
 */

struct ScuepTrack *track_load ( TrackId );

int track_store( struct ScuepTrack *);

// Always returns NULL
void *track_free(struct ScuepTrack *);


TrackId track_by_uri( const char* );



int  db_initialize( char* );
int  db_terminate();

int  db_reset( void );

int  db_intvar_load  (const char *key);
int  db_intvar_store (const char *key, int val);

int  transaction_begin(void);
int  transaction_end(void);

// Playlist is one indexed!!!

int     playlist_count(void);
int     playlist_clear(void);
int     playlist_push (TrackId);
TrackId playlist_track(int);



#endif
