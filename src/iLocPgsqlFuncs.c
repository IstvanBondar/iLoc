/*
 * Copyright (c) 2018, Istvan Bondar,
 * Written by Istvan Bondar, ibondar2014@gmail.com
 *
 * BSD Open Source License.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the <organization> nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifdef PGSQL

#include "iLoc.h"
extern int verbose;
extern FILE *logfp;
extern FILE *errfp;

extern PGconn *conn;

/*
 * Print SQL error message
 */
void PrintPgsqlError(char *message)
{
    fprintf(errfp, "ERROR: %s %s\n", message, PQerrorMessage(conn));
    fprintf(logfp, "ERROR: %s %s\n", message, PQerrorMessage(conn));
}

/*
 * Connect to Postgres DB
 */
int PgsqlConnect(char *conninfo)
{
    PGresult *res_set = (PGresult *)NULL;
    char sql[1536], errmsg[1536];
    conn = PQconnectdb(conninfo);
    if (PQstatus(conn) != CONNECTION_OK) {
        PrintPgsqlError(PQerrorMessage(conn));
        PgsqlDisconnect();
        return 1;
    }
    sprintf(sql, "SET enable_seqscan = off");
    if ((res_set = PQexec(conn, sql)) == NULL) {
        PrintPgsqlError("pgsql_funcs: set:");
    } else if (PQresultStatus(res_set) != PGRES_COMMAND_OK) {
        sprintf(errmsg, "PgsqlConnect: %d", PQresultStatus(res_set));
        PrintPgsqlError(errmsg);
    }
    PQclear(res_set);
    fprintf(stderr, "    Connected to db\n");
    return 0;
}

/*
 * Disconnect from Postgres DB
 */
void PgsqlDisconnect(void)
{
    PQfinish(conn);
    if (verbose) fprintf(stderr, "    Disconnected from db\n");
}

#endif
/*  EOF */
