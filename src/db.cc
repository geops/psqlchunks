#include <cstdlib>
#include <algorithm>
#include <sstream>

#include "debug.h"
#include "db.h"

#define CANCEL_BUF_SIZE     256

using namespace PsqlChunks;


Db::Db()
    : conn(NULL), do_commit(false), failed_count(0), in_transaction(false)
{
}


Db::~Db()
{
    disconnect();
}

bool
Db::connect( const char * host, const char * db_name,  const char * port, const char * user, const char * passwd)
{
    conn = PQsetdbLogin(host, port, NULL, NULL, db_name, user, passwd);
    return isConnected();
}


bool
Db::setEncoding(const char * enc_name)
{
    if (enc_name == NULL) {
        return false;
    }

    std::stringstream sqlstrm;
    sqlstrm << "set client_encoding to " << enc_name << ";";
    std::string sqlstr = sqlstrm.str();

    try {
        executeSql(sqlstr.c_str(), true);
    }
    catch (DbException &e) {
        return false;
    }

    return true;
}

void
Db::disconnect()
{
    finish();
    PQfinish(conn);
}


bool
Db::isConnected()
{
    if (PQstatus(conn) != CONNECTION_OK) {
        log_debug("no db connection");
        return false;
    }
    log_debug("got a working db connection");
    return true;
}


std::string
Db::getErrorMessage()
{
    std::string msg;
    if (conn) {
        msg.assign(PQerrorMessage(conn));
    }
    return msg;
}


bool
Db::runChunk(Chunk & chunk)
{
    bool success = true;

    if (!isConnected()) {
        DbException e("lost db connection");
        throw e;
    }

    begin();

    std::string sql = chunk.getSql();

    executeSql("savepoint chunk;");

    PGresult * pgres = PQexec(conn, sql.c_str());
    if (!pgres) {
        log_error("PQExec failed");
        DbException e("PQExec failed");
        throw e;
    }

    if ((PQresultStatus(pgres) == PGRES_FATAL_ERROR) ||
        (PQresultStatus(pgres) == PGRES_NONFATAL_ERROR)) {
        success = false;

        // collect diagonstics
        chunk.diagnostics = new Diagnostics();

        // error line and position in that line
        char * statement_position = PQresultErrorField(pgres, PG_DIAG_STATEMENT_POSITION);
        if (statement_position) {
            int pos = atoi(statement_position);
            if ((sql.begin()+pos) < sql.end()) {
                chunk.diagnostics->error_line = chunk.start_line 
                                + std::count(sql.begin(), sql.begin()+pos, '\n');
            }
            else {
                log_error("PG_DIAG_STATEMENT_POSITION is beyond the length of sql string");
            }
        }
        else {
            log_debug("got an empty PG_DIAG_STATEMENT_POSITION");
            chunk.diagnostics->error_line = LINE_NUMBER_NOT_AVAILABLE;
        }

        char * sqlstate = PQresultErrorField(pgres, PG_DIAG_SQLSTATE);
        if (sqlstate) {
            chunk.diagnostics->sqlstate.assign(sqlstate);
        }

        char * msg_primary = PQresultErrorField(pgres, PG_DIAG_MESSAGE_PRIMARY);
        if (msg_primary) {
            chunk.diagnostics->msg_primary.assign(msg_primary);
        }

        char * msg_detail = PQresultErrorField(pgres, PG_DIAG_MESSAGE_DETAIL);
        if (msg_detail) {
            chunk.diagnostics->msg_detail.assign(msg_detail);
        }

        char * msg_hint = PQresultErrorField(pgres, PG_DIAG_MESSAGE_HINT);
        if (msg_hint) {
            chunk.diagnostics->msg_hint.assign(msg_hint);
        }
    }

    PQclear(pgres);

    if (!success) {
        executeSql("rollback to savepoint chunk;");
        failed_count++;
    }
    else {
        executeSql("release savepoint chunk;");
    }

    return success;
}


void
Db::finish()
{
    if (failed_count>0) {
        rollback();
    }
    else {
        commit();
    }
    failed_count = 0;
}


void
Db::executeSql(const char * sqlstr, bool silent)
{
    if (!isConnected()) {
        log_warn("can not run chunk - no db connection");
    }

    log_debug("executing sql: %s", sqlstr);

    PGresult * pgres = PQexec(conn, sqlstr);
    if (!pgres) {
        log_error("PQExec failed");
        DbException e("PQExec failed");
        throw e;
    }

    if ((PQresultStatus(pgres) == PGRES_FATAL_ERROR) ||
        (PQresultStatus(pgres) == PGRES_NONFATAL_ERROR)) {
        const char * error_primary = PQresultErrorField(pgres, PG_DIAG_MESSAGE_PRIMARY);

        std::stringstream msgstream;
        std::string msg;
        msgstream << "could not execute query \"" << sqlstr << "\": "<< error_primary;
        msg = msgstream.str();

        PQclear(pgres);

        if (!silent) {
            log_error("%s", msg.c_str());
        }
        DbException e(msg);
        throw e;
    }


    PQclear(pgres);
}


void
Db::begin()
{
    if (!in_transaction) {
        executeSql("begin;");
        in_transaction = true;
    }
}


void
Db::commit()
{
    if (!do_commit) {
        return rollback();
    }

    if (in_transaction) {
        executeSql("commit;");
        in_transaction = false;
    }
}


void
Db::rollback()
{
    if (in_transaction) {
        executeSql("rollback;");
        in_transaction = false;
    }
}


bool
Db::cancel(std::string & errmsg)
{
    if (!isConnected()) {
        log_debug("not connected - no query to cancel");
        return true; // nothing to cancel
    }

    PGcancel * cncl;
    cncl = PQgetCancel(conn);
    if (cncl == NULL) {
        log_error("could not get PGCancel pointer");
        return false;
    }
    log_debug("Query successfuly canceled");

    char cnclbuf[CANCEL_BUF_SIZE];
    bool success = true;
    if (PQcancel(cncl, &cnclbuf[0], CANCEL_BUF_SIZE) != 1) {
        success = false;
        log_debug("could not cancel running query: %s", cnclbuf);
        errmsg.assign(cnclbuf);
    }

    PQfreeCancel(cncl);
    return success;
}
