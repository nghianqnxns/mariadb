/* -*- mode: C; c-basic-offset: 4 -*- */
#include <toku_portability.h>
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <memory.h>
#include <sys/stat.h>
#include <toku_portability.h>
#include <db.h>

#include "test.h"

static void
db_put (DB *db, int k, int v) {
    DB_TXN * const null_txn = 0;
    DBT key, val;
    int r = db->put(db, null_txn, dbt_init(&key, &k, sizeof k), dbt_init(&val, &v, sizeof v), DB_YESOVERWRITE);
    assert(r == 0);
}

static void
db_del (DB *db, int k) {
    DB_TXN * const null_txn = 0;
    DBT key;
    int r = db->del(db, null_txn, dbt_init(&key, &k, sizeof k), 0);
    assert(r == 0);
}

static void
expect_db_get (DB *db, int k, int v) {
    DB_TXN * const null_txn = 0;
    DBT key, val;
    int r = db->get(db, null_txn, dbt_init(&key, &k, sizeof k), dbt_init_malloc(&val), 0);
    assert(r == 0);
    int vv;
    assert(val.size == sizeof vv);
    memcpy(&vv, val.data, val.size);
    assert(vv == v);
    toku_free(val.data);
}

static void
expect_cursor_get (DBC *cursor, int k, int v) {
    DBT key, val;
    int r = cursor->c_get(cursor, dbt_init_malloc(&key), dbt_init_malloc(&val), DB_NEXT);
    assert(r == 0);
    assert(key.size == sizeof k);
    int kk;
    memcpy(&kk, key.data, key.size);
    assert(val.size == sizeof v);
    int vv;
    memcpy(&vv, val.data, val.size);
    if (kk != k || vv != v) printf("expect key %u got %u - %u %u\n", (uint32_t)ntohl(k), (uint32_t)ntohl(kk), (uint32_t)ntohl(v), (uint32_t)ntohl(vv));
    assert(kk == k);
    assert(vv == v);

    toku_free(key.data);
    toku_free(val.data);
}

static void
expect_cursor_get_both_range (DBC *cursor, int k, int v, int expectr) {
    DBT key, val;
    int r = cursor->c_get(cursor, dbt_init(&key, &k, sizeof k), dbt_init(&val, &v, sizeof v), DB_GET_BOTH_RANGE);
    assert(r == expectr);
}

static void
expect_cursor_get_current (DBC *cursor, int k, int v) {
    DBT key, val;
    int r = cursor->c_get(cursor, dbt_init_malloc(&key), dbt_init_malloc(&val), DB_CURRENT);
    assert(r == 0);
    int kk, vv;
    assert(key.size == sizeof kk); memcpy(&kk, key.data, key.size); assert(kk == k);
    assert(val.size == sizeof vv); memcpy(&vv, val.data, val.size); assert(vv == v);
    toku_free(key.data); toku_free(val.data);
}


/* insert, close, delete, insert, search */
static void
test_icdi_search (int n, int dup_mode) {
    if (verbose) printf("test_icdi_search:%d %d\n", n, dup_mode);

    DB_ENV * const null_env = 0;
    DB *db;
    DB_TXN * const null_txn = 0;
    const char * const fname = ENVDIR "/" "test_icdi_search.brt";
    int r;

    unlink(fname);

    /* create the dup database file */
    r = db_create(&db, null_env, 0); assert(r == 0);
    r = db->set_flags(db, dup_mode); assert(r == 0);
    r = db->set_pagesize(db, 4096); assert(r == 0);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, DB_CREATE, 0666); assert(r == 0);

    /* insert n duplicates */
    int i;
    for (i=0; i<n; i++) {
        int k = htonl(1+n/2);
        int v = htonl(1+i);
        db_put(db, k, v);

        expect_db_get(db, k, htonl(1));
    } 

    /* reopen the database to force nonleaf buffering */
    r = db->close(db, 0); assert(r == 0);
    r = db_create(&db, null_env, 0); assert(r == 0);
    r = db->set_flags(db, dup_mode); assert(r == 0);
    r = db->set_pagesize(db, 4096); assert(r == 0);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, 0, 0666); assert(r == 0);

    db_del(db, htonl(1+n/2));

    /* insert n duplicates */
    for (i=n-1; i>=0; i--) {
        int k = htonl(1+n/2);
        int v = htonl(1+n+i);
        db_put(db, k, v);

        DBC *cursor;
        r = db->cursor(db, null_txn, &cursor, 0); assert(r == 0);
        expect_cursor_get_both_range(cursor, k, v, 0);
        expect_cursor_get_current(cursor, k, v);
        r = cursor->c_close(cursor); assert(r == 0);
    } 

    DBC *cursor;
    r = db->cursor(db, null_txn, &cursor, 0); assert(r == 0);
    expect_cursor_get_both_range(cursor, 0, 0, DB_NOTFOUND);
    expect_cursor_get_both_range(cursor, 0, 0, DB_NOTFOUND);
    if (n>1)
	expect_cursor_get_both_range(cursor, htonl(1+n/2), 0, 0);
    expect_cursor_get_both_range(cursor, htonl(1+n), 0, DB_NOTFOUND);
    r = cursor->c_close(cursor); assert(r == 0);

    r = db->cursor(db, null_txn, &cursor, 0); assert(r == 0);
    for (i=0; i<n; i++) {
        expect_cursor_get(cursor, htonl(1+n/2), htonl(1+n+i));
    }
    r = cursor->c_close(cursor); assert(r == 0);

    r = db->close(db, 0); assert(r == 0);
}


int
test_main(int argc, const char *argv[]) {
    int i;

    parse_args(argc, argv);
  
    system("rm -rf " ENVDIR);
    toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);

    int limit = 1<<13;
    if (verbose > 1)
        limit = 1<<16;

    for (i=1; i <= limit; i *= 2) {
        test_icdi_search(i, DB_DUP + DB_DUPSORT);
    }
    return 0;
}
