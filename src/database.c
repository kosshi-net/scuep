#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <sqlite3.h>
#include "sql.h"

#include "database.h"
#include "uri.h"
#include "util.h"
#include "log.h"

static int db_check();
static int db_prepare();
static int db_stmt_finalize_all();


#define SCUEP_FORMAT_VERSION 1

static char    *path_database;
static sqlite3 *db;

sqlite3_stmt **stmt_list[256];
int	           stmt_list_count = 0;


/*
 * TODO
 * Error handling
 * Optimize multi-stmt queries to one (im dumb)
 */
	
	
/*  
 * Convenience function. Compiles SQL if not already, otherwise resets stmt.
 */
int prepare (sqlite3_stmt **stmt, const char *sql)
{
	if( *stmt != NULL ){
		sqlite3_reset(*stmt);
		return SQLITE_OK;
	}

	int rc = sqlite3_prepare_v2(db, sql, -1, stmt, 0);

	if(rc == SQLITE_OK) {
		stmt_list[stmt_list_count++] = stmt;
		scuep_logf("Prepared %s\n", sql);
		return rc;
	} 
	
	scuep_logf("During statement: %s", sql);
	scuep_logf("Prepare error: %s\n", sqlite3_errmsg(db));
	
	return rc;
}


/*
 * Do whatever it takes to get a functioning database.
 * Returns 1 if fails.
 * */
int db_initialize( char* _path_database )
{
	path_database = _path_database;

	int rc = sqlite3_open( path_database, &db );
	if (rc != SQLITE_OK) {
		scuep_logf("Cannot open %s\n", path_database);
		return 1;
	}

	if (db_check())
		return db_reset();
	
	if (db_prepare())
		return 1;

	return 0;
}

int db_terminate()
{
	db_stmt_finalize_all();
	sqlite3_close(db);

	return 0;
}

int transaction_begin(){
	sqlite3_exec(db, "BEGIN TRANSACTION", NULL, NULL, NULL);
	return 0;
}
int transaction_end(){
	sqlite3_exec(db, "END TRANSACTION", NULL, NULL, NULL);
	return 0;
}


int db_stmt_finalize_all(){

	for( int i = 0; i < stmt_list_count; i++ ){
		sqlite3_finalize( *stmt_list[i] );
		*stmt_list[i] = NULL;
	}

	scuep_logf("Finalized %i\n", stmt_list_count);
	stmt_list_count = 0;
	return 0;
}


// Check if database is valid and version is OK
int db_check()
{
	int ver = db_intvar_load("version");
	return ( ver != SCUEP_FORMAT_VERSION );
};

int db_reset()
{
	scuep_logf("Database reset\n");

	db_terminate();
	remove(path_database);

	int rc = sqlite3_open( path_database, &db );
	if (rc != SQLITE_OK) {
		return 1;
	}

	sql_schema_sql[sql_schema_sql_len-1]=0;

	char *errmsg = NULL;
	rc = sqlite3_exec( 
		db,
		(char*)sql_schema_sql,
		NULL,
		NULL,
		&errmsg
	);
	
	if(errmsg){
		fprintf(stderr, "Reset failure, %s\n", errmsg);
		sqlite3_free(errmsg);
		return 1;
	}
	
	if (db_prepare())
		return 1;

	db_intvar_store("version", SCUEP_FORMAT_VERSION);
	int read_ver = db_intvar_load("version");
	scuep_logf("DB Version %i\n", read_ver);
	return read_ver != SCUEP_FORMAT_VERSION;

}

int db_prepare()
{
	
/*
	if (sqlite3_prepare_v2(db, 
		"SELECT id FROM tracks WHERE uri=?1",
		-1, &stmt_track_id_by_url, 0
	) !=SQLITE_OK) goto prepare_error;
*/

	// TEST CODE HERE
	
	if(0) goto error;

	return 0;
	
	error:
	scuep_logf("Prepare error: %s\n", sqlite3_errmsg(db));
	return 1;

}



int db_intvar_load (const char *key)
{
	int rc;
	static sqlite3_stmt *stmt;
	rc = prepare( &stmt, 
		"SELECT value FROM variables WHERE key=?1;"
	);
	if (rc != SQLITE_OK) goto error;

	rc = sqlite3_bind_text(stmt, 1, key, -1, NULL);
	rc = sqlite3_step(stmt);                 
	if (rc != SQLITE_ROW) goto error;

	int val = sqlite3_column_int (stmt, 0);

	return val;

	error:
	fprintf(stderr, "Error: %s\n", sqlite3_errmsg(db));
	return -1;
}

