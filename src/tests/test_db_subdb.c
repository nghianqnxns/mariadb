/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
#include "test.h"


#include <stdlib.h>
#include <sys/stat.h>
#include <stdio.h>

#include <unistd.h>
#include <db.h>

// ENVDIR is defined in the Makefile

int
test_main(int argc, char *const argv[]) {
    parse_args(argc, argv);
    DB_ENV * env = 0;
    DB *db;
    DB_TXN * const null_txn = 0;
    const char * const fname = "test.db";
    int r;

    r = system("rm -rf " ENVDIR);
    CKERR(r);

    r=toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO); assert(r==0);

    r=db_env_create(&env, 0);   assert(r==0);
    // Note: without DB_INIT_MPOOL the BDB library will fail on db->open().
    r=env->open(env, ENVDIR, DB_INIT_MPOOL|DB_PRIVATE|DB_CREATE|DB_INIT_LOG|DB_INIT_TXN, S_IRWXU+S_IRWXG+S_IRWXO); assert(r==0);

    r = db_create(&db, env, 0);
    CKERR(r);

    r = db->open(db, null_txn, fname, "main", DB_BTREE, DB_CREATE, 0666);
    CKERR(r);
    
    r = db->close(db, 0);
    CKERR(r);

#if 0    
    const char * const fname2 = "test2.db";
    // This sequence segfaults in BDB 4.3.29
    // See what happens if we open a database with a subdb, when the file has only the main db.
    r = db->open(db, null_txn, fname2, 0, DB_BTREE, DB_CREATE, 0666);
    CKERR(r);
    r = db->close(db,0);
    CKERR(r);
    r = db->open(db, null_txn, fname2, "main", DB_BTREE, 0, 0666);
    CKERR(r);
    r = db->close(db, 0);
    CKERR(r);
#endif

    r = env->close(env, 0);
    CKERR(r);

    return 0;
}
