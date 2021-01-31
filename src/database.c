#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include <sqlite3.h>
#include "sql.h"

#include "database.h"

static int database_check();
static int database_reset();
static int database_prepare();
int database_intvar_load  (const char *key);
int database_intvar_store (const char *key, int val);

#define SCUEP_FORMAT_VERSION 1

static char    *path_database;
static sqlite3 *db;

/*
 * Do whatever it takes to get a functioning database.
 * Returns 1 if fails.
 * */
int database_init( char* _path_database )
{
	path_database = _path_database;

	int rc = sqlite3_open( path_database, &db );
	if (rc != SQLITE_OK) {
		printf("Cannot open %s\n", path_database);
		return 1;
	}

	if (database_check())
		return database_reset();
	
	if (database_prepare())
		return 1;

	return 0;
}

sqlite3_stmt *intvar_load;
sqlite3_stmt *intvar_store;


// Check if database is valid and version is OK
int database_check()
{

	// Todo: FINALZIE!!
	
	sqlite3_stmt *res;

	int rc = sqlite3_prepare_v2(db, 
		"SELECT value FROM variables WHERE key=\"version\"", 
		-1, &res, 0
	);    
	
	if( rc != SQLITE_OK ){
		printf("Check error: %s\n", sqlite3_errmsg(db));
		return 1;
	}
	
	rc = sqlite3_step(res);
    
    if (rc == SQLITE_ROW) {
		int ver = sqlite3_column_int(res, 0);
		return ( ver != SCUEP_FORMAT_VERSION );
    };

	return 1;
};

int database_reset()
{
	printf("Database reset\n");
	sqlite3_close(db);
	remove(path_database);

	int rc = sqlite3_open( path_database, &db );
	if (rc != SQLITE_OK) {
		return 1;
	}

	char *errmsg = NULL;
	rc = sqlite3_exec( 
		db,
		sql_reset_sql,
		NULL,
		NULL,
		&errmsg
	);
	
	if(errmsg){
		fprintf(stderr, "Reset failure, %s\n", errmsg);
		sqlite3_free(errmsg);
		return 1;
	}
	
	if (database_prepare())
		return 1;

	database_intvar_store("version", SCUEP_FORMAT_VERSION);
	int read_ver = database_intvar_load("version");
	return read_ver != SCUEP_FORMAT_VERSION;

}

int database_prepare()
{
	int rc;
	
	rc = sqlite3_prepare_v2(db, 
		"SELECT value FROM variables value WHERE key=?1;", 
		-1, &intvar_load, 0
	);
	if (rc!=SQLITE_OK) goto prepare_error;
	rc = sqlite3_prepare_v2(db, 
		"REPLACE INTO variables (key, value) VALUES (?1, ?2);",
		-1, &intvar_store, 0
	);
	if (rc!=SQLITE_OK) goto prepare_error;

	return 0;
	
	prepare_error:
	printf("Prepare error: %s\n", sqlite3_errmsg(db));
	return 1;

}



int database_intvar_load (const char *key)
{
	int rc;
	sqlite3_reset(intvar_load);
	rc = sqlite3_bind_text(intvar_load, 1, key, -1, NULL);
	if(rc != SQLITE_OK) goto load_error;

	rc = sqlite3_step(intvar_load);
	if(rc != SQLITE_ROW) goto load_error;

	int val = sqlite3_column_int (intvar_load, 0);

	return val;

	load_error:
	fprintf(stderr, "Load error: %s\n", sqlite3_errmsg(db));
	return -1;
}

int database_intvar_store (const char *key, int val)
{
	int rc;
	sqlite3_reset(intvar_store);

	sqlite3_bind_text(intvar_store, 1, key, -1, NULL);
	sqlite3_bind_int( intvar_store, 2, val);
	
	rc = sqlite3_step( intvar_store );
	if(rc != SQLITE_DONE) goto store_error;

	return 0;

	store_error:
	fprintf(stderr, "Store error: %s\n", sqlite3_errmsg(db));
	return 1;

}