int db_intvar_store (const char *key, int val)
{
	int rc;
	static sqlite3_stmt *stmt;
	rc = prepare( &stmt, 
		"REPLACE INTO variables (key, value) VALUES (?1, ?2);"
	);
	if(rc != SQLITE_OK) goto error;

	sqlite3_bind_text(stmt, 1, key, -1, NULL);
	sqlite3_bind_int( stmt, 2, val);
	
	if (sqlite3_step(stmt) != SQLITE_DONE) goto error;

	return 0;

	error:
	fprintf(stderr, "Store error: %s\n", sqlite3_errmsg(db));
	return -1;

}


int playlist_clear()
{
	int rc = sqlite3_exec( 
		db,
		"DELETE FROM playlist",
		NULL,
		NULL,
		NULL // TODO add errmsg
	);
	
	return (rc != SQLITE_OK );

}


int playlist_count(void)
{
	int rc;
	static sqlite3_stmt *stmt;
	prepare( &stmt, "SELECT COUNT(*) FROM playlist");
	
	rc = sqlite3_step(stmt);

	return sqlite3_column_int(stmt, 0);
}

TrackId playlist_track(int row)
{
	int rc;
	static sqlite3_stmt *stmt;
	prepare( &stmt, "SELECT track_id FROM playlist WHERE id=?1");

	sqlite3_bind_int( stmt, 1, row );
	
	rc = sqlite3_step(stmt);

	return sqlite3_column_int(stmt, 0);
}







int playlist_push( TrackId id )
{
	int rc;
	static sqlite3_stmt *stmt;
	rc = prepare( &stmt, "INSERT INTO playlist(track_id) VALUES (?1)");
	
	rc = sqlite3_bind_int( stmt, 1, id );

	rc = sqlite3_step(stmt);

	if (rc != SQLITE_DONE) goto error;
		
	return 0;
	error:
	return -1;

}




TrackId track_by_uri( const char* uri )
{
	int rc;
	static sqlite3_stmt *stmt;
	rc = prepare( &stmt, 
		"SELECT id FROM tracks WHERE uri=?1"
	);
	rc = sqlite3_bind_text(stmt, 1, uri, -1, NULL );

	if(rc != SQLITE_OK) goto error;

	rc=sqlite3_step(stmt);
	if(rc != SQLITE_ROW) goto error;

	return sqlite3_column_int(stmt, 0);

	error:
	// TODO: discriminate between actual database errors and just lack of row
	//fprintf(stderr, "Track ID fetch error: %s\n", sqlite3_errmsg(db));
	return -1;
}


int insert_ignore_select( 
	sqlite3_stmt *ins, 
	sqlite3_stmt *sel, 
	const char *val 
){
	int rc;
	rc=(sqlite3_bind_text(ins, 1, val, -1, NULL )
	||  sqlite3_bind_text(sel, 1, val, -1, NULL ));
	if (rc != SQLITE_OK) goto error;

	rc=sqlite3_step(ins);
	if (rc != SQLITE_DONE) goto error;

	rc=sqlite3_step(sel);
	if (rc != SQLITE_ROW) goto error;
	return sqlite3_column_int(sel, 0);

	error:
	fprintf(stderr, "insert_ignore_select error: %s\n", sqlite3_errmsg(db));
	return -1;
}

void *track_free( struct ScuepTrack*track )
{
	if(!track) return NULL;

	if (track->uri)      free(track->uri);
	if (track->path)     free(track->path);
	if (track->dirname)  free(track->dirname);
	if (track->basename) free(track->basename);

	if (track->title)    free(track->title);
	if (track->artist)   free(track->artist);
	if (track->album)    free(track->album);

	free(track);

	return NULL;
}

struct ScuepTrack *track_load ( int id )
{
	int rc=0;
	static sqlite3_stmt *stmt;
	static sqlite3_stmt *stmt_artist;
	static sqlite3_stmt *stmt_album;

	struct ScuepTrack *track = NULL;

	prepare( &stmt,
		"SELECT  "
		"uri, "
		"title, "
		"artist_id, "
		"album_id, "
		"pcm_start, " // 4
		"pcm_length, "
		"pcm_chapter, "
		"bitmask, "
		"basename "
		" FROM tracks WHERE id=?1;"
	);
	prepare( &stmt_artist, "SELECT (name) FROM artists WHERE id=(?1)" );
	prepare( &stmt_album,  "SELECT (name) FROM albums  WHERE id=(?1)" );

	rc=sqlite3_bind_int(stmt, 1, id);
	if (rc != SQLITE_OK) goto error;


	rc=sqlite3_step(stmt);
	if (rc != SQLITE_ROW) goto error;
	

