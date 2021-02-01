#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include <sqlite3.h>
#include "sql.h"

#include "database.h"

static int db_check();
static int db_prepare();


#define SCUEP_FORMAT_VERSION 1

static char    *path_database;
static sqlite3 *db;

sqlite3_stmt **stmt_list[256];
int	           stmt_list_count = 0;


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
		printf("Prepared %s\n", sql);
		return rc;
	} 
	
	printf("During statement: %s", sql);
	printf("Prepare error: %s\n", sqlite3_errmsg(db));
	
	return rc;
}


/*
 * Do whatever it takes to get a functioning database.
 * Returns 1 if fails.
 * */
int db_init( char* _path_database )
{
	path_database = _path_database;

	int rc = sqlite3_open( path_database, &db );
	if (rc != SQLITE_OK) {
		printf("Cannot open %s\n", path_database);
		return 1;
	}

	if (db_check())
		return db_reset();
	
	if (db_prepare())
		return 1;

	return 0;
}

int db_finalize(){

	for( int i = 0; i < stmt_list_count; i++ ){
		sqlite3_finalize( *stmt_list[i] );
		*stmt_list[i] = NULL;
	}

	printf("Finalized %i\n", stmt_list_count);
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
	printf("Database reset\n");

	db_finalize();
	sqlite3_close(db);
	remove(path_database);

	int rc = sqlite3_open( path_database, &db );
	if (rc != SQLITE_OK) {
		return 1;
	}

	sql_reset_sql[sql_reset_sql_len-1]=0;

	char *errmsg = NULL;
	rc = sqlite3_exec( 
		db,
		(char*)sql_reset_sql,
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
	printf("DB Version %i\n", read_ver);
	return read_ver != SCUEP_FORMAT_VERSION;

}

int db_prepare()
{
	
/*
	if (sqlite3_prepare_v2(db, 
		"SELECT id FROM tracks WHERE url=?1",
		-1, &stmt_track_id_by_url, 0
	) !=SQLITE_OK) goto prepare_error;
*/

	// TEST CODE HERE

	return 0;
	
	prepare_error:
	printf("Prepare error: %s\n", sqlite3_errmsg(db));
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

int playlist_push( int id )
{
	// TODO
	return 0;
}


int track_id_by_url( char* url )
{
	int rc;
	static sqlite3_stmt *stmt;
	rc = prepare( &stmt, 
		"SELECT id FROM tracks WHERE url=?1"
	);
	if(rc != SQLITE_OK) goto error;
	// TODO
	return 0;
	error:
	return 1;
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

//struct ScuepTrackUTF8 *track_load ( int id );
int track_store( struct ScuepTrackUTF8 *track )
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

	printf("track_store: %i %i\n", artist_id, album_id);

	int rc;
	int k = 1;

	printf("store url %s\n", track->url);

	stmt=stmt_ins_track;
	// Neat but assumes SQLITE_OK is always zero, is that ok?
	rc=(sqlite3_bind_text(stmt, k++, track->url, -1, NULL)
	||	sqlite3_bind_text(stmt, k++, track->title, -1, NULL)
	||	sqlite3_bind_int( stmt, k++, artist_id )
	||	sqlite3_bind_int( stmt, k++, album_id )
	||	sqlite3_bind_int( stmt, k++, track->start )
	||	sqlite3_bind_int( stmt, k++, track->length )
	||	sqlite3_bind_int( stmt, k++, track->chapter )
	||	sqlite3_bind_int( stmt, k++, track->mask )
	);
	if(rc != SQLITE_OK) goto error;
	printf("Binds OK\n");
	
	rc = sqlite3_step( stmt );
	if(rc != SQLITE_DONE) goto error;

	return 0;

	error:
	fprintf(stderr, "Store error: %s\n", sqlite3_errmsg(db));
	return -1;


}

