#ifndef SCUEP_DATABASE_H
#define SCUEP_DATABASE_H

#include "track.h"

int db_init( char* );
int db_reset( void );

int db_intvar_load  (const char *key);
int db_intvar_store (const char *key, int val);


int track_id_by_url( char* url );

struct ScuepTrackUTF8 *track_load ( int id );
int                    track_store( struct ScuepTrackUTF8 *);


int playlist_clear(void);
int playlist_push( int id );


#endif