	track = calloc(1,sizeof(struct ScuepTrack));

	const char *uri      = (const char*)sqlite3_column_text(stmt, 0);
	const char *title    = (const char*)sqlite3_column_text(stmt, 1);
	const char *basename = (const char*)sqlite3_column_text(stmt, 8);
/*
	track->uri   = calloc( sqlite3_column_bytes(stmt, 0)+1, 1 );
	track->title = calloc( sqlite3_column_bytes(stmt, 1)+1, 1 );

	strcpy(track->uri,   uri);
	strcpy(track->title, title);
*/

	track->uri      = scuep_strdup( uri );
	track->title    = scuep_strdup( title );
	track->basename = scuep_strdup( basename );

	track->start  = sqlite3_column_int(stmt, 4);
	track->length = sqlite3_column_int(stmt, 5);
	track->chapter= sqlite3_column_int(stmt, 6);
	track->mask   = sqlite3_column_int(stmt, 7);
	
	int artist_id = sqlite3_column_int( stmt, 2 );
	int album_id  = sqlite3_column_int( stmt, 3 );

	sqlite3_bind_int(stmt_artist, 1, artist_id);
	sqlite3_bind_int(stmt_album,  1, album_id);

	rc=sqlite3_step(stmt_artist);
	if (rc != SQLITE_ROW) goto error;
	rc=sqlite3_step(stmt_album);
	if (rc != SQLITE_ROW) goto error;
	
	const char *artist = (const char*)sqlite3_column_text( stmt_artist, 0 );
	const char *album  = (const char*)sqlite3_column_text( stmt_album,  0 );

	track->artist = calloc( sqlite3_column_bytes(stmt_artist, 0)+1, 1 );
	track->album  = calloc( sqlite3_column_bytes(stmt_album,  0)+1, 1 );
	
	strcpy(track->artist, artist);
	strcpy(track->album,  album);

	char *uripath = path_from_uri(track->uri);
	track->dirname  = scuep_dirname(uripath);
	
	track->path = scuep_strcat(scuep_strdup(track->dirname), track->basename );
	free(uripath);
	return track;

	error:
	fprintf(stderr, "Track load error: %s\n", sqlite3_errmsg(db));
	return NULL;
}


int track_store( struct ScuepTrack*track )
{
	static sqlite3_stmt *stmt_ins_track;

	static sqlite3_stmt *stmt_ins_artist;
	static sqlite3_stmt *stmt_sel_artist;
	static sqlite3_stmt *stmt_ins_album;
	static sqlite3_stmt *stmt_sel_album;

	sqlite3_stmt *stmt; 

	prepare( &stmt_ins_artist, "INSERT OR IGNORE INTO artists(name) VALUES (?1)");
	prepare( &stmt_sel_artist, "SELECT id FROM artists WHERE name=?1" );
	prepare( &stmt_ins_album,  "INSERT OR IGNORE INTO albums(name) VALUES (?1)");
	prepare( &stmt_sel_album,  "SELECT id FROM albums  WHERE name=?1" );
	
	prepare( &stmt_ins_track, (char*)sql_insert_track_sql );

	int artist_id = -1;
	int album_id  = -1;


	artist_id = insert_ignore_select( 
		stmt_ins_artist,
		stmt_sel_artist,
		track->  artist
	);
	album_id = insert_ignore_select( 
		stmt_ins_album,
		stmt_sel_album,
		track->  album
	);

	scuep_logf("track_store: %i %i\n", artist_id, album_id);

	int rc;
	int k = 1;

	scuep_logf("store uri %s\n", track->uri);

	stmt=stmt_ins_track;
	rc=(sqlite3_bind_text(stmt, k++, track->uri, -1, NULL)
	||	sqlite3_bind_text(stmt, k++, track->basename, -1, NULL)
	||	sqlite3_bind_text(stmt, k++, track->title, -1, NULL)
	||	sqlite3_bind_int( stmt, k++, artist_id )
	||	sqlite3_bind_int( stmt, k++, album_id )
	||	sqlite3_bind_int( stmt, k++, track->start )
	||	sqlite3_bind_int( stmt, k++, track->length )
	||	sqlite3_bind_int( stmt, k++, track->chapter )
	||	sqlite3_bind_int( stmt, k++, track->mask )
	);
	if(rc != SQLITE_OK) goto error;
	
	rc = sqlite3_step( stmt );
	if(rc != SQLITE_DONE) goto error;

	return 0;

	error:
	fprintf(stderr, "Store error: %s\n", sqlite3_errmsg(db));
	return -1;


}

